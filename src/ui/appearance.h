#pragma once
#include <QPalette>
#include <QStringList>
namespace aeris {
QStringList themeNames();
QPalette themePalette(const QString &name, const QPalette &system);
void applyTheme(const QString &name);
double contrast(const QColor &a, const QColor &b);
QColor readableText(QColor text, const QColor &surface);
QColor blend(const QColor &a, const QColor &b, double amount);
} // namespace aeris
