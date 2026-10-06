#include "brands.h"
#include <QCache>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>
namespace aeris {
QString normalizedBrandName(const QString &name) {
    static const QRegularExpression separators(QStringLiteral("[\\s_-]+"),
                                               QRegularExpression::UseUnicodePropertiesOption);
    auto result = name.toCaseFolded();
    result.remove(separators);
    return result;
}
void BrandCatalog::add(const QString &name, const QString &id) {
    const auto key = normalizedBrandName(name);
    if (key.isEmpty())
        return;
    auto found = names_.find(key);
    if (found == names_.end())
        names_.insert(key, id);
    else if (*found != id)
        *found = QString(); // A collision stays ambiguous, regardless of insertion order.
}
BrandCatalog::BrandCatalog(const QJsonArray &icons) {
    for (const auto &value : icons) {
        const auto icon = value.toObject();
        const auto id = icon.value("id").toString();
        add(id, id);
        add(icon.value("name").toString(), id);
        for (const auto &alias : icon.value("aliases").toArray())
            add(alias.toString(), id);
    }
}
QString BrandCatalog::match(const QString &issuer) const {
    return names_.value(normalizedBrandName(issuer));
}
QString brandIdentifier(const QString &issuer, bool steam) {
    static const BrandCatalog catalog([] {
        QFile file(":/brands/catalog.json");
        if (!file.open(QIODevice::ReadOnly))
            return QJsonArray();
        return QJsonDocument::fromJson(file.readAll()).object().value("icons").toArray();
    }());
    return catalog.match(steam ? QStringLiteral("steam") : issuer);
}
QPixmap brandPixmap(const QString &identifier, int size, qreal scale) {
    // GUI-thread only, like QPixmap and the account delegate. Cost is bytes.
    static QCache<QString, QPixmap> cache(8 * 1024 * 1024);
    static const QRegularExpression safeId(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    if (!safeId.match(identifier).hasMatch() || size <= 0 || !std::isfinite(scale) || scale <= 0)
        return {};
    const int pixels = qRound(qMin(1024.0, size * scale));
    const auto key = identifier + ':' + QString::number(size) + ':' + QString::number(scale, 'g', 17);
    if (const auto *cached = cache.object(key))
        return *cached;
    // QImage avoids QPixmap::load adding another copy to Qt's global pixmap cache.
    QImage source(":/brands/" + identifier + ".png");
    if (source.isNull())
        return {};
    auto result =
        QPixmap::fromImage(source.scaled(pixels, pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    result.setDevicePixelRatio(scale);
    cache.insert(key, new QPixmap(result), result.width() * result.height() * 4);
    return result;
}
} // namespace aeris
