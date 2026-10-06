#pragma once
#include <QColor>
#include <QString>
namespace aeris {
struct AccountIdentity {
    QString letters;
    QColor color;
    quint8 mark;
    QString brand;
};
AccountIdentity accountIdentity(const QString &issuer, const QString &account);
} // namespace aeris
