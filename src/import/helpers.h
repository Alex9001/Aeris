#pragma once
#include "importer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
namespace aeris::imports {
int defaultDigits(const QString &type);
QString string(const QJsonObject &o, const QString &field, const QString &fallback = {});
qint64 number(const QJsonObject &o, const QString &field, qint64 fallback);
QJsonObject object(const QJsonValue &v);
QJsonArray array(const QJsonValue &v);
QJsonDocument json(const QByteArray &data, qsizetype limit = Importer::MaxFile);
const QByteArray &password(const ImportOptions &options);
void append(ImportResult &result, QString type, QString issuer, QString account, const QString &encoded,
            QString algorithm, int digits, int period);
void uri(ImportResult &result, const QString &text, const QString &name = {});
ImportResult text(const QByteArray &data);
ImportResult jsonImport(const QJsonDocument &doc, const ImportOptions &options);
ImportResult aegis(const QJsonObject &o, const ImportOptions &options);
ImportResult twofas(const QJsonObject &o, const ImportOptions &options);
ImportResult ente(const QJsonObject &o, const ImportOptions &options);
ImportResult proton(const QJsonObject &o, const ImportOptions &options);
ImportResult bitwarden(const QJsonObject &o);
ImportResult csv(const QByteArray &data);
ImportResult records(const QJsonArray &a);
ImportResult andOtp(const QByteArray &data, const ImportOptions &options);
ImportResult raivoZip(const QByteArray &data, const ImportOptions &options);
QStringList qr(const QByteArray &data);
ImportResult google(const QStringList &links);
} // namespace aeris::imports
