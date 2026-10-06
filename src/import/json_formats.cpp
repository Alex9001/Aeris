#include "helpers.h"
namespace aeris::imports {
static QByteArray aegisDecrypt(const QJsonObject &o, const ImportOptions &options) {
    auto h = object(o.value("header"));
    auto keySlots = array(h.value("slots"));
    require(keySlots.size() <= 8, "Too many Aegis key slots.");
    const auto &pw = password(options);
    for (const auto &v : keySlots) {
        auto slot = object(v);
        if (number(slot, "type", -1) != 1)
            continue;
        auto key = crypto::scrypt(pw, crypto::hex(string(slot, "salt")), quint64(number(slot, "n", 0)),
                                  quint64(number(slot, "r", 0)), quint64(number(slot, "p", 0)));
        auto kp = object(slot.value("key_params"));
        SecretPtr master;
        try {
            master = secret(crypto::open(crypto::hex(string(slot, "key")) + crypto::hex(string(kp, "tag")),
                                         key->bytes(), crypto::hex(string(kp, "nonce"))));
        } catch (const Error &) {
            continue;
        }
        auto params = object(h.value("params"));
        return crypto::open(crypto::base64(string(o, "db")) + crypto::hex(string(params, "tag")),
                            master->bytes(), crypto::hex(string(params, "nonce")));
    }
    throw Error("Wrong password, damaged vault, or no password slot in Aegis export.");
}
ImportResult aegis(const QJsonObject &o, const ImportOptions &options) {
    require(number(o, "version", -1) == 1, "Unsupported Aegis format version.");
    QJsonObject db;
    if (o.value("db").isString()) {
        auto plain = secret(aegisDecrypt(o, options));
        db = json(plain->bytes()).object();
    } else
        db = object(o.value("db"));
    require(number(db, "version", -1) >= 1 && number(db, "version", -1) <= 3,
            "Unsupported Aegis database version.");
    ImportResult r;
    r.source = "Aegis";
    for (const auto &v : array(db.value("entries"))) {
        auto e = object(v);
        auto info = object(e.value("info"));
        auto type = string(e, "type");
        auto d = number(info, "digits", defaultDigits(type)), p = number(info, "period", 30);
        require(d <= 10 && p <= 86400, "OTP parameters exceed supported limits.");
        append(r, type, string(e, "issuer"), string(e, "name"), string(info, "secret"),
               string(info, "algo", "SHA1"), int(d), int(p));
    }
    return r;
}
ImportResult twofas(const QJsonObject &o, const ImportOptions &options) {
    auto version = number(o, "schemaVersion", 0);
    require(version >= 1 && version <= 4, "Unsupported 2FAS schema version.");
    QJsonArray services;
    if (o.value("servicesEncrypted").isString()) {
        auto parts = string(o, "servicesEncrypted").split(':');
        require(parts.size() == 3, "Malformed encrypted 2FAS export.");
        auto key = crypto::pbkdf(password(options), crypto::base64(parts[1]), 10000, "SHA256");
        auto plain = secret(crypto::open(crypto::base64(parts[0]), key->bytes(), crypto::base64(parts[2])));
        auto doc = json(plain->bytes());
        require(doc.isArray(), "Malformed 2FAS account array.");
        services = doc.array();
    } else
        services = array(o.value("services"));
    ImportResult r;
    r.source = "2FAS";
    for (const auto &v : services) {
        auto e = object(v);
        auto otp = object(e.value("otp"));
        auto issuer = string(e, "name", string(otp, "issuer"));
        if (issuer.isEmpty())
            issuer = string(otp, "issuer");
        auto type = string(otp, "tokenType", "TOTP");
        auto d = number(otp, "digits", defaultDigits(type)), p = number(otp, "period", 30);
        require(d <= 10 && p <= 86400, "OTP parameters exceed supported limits.");
        append(r, type, issuer, string(otp, "account"), string(e, "secret"), string(otp, "algorithm", "SHA1"),
               int(d), int(p));
    }
    return r;
}
ImportResult ente(const QJsonObject &o, const ImportOptions &options) {
    require(number(o, "version", 0) == 1, "Unsupported Ente export version.");
    auto k = object(o.value("kdfParams"));
    auto key = crypto::argon(password(options), crypto::base64(string(k, "salt")),
                             quint64(number(k, "opsLimit", 0)), quint64(number(k, "memLimit", 0)));
    auto plain = secret(crypto::enteOpen(crypto::base64(string(o, "encryptedData")), key->bytes(),
                                         crypto::base64(string(o, "encryptionNonce"))));
    auto r = text(plain->bytes());
    r.source = "Ente Auth";
    return r;
}
ImportResult proton(const QJsonObject &o, const ImportOptions &options) {
    require(number(o, "version", -1) == 1, "Unsupported Proton export version.");
    auto root = o;
    if (o.contains("salt")) {
        auto key = crypto::argon(password(options), crypto::base64(string(o, "salt")), 2, 19 * 1024 * 1024);
        auto content = crypto::base64(string(o, "content"));
        require(content.size() >= 28, "Truncated Proton export.");
        auto plain = secret(
            crypto::open(content.mid(12), key->bytes(), content.left(12), "proton.authenticator.export.v1"));
        root = json(plain->bytes()).object();
        require(number(root, "version", -1) == 1, "Unsupported Proton content version.");
    }
    ImportResult r;
    r.source = "Proton Authenticator";
    for (const auto &v : array(root.value("entries"))) {
        auto content = object(object(v).value("content"));
        uri(r, string(content, "uri"), content.value("name").isNull() ? QString{} : string(content, "name"));
    }
    return r;
}
static void bitwardenEntry(ImportResult &r, const QJsonObject &e) {
    auto login = object(e.value("login"));
    require(!login.contains("password") && !login.contains("uris"),
            "Password-manager vaults are outside this release. Use a Bitwarden Authenticator export.");
    auto value = string(login, "totp");
    require(!value.isEmpty(), "Missing OTP in Bitwarden Authenticator export.");
    if (value.contains("://"))
        uri(r, value);
    else
        append(r, "totp", string(e, "name"), string(login, "username"), value, "SHA1", 6, 30);
}
ImportResult bitwarden(const QJsonObject &o) {
    require(o.value("encrypted").isBool() && !o.value("encrypted").toBool(),
            "Encrypted Bitwarden vaults are outside this release.");
    ImportResult r;
    r.source = "Bitwarden Authenticator";
    for (const auto &v : array(o.value("items")))
        bitwardenEntry(r, object(v));
    return r;
}
static void record(ImportResult &r, const QJsonObject &e, bool raivo) {
    auto issuer = string(e, "issuer"), account = string(e, raivo ? "account" : "label");
    if (!raivo && !e.contains("issuer") && account.contains(" - ")) {
        issuer = account.section(" - ", 0, 0);
        account = account.section(" - ", 1);
    }
    auto type = string(e, raivo ? "kind" : "type");
    auto d = number(e, "digits", defaultDigits(type)), p = number(e, raivo ? "timer" : "period", 30);
    require(d <= 10 && p <= 86400, "OTP parameters exceed supported limits.");
    append(r, type, issuer, account, string(e, "secret"), string(e, "algorithm", "SHA1"), int(d), int(p));
}
ImportResult records(const QJsonArray &a) {
    require(!a.isEmpty(), "The export contains no accounts.");
    bool raivo = object(a.first()).contains("kind");
    ImportResult r;
    r.source = raivo ? "Raivo OTP" : "andOTP";
    for (const auto &v : a)
        record(r, object(v), raivo);
    return r;
}
ImportResult jsonImport(const QJsonDocument &doc, const ImportOptions &options) {
    if (doc.isArray())
        return records(doc.array());
    auto o = doc.object();
    if (o.contains("db"))
        return aegis(o, options);
    if (o.contains("schemaVersion"))
        return twofas(o, options);
    if (o.contains("kdfParams"))
        return ente(o, options);
    if (o.contains("entries") || o.contains("salt"))
        return proton(o, options);
    if (o.contains("items"))
        return bitwarden(o);
    throw Error("Unrecognized JSON export format.");
}
} // namespace aeris::imports
