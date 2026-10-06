#include "vault.h"
#include "core/error.h"
#include "import/helpers.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
namespace aeris {
static const QString Snapshot = "vault.json", Journal = "transaction.json";
static QString idOf(const QJsonObject &o) {
    auto id = imports::string(o, "key");
    require(!QUuid(id).isNull(), "Malformed vault credential identifier.");
    return id;
}
static QJsonObject envelope(const QByteArray &bytes) {
    auto o = imports::json(bytes, 32 * 1024 * 1024).object();
    require(imports::number(o, "version", 0) == 1, "Unsupported encrypted vault version.");
    idOf(o);
    return o;
}
static QByteArray encode(const Entries &entries) {
    QJsonArray values;
    for (const auto &e : entries)
        values.append(QJsonObject{{"issuer", e->issuer},
                                  {"account", e->account},
                                  {"secret", QString::fromLatin1(e->key->bytes().toBase64())},
                                  {"algorithm", e->algorithm},
                                  {"digits", e->digits},
                                  {"period", e->period},
                                  {"steam", e->type == TokenType::Steam}});
    return QJsonDocument(QJsonObject{{"version", 1}, {"entries", values}}).toJson(QJsonDocument::Compact);
}
static Entries decode(const QByteArray &plain) {
    auto o = imports::json(plain).object();
    require(imports::number(o, "version", 0) == 1, "Unsupported vault content version.");
    Entries entries;
    for (const auto &v : imports::array(o.value("entries"))) {
        auto e = imports::object(v);
        auto d = imports::number(e, "digits", 0), p = imports::number(e, "period", 0);
        require(d <= 10 && p <= 86400, "Invalid stored OTP parameters.");
        entries.append(std::make_shared<Entry>(
            imports::string(e, "issuer"), imports::string(e, "account"),
            crypto::base64(imports::string(e, "secret")), imports::string(e, "algorithm"), int(d), int(p),
            e.value("steam").toBool() ? TokenType::Steam : TokenType::Totp));
    }
    require(!entries.isEmpty(), "Empty encrypted vault.");
    return entries;
}
VaultStore::VaultStore(std::shared_ptr<CredentialStore> c, std::shared_ptr<SnapshotFiles> f)
    : credentials_(std::move(c)), files_(std::move(f)) {}
QString VaultStore::activeId() {
    auto bytes = files_->read(Snapshot);
    if (bytes.isEmpty())
        return {};
    return idOf(envelope(bytes));
}
void VaultStore::journal(const QString &oldId, const QString &newId, bool deleting) {
    QJsonArray ids;
    if (!oldId.isEmpty())
        ids.append(oldId);
    if (!newId.isEmpty())
        ids.append(newId);
    files_->write(Journal, QJsonDocument(QJsonObject{{"version", 1}, {"keys", ids}, {"deleting", deleting}})
                               .toJson(QJsonDocument::Compact));
}
void VaultStore::retire(const QStringList &ids, const QString &keep) {
    for (const auto &id : ids)
        if (id != keep)
            credentials_->remove(id);
    files_->remove(Journal);
}
static QStringList transactionKeys(const QJsonObject &o) {
    require(imports::number(o, "version", 0) == 1, "Unsupported transaction version.");
    auto keys = imports::array(o.value("keys"));
    require(keys.size() <= 2, "Invalid transaction journal.");
    QStringList ids;
    for (const auto &key : keys) {
        require(key.isString() && !QUuid(key.toString()).isNull(), "Invalid transaction credential.");
        ids.append(key.toString());
    }
    require(o.value("deleting").isBool(), "Invalid transaction operation.");
    return ids;
}
void VaultStore::recover() {
    auto bytes = files_->read(Journal);
    if (bytes.isEmpty())
        return;
    auto o = imports::json(bytes).object();
    auto ids = transactionKeys(o);
    if (o.value("deleting").toBool()) {
        files_->remove(Snapshot);
        retire(ids, {});
    } else
        retire(ids, activeId());
}
VaultResult VaultStore::load() {
    recover();
    auto bytes = files_->read(Snapshot);
    if (bytes.isEmpty())
        return {};
    auto o = envelope(bytes);
    auto id = idOf(o);
    auto key = credentials_->read(id);
    auto plain =
        secret(crypto::open(crypto::base64(imports::string(o, "cipher")), key->bytes(),
                            crypto::base64(imports::string(o, "nonce")), ("Aeris/v1/" + id).toUtf8()));
    return {decode(plain->bytes()), {}};
}
VaultResult VaultStore::replace(const Entries &entries) {
    require(!entries.isEmpty() && entries.size() <= Importer::MaxEntries,
            "Cannot replace with an empty or oversized collection.");
    recover();
    auto old = activeId();
    auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto key = secret(crypto::random(32));
    auto nonce = crypto::random(12);
    auto plain = secret(encode(entries));
    require(plain->bytes().size() <= Importer::MaxFile, "Collection exceeds supported vault size.");
    auto cipher = crypto::seal(plain->bytes(), key->bytes(), nonce, ("Aeris/v1/" + id).toUtf8());
    auto bytes = QJsonDocument(QJsonObject{{"version", 1},
                                           {"key", id},
                                           {"nonce", QString::fromLatin1(nonce.toBase64())},
                                           {"cipher", QString::fromLatin1(cipher.toBase64())}})
                     .toJson(QJsonDocument::Compact);
    journal(old, id);
    QString warning;
    try {
        credentials_->write(id, key->bytes());
        auto staged = credentials_->read(id);
        require(staged && staged->bytes() == key->bytes(),
                "The system keyring did not retain the staged key. Existing accounts have been kept.");
        files_->write(Snapshot, bytes);
    } catch (...) {
        // A directory-sync failure may follow a successful atomic rename. Resolve by the on-disk pointer.
        if (activeId() != id) {
            try {
                recover();
            } catch (...) {
            }
            throw;
        }
        warning = "Replacement committed, but directory synchronization failed. Retry to verify the saved "
                  "collection.";
    }
    try {
        retire({old, id}, id);
    } catch (const Error &) {
        return {entries,
                "Replacement saved. Old credential cleanup is pending; unlock the keyring and retry."};
    }
    return {entries, warning};
}
void VaultStore::clear() {
    QStringList ids;
    auto pending = files_->read(Journal);
    if (!pending.isEmpty())
        ids = transactionKeys(imports::json(pending).object());
    auto snapshot = files_->read(Snapshot);
    if (!snapshot.isEmpty())
        ids.append(idOf(imports::json(snapshot, 32 * 1024 * 1024).object()));
    ids.removeDuplicates();
    require(ids.size() <= 2, "Unexpected vault transaction state.");
    files_->write(Journal, QJsonDocument(QJsonObject{{"version", 1},
                                                     {"keys", QJsonArray::fromStringList(ids)},
                                                     {"deleting", true}})
                               .toJson(QJsonDocument::Compact));
    files_->remove(Snapshot);
    retire(ids, {});
}
} // namespace aeris
