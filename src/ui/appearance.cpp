#include "appearance.h"
#include <QApplication>
#include <QMap>
#include <QStyle>
#include <QStyleFactory>
#include <cmath>
namespace aeris {
QColor blend(const QColor &a, const QColor &b, double amount) {
    return QColor::fromRgbF(a.redF() * (1 - amount) + b.redF() * amount,
                            a.greenF() * (1 - amount) + b.greenF() * amount,
                            a.blueF() * (1 - amount) + b.blueF() * amount);
}
static double linear(double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}
static double luminance(const QColor &c) {
    return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
}
double contrast(const QColor &a, const QColor &b) {
    auto x = luminance(a), y = luminance(b);
    return (qMax(x, y) + .05) / (qMin(x, y) + .05);
}
QColor readableText(QColor text, const QColor &surface) {
    if (contrast(text, surface) >= 4.5)
        return text;
    return contrast(Qt::white, surface) > contrast(Qt::black, surface) ? Qt::white : Qt::black;
}
QStringList themeNames() {
    return {"System", "Light", "Dark", "Midnight", "Ocean", "Forest", "Violet", "Rose", "Paper"};
}
static QPalette seeded(const QString &name, QPalette palette) {
    const QMap<QString, QStringList> colors{
        {"Light", {"#f4f6f9", "#ffffff", "#172332", "#526174", "#1767c5"}},
        {"Dark", {"#191d24", "#222832", "#edf1f7", "#a9b4c5", "#89b8ff"}},
        {"Midnight", {"#0c111c", "#121b2a", "#e8effb", "#a1b2cc", "#70d8ed"}},
        {"Ocean", {"#102737", "#183344", "#e5f6ff", "#a6c4d4", "#7bdfeb"}},
        {"Forest", {"#142a25", "#1e3931", "#e7f5ed", "#aecbbb", "#8fe1b4"}},
        {"Violet", {"#241e34", "#302842", "#f3edff", "#c2b4d8", "#ccb3ff"}},
        {"Rose", {"#f8f0f3", "#fffafd", "#432438", "#745166", "#a52b68"}},
        {"Paper", {"#f2eddf", "#fcf9ef", "#3b3528", "#6c604b", "#7c5d29"}}};
    if (!colors.contains(name))
        return palette;
    const auto c = colors.value(name);
    const QList<QPalette::ColorRole> roles{QPalette::Window, QPalette::Base, QPalette::Text,
                                           QPalette::PlaceholderText, QPalette::Highlight};
    for (int i = 0; i < roles.size(); ++i)
        palette.setColor(roles[i], QColor(c[i]));
    return palette;
}
QPalette themePalette(const QString &name, const QPalette &system) {
    auto p = seeded(name, system);
    const auto base = p.color(QPalette::Base), window = p.color(QPalette::Window);
    const auto text = readableText(p.color(QPalette::Text), base);
    const auto muted = readableText(p.color(QPalette::PlaceholderText), base);
    const auto accent = p.color(QPalette::Highlight);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::WindowText, readableText(text, window));
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Button, base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::AlternateBase, blend(base, text, .035));
    p.setColor(QPalette::Mid, blend(base, text, .17));
    p.setColor(QPalette::Light, base.lighter(112));
    p.setColor(QPalette::Dark, base.darker(115));
    p.setColor(QPalette::HighlightedText, readableText(Qt::white, accent));
    p.setColor(QPalette::Link, readableText(accent, base));
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, muted);
    return p;
}
static QString controlStyles() {
    return QStringLiteral(
        "QLineEdit, QTextEdit, QTextBrowser {background:%1; color:%2; border:1px solid %3; "
        "border-radius:6px; selection-background-color:%4; selection-color:%5;}"
        "QLineEdit {padding:6px 9px;} QLineEdit:focus {border-color:%4;}"
        "QPushButton, QToolButton {color:%2; background:%1; border:1px solid %3; border-radius:6px; "
        "padding:5px 10px;}"
        "QPushButton:hover, QToolButton:hover {background:%6;}"
        "QPushButton:focus, QToolButton:focus {border-color:%4;}"
        "QPushButton:disabled, QToolButton:disabled {color:%7;}"
        "QPushButton#importButton {background:%4; color:%5; border-color:%4;}"
        "QPushButton#importButton:disabled {background:%6; color:%7; border-color:%3;}"
        "QPushButton#deleteAllButton {background:transparent; border:0; color:%7; padding:4px 0;}"
        "QPushButton#deleteAllButton:hover {color:%2;}"
        "QToolButton {padding:5px 7px;} QToolButton:checked {background:%6; border-color:%4;}"
        "QToolButton::menu-indicator {image:none;}"
        "QComboBox {background:%1; color:%2; border:1px solid %3; border-radius:6px; padding:5px 8px;}"
        "QComboBox QAbstractItemView {background:%1; color:%2; selection-background-color:%4; "
        "selection-color:%5;}"
        "QLabel#importStatus, QLabel#accountCount {color:%7;}"
        "QScrollBar:vertical {background:transparent; width:8px; margin:0;}"
        "QScrollBar::handle:vertical {background:%3; border-radius:4px; min-height:24px;}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {height:0;}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {background:transparent;}");
}
static QString chromeStyles() {
    return QStringLiteral("QMainWindow, QDialog {background:%8; color:%2;}"
                          "QListView#accounts {background:transparent; border:0; outline:0;}"
                          "QMenuBar {background:%8; color:%2; border:0;}"
                          "QMenuBar::item {background:transparent; padding:4px 8px;}"
                          "QMenuBar::item:selected {background:%6; border-radius:4px;}"
                          "QMenu {background:%1; color:%2; border:1px solid %3; padding:5px;}"
                          "QMenu::item {padding:5px 24px 5px 12px;}"
                          "QMenu::item:selected {background:%4; color:%5; border-radius:4px;}"
                          "QMenu::separator {height:1px; background:%3; margin:4px 8px;}"
                          "QToolTip {background:%1; color:%2; border:1px solid %3; padding:4px;}");
}
void applyTheme(const QString &name) {
    static const QPalette original = QApplication::palette();
    static const QString style = QApplication::style()->objectName();
    qApp->setStyleSheet({});
    const auto selectedStyle = name == "System" ? style : QStringLiteral("Fusion");
    if (QApplication::style()->objectName().compare(selectedStyle, Qt::CaseInsensitive))
        QApplication::setStyle(QStyleFactory::create(selectedStyle));
    const auto p = themePalette(name, original);
    QApplication::setPalette(p);
    auto css = (controlStyles() + chromeStyles())
                   .arg(p.color(QPalette::Base).name(), p.color(QPalette::Text).name(),
                        p.color(QPalette::Mid).name(), p.color(QPalette::Highlight).name(),
                        p.color(QPalette::HighlightedText).name(),
                        blend(p.color(QPalette::Base), p.color(QPalette::Highlight), .13).name(),
                        p.color(QPalette::PlaceholderText).name(), p.color(QPalette::Window).name());
    qApp->setStyleSheet(css);
}
} // namespace aeris
