#pragma once
#include <QHash>
#include <QJsonArray>
#include <QPixmap>
#include <QString>
namespace aeris {
QString normalizedBrandName(const QString &name);
class BrandCatalog {
  public:
    explicit BrandCatalog(const QJsonArray &icons);
    QString match(const QString &issuer) const;

  private:
    QHash<QString, QString> names_;
    void add(const QString &name, const QString &id);
};
QString brandIdentifier(const QString &issuer, bool steam = false);
QPixmap brandPixmap(const QString &identifier, int size, qreal scale);
} // namespace aeris
