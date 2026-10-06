#include "helpers.h"
#include <QStringDecoder>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>
namespace aeris::imports {
int defaultDigits(const QString &type) {
    return type.compare("steam", Qt::CaseInsensitive) == 0 ? 5 : 6;
}
QString string(const QJsonObject &o, const QString &field, const QString &fallback) {
    if (!o.contains(field))
        return fallback;
    require(o.value(field).isString(), "Malformed text field in export.");
    return o.value(field).toString();
}
qint64 number(const QJsonObject &o, const QString &field, qint64 fallback) {
    if (!o.contains(field))
        return fallback;
    auto v = o.value(field);
    bool ok = false;
    if (v.isString()) {
        auto n = v.toString().toLongLong(&ok);
        require(ok && n >= 0 && n <= 1099511627776LL, "Numeric field exceeds supported limits.");
        return n;
    }
    require(v.isDouble(), "Malformed numeric field.");
    double d = v.toDouble();
    require(std::isfinite(d) && d >= 0 && d <= 1099511627776.0 && std::floor(d) == d,
            "Numeric field exceeds supported limits.");
    return qint64(d);
}
QJsonObject object(const QJsonValue &v) {
    require(v.isObject(), "Expected an object in export.");
    return v.toObject();
}
QJsonArray array(const QJsonValue &v) {
    require(v.isArray(), "Expected an account array in export.");
    require(v.toArray().size() <= Importer::MaxEntries, "Too many accounts.");
    return v.toArray();
}
QJsonDocument json(const QByteArray &data, qsizetype limit) {
    require(data.size() <= limit, "JSON data exceeds the supported size.");
    QStringDecoder decoder(QStringDecoder::Utf8);
    decoder(data);
    require(!decoder.hasError(), "Export is not valid UTF-8.");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(data, &error);
    require(error.error == QJsonParseError::NoError, "Malformed JSON export.");
    return doc;
}
const QByteArray &password(const ImportOptions &options) {
    if (!options.password)
        throw Error("Enter the export password.", ErrorKind::PasswordRequired);
    require(options.password->bytes().size() <= 4096, "Password exceeds supported length.");
    return options.password->bytes();
}
void append(ImportResult &r, QString type, QString issuer, QString account, const QString &encoded,
            QString algorithm, int digits, int period) {
    require(r.entries.size() + r.unsupported.size() < Importer::MaxEntries, "Too many accounts.");
    require(issuer.size() <= 1024 && account.size() <= 1024 && type.size() <= 40,
            "Account metadata exceeds supported length.");
    type = type.toLower();
    algorithm = algorithm.toUpper();
    algorithm.remove('-');
    if (type != "totp" && type != "steam") {
        r.unsupported.append({issuer + " / " + account, type == "hotp"
                                                            ? "HOTP (counter-based)"
                                                            : "Unsupported token type: " + type.left(40)});
        return;
    }
    r.entries.append(std::make_shared<Entry>(std::move(issuer), std::move(account), crypto::base32(encoded),
                                             algorithm, digits, period,
                                             type == "steam" ? TokenType::Steam : TokenType::Totp));
}
static QJsonObject query(const QUrl &url) {
    QJsonObject o;
    const auto items = QUrlQuery(url).queryItems(QUrl::FullyEncoded);
    for (const auto &item : items) {
        auto key = QUrl::fromPercentEncoding(QString(item.first).replace('+', ' ').toUtf8());
        auto value = QUrl::fromPercentEncoding(QString(item.second).replace('+', ' ').toUtf8());
        require(!o.contains(key), "Duplicate parameter in OTP link.");
        o.insert(key, value);
    }
    return o;
}
void uri(ImportResult &r, const QString &text, const QString &name) {
    require(text.size() <= 8192, "OTP link exceeds supported size.");
    QUrl url(text, QUrl::StrictMode);
    require(url.isValid() && !url.hasFragment() && url.userInfo().isEmpty(), "Malformed OTP link.");
    if (url.scheme() == "steam") {
        append(r, "steam", "Steam", name, url.host(), "SHA1", 5, 30);
        return;
    }
    require(url.scheme() == "otpauth", "Unsupported link in export.");
    auto o = query(url);
    auto label = url.path(QUrl::FullyDecoded).mid(1);
    QString issuer = string(o, "issuer");
    QString account = label;
    auto colon = label.indexOf(':');
    if (colon >= 0) {
        auto prefix = label.left(colon);
        require(issuer.isEmpty() || issuer == prefix, "Conflicting issuers in OTP link.");
        issuer = prefix;
        account = label.mid(colon + 1);
    }
    if (!name.isEmpty())
        account = name;
    auto type = url.host();
    if (string(o, "encoder") == "steam")
        type = "steam";
    const auto d = number(o, "digits", defaultDigits(type));
    const auto p = number(o, "period", 30);
    require(d <= 10 && p <= 86400, "OTP parameters exceed supported limits.");
    append(r, type, issuer, account, string(o, "secret"), string(o, "algorithm", "SHA1"), int(d), int(p));
}
ImportResult text(const QByteArray &data) {
    ImportResult r;
    r.source = "OTP links / Ente Auth";
    QStringDecoder decoder(QStringDecoder::Utf8);
    auto decoded = QString(decoder(data));
    require(!decoder.hasError(), "Export is not valid UTF-8.");
    for (const auto &line : decoded.split('\n')) {
        auto s = line.trimmed();
        if (!s.isEmpty())
            uri(r, s);
    }
    return r;
}
} // namespace aeris::imports
