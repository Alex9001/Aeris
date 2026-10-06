#include "import/importer.h"
#include "storage/vault.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
using namespace aeris;
class MemoryCredentials final : public CredentialStore {
  public:
    QMap<QString, SecretPtr> values;
    bool unavailable = false, failRemove = false;
    SecretPtr read(const QString &id) override {
        if (unavailable || !values.contains(id))
            throw Error("Keyring unavailable.");
        return values[id];
    }
    void write(const QString &id, const QByteArray &key) override {
        if (unavailable)
            throw Error("Keyring unavailable.");
        values[id] = secret(key);
    }
    void remove(const QString &id) override {
        if (unavailable || failRemove)
            throw Error("Cleanup failed.");
        values.remove(id);
    }
};
class MemoryFiles final : public SnapshotFiles {
  public:
    QMap<QString, QByteArray> values;
    QString failWrite, failRemove;
    QByteArray read(const QString &name) override {
        return values.value(name);
    }
    void write(const QString &name, const QByteArray &data) override {
        if (name == failWrite)
            throw Error("Disk full.");
        values[name] = data;
    }
    void remove(const QString &name) override {
        if (name == failRemove)
            throw Error("Removal failed.");
        values.remove(name);
    }
};
struct Harness {
    std::shared_ptr<MemoryCredentials> keys = std::make_shared<MemoryCredentials>();
    std::shared_ptr<MemoryFiles> files = std::make_shared<MemoryFiles>();
    VaultStore store{keys, files};
    Entries a{std::make_shared<Entry>("issuer A", "account A", "secret A")};
    Entries b{std::make_shared<Entry>("issuer B", "account B", "secret B")};
};
class StorageTest final : public QObject {
    Q_OBJECT
  private slots:
    void roundtrip();
    void writeRollback();
    void keyringRollback();
    void journalFailure();
    void cleanupRecovery();
    void interruptedStage();
    void interruptedDelete();
    void deletionFailure();
    void deleteDuringPendingCleanup();
    void staleStaging();
    void diskIsEncrypted();
    void corruptSnapshot();
    void parseFailurePreserves();
};
void StorageTest::roundtrip() {
    Harness h;
    QVERIFY(h.store.load().entries.isEmpty());
    h.store.replace(h.a);
    auto original = h.keys->values.firstKey();
    QCOMPARE(h.store.load().entries.first()->issuer, QString("issuer A"));
    h.store.replace(h.b);
    QCOMPARE(h.keys->values.size(), 1);
    QVERIFY(h.keys->values.firstKey() != original);
    QCOMPARE(h.store.load().entries.first()->issuer, QString("issuer B"));
    h.store.clear();
    QVERIFY(h.store.load().entries.isEmpty());
    QVERIFY(h.keys->values.isEmpty());
    QVERIFY(h.files->values.isEmpty());
}
void StorageTest::writeRollback() {
    Harness h;
    h.store.replace(h.a);
    auto old = h.files->values["vault.json"];
    h.files->failWrite = "vault.json";
    QVERIFY_THROWS_EXCEPTION(Error, h.store.replace(h.b));
    QCOMPARE(h.files->values["vault.json"], old);
    QCOMPARE(h.keys->values.size(), 1);
    QCOMPARE(h.store.load().entries.first()->issuer, QString("issuer A"));
}
void StorageTest::keyringRollback() {
    Harness h;
    h.store.replace(h.a);
    auto old = h.files->values["vault.json"];
    h.keys->unavailable = true;
    QVERIFY_THROWS_EXCEPTION(Error, h.store.replace(h.b));
    QCOMPARE(h.files->values["vault.json"], old);
    QVERIFY_THROWS_EXCEPTION(Error, h.store.load());
    h.keys->unavailable = false;
    QCOMPARE(h.store.load().entries.first()->issuer, QString("issuer A"));
    QCOMPARE(h.keys->values.size(), 1);
}
void StorageTest::journalFailure() {
    Harness h;
    h.store.replace(h.a);
    h.files->failWrite = "transaction.json";
    QVERIFY_THROWS_EXCEPTION(Error, h.store.replace(h.b));
    QCOMPARE(h.keys->values.size(), 1);
    QCOMPARE(h.store.load().entries.first()->issuer, QString("issuer A"));
}
void StorageTest::cleanupRecovery() {
    Harness h;
    h.store.replace(h.a);
    h.keys->failRemove = true;
    auto r = h.store.replace(h.b);
    QVERIFY(!r.warning.isEmpty());
    QCOMPARE(h.keys->values.size(), 2);
    QVERIFY(h.files->values.contains("transaction.json"));
    h.keys->failRemove = false;
    VaultStore reopened(h.keys, h.files);
    QCOMPARE(reopened.load().entries.first()->issuer, QString("issuer B"));
    QCOMPARE(h.keys->values.size(), 1);
    QVERIFY(!h.files->values.contains("transaction.json"));
}
static QByteArray journalFor(const QStringList &ids, bool deleting) {
    return QJsonDocument(
               QJsonObject{{"version", 1}, {"keys", QJsonArray::fromStringList(ids)}, {"deleting", deleting}})
        .toJson();
}
void StorageTest::interruptedStage() {
    Harness h;
    h.store.replace(h.a);
    auto old = h.keys->values.firstKey();
    QString staged = "00000000-0000-4000-8000-000000000001";
    h.files->values["transaction.json"] = journalFor({old, staged}, false);
    h.keys->values[staged] = secret(crypto::random(32));
    VaultStore reopened(h.keys, h.files);
    QCOMPARE(reopened.load().entries.first()->issuer, QString("issuer A"));
    QCOMPARE(h.keys->values.size(), 1);
    QVERIFY(h.keys->values.contains(old));
}
void StorageTest::interruptedDelete() {
    Harness h;
    h.store.replace(h.a);
    h.files->values["transaction.json"] = journalFor({h.keys->values.firstKey()}, true);
    VaultStore reopened(h.keys, h.files);
    QVERIFY(reopened.load().entries.isEmpty());
    QVERIFY(h.keys->values.isEmpty());
    QVERIFY(h.files->values.isEmpty());
}
void StorageTest::deletionFailure() {
    Harness h;
    h.store.replace(h.a);
    h.keys->failRemove = true;
    QVERIFY_THROWS_EXCEPTION(Error, h.store.clear());
    QVERIFY(!h.files->values.contains("vault.json"));
    QVERIFY(h.files->values.contains("transaction.json"));
    h.keys->failRemove = false;
    QVERIFY(h.store.load().entries.isEmpty());
    QVERIFY(h.keys->values.isEmpty());
    QVERIFY(h.files->values.isEmpty());
}
void StorageTest::deleteDuringPendingCleanup() {
    Harness h;
    h.store.replace(h.a);
    h.keys->failRemove = true;
    QVERIFY(!h.store.replace(h.b).warning.isEmpty());
    QVERIFY_THROWS_EXCEPTION(Error, h.store.clear());
    QVERIFY(!h.files->values.contains("vault.json"));
    h.keys->failRemove = false;
    QVERIFY(h.store.load().entries.isEmpty());
    QVERIFY(h.keys->values.isEmpty());
    QVERIFY(h.files->values.isEmpty());
}
void StorageTest::staleStaging() {
    QTemporaryDir directory;
    QFile temporary(directory.filePath("vault.json.Abc123"));
    QVERIFY(temporary.open(QIODevice::WriteOnly));
    temporary.write("encrypted staging");
    temporary.close();
    QFile unrelated(directory.filePath("other.txt"));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.write("keep");
    unrelated.close();
    DiskFiles files(directory.path());
    QVERIFY(!temporary.exists());
    QVERIFY(unrelated.exists());
}
void StorageTest::diskIsEncrypted() {
    QTemporaryDir directory;
    auto files = std::make_shared<DiskFiles>(directory.path());
    auto keys = std::make_shared<MemoryCredentials>();
    VaultStore store(keys, files);
    Entries entries{std::make_shared<Entry>("PRIVATE ISSUER", "PRIVATE ACCOUNT", "NEVER_PLAINTEXT")};
    store.replace(entries);
    for (const auto &name : QDir(directory.path()).entryList(QDir::Files)) {
        QFile f(directory.filePath(name));
        QVERIFY(f.open(QIODevice::ReadOnly));
        auto bytes = f.readAll();
        QVERIFY(!bytes.contains("NEVER_PLAINTEXT"));
        QVERIFY(!bytes.contains("PRIVATE"));
        QVERIFY(!bytes.contains(QByteArray("NEVER_PLAINTEXT").toBase64()));
    }
    store.clear();
    QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
}
void StorageTest::corruptSnapshot() {
    Harness h;
    h.store.replace(h.a);
    auto o = QJsonDocument::fromJson(h.files->values["vault.json"]).object();
    auto s = o["cipher"].toString();
    s[0] = s[0] == 'A' ? 'B' : 'A';
    o["cipher"] = s;
    h.files->values["vault.json"] = QJsonDocument(o).toJson();
    QVERIFY_THROWS_EXCEPTION(Error, h.store.load());
    QCOMPARE(h.keys->values.size(), 1);
}
void StorageTest::parseFailurePreserves() {
    Harness h;
    h.store.replace(h.a);
    auto before = h.files->values;
    QVERIFY_THROWS_EXCEPTION(Error, Importer::parse({{"[]", false}}));
    QCOMPARE(h.files->values, before);
    QVERIFY_THROWS_EXCEPTION(Error, h.store.replace({}));
    QCOMPARE(h.files->values, before);
}
QTEST_GUILESS_MAIN(StorageTest)
#include "test_storage.moc"
