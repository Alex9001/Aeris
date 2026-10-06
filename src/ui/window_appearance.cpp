#include "appearance.h"
#include "icons.h"
#include "window.h"
#include <QActionGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>
namespace aeris {
void Window::setupHeader(QVBoxLayout *layout) {
    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *title = new QLabel("Accounts");
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() + 7);
    font.setWeight(QFont::DemiBold);
    title->setFont(font);
    count_ = new QLabel("0");
    count_->setObjectName("accountCount");
    header->addWidget(title);
    header->addWidget(count_);
    header->addStretch();
    import_ = new QPushButton("Import…");
    import_->setObjectName("importButton");
    header->addWidget(import_);
    layout->addLayout(header);
    toolbar_ = new QHBoxLayout;
    toolbar_->setSpacing(4);
    search_ = new QLineEdit;
    search_->setObjectName("searchAccounts");
    search_->setPlaceholderText("Search accounts");
    search_->setAccessibleName("Search accounts");
    search_->setClearButtonEnabled(true);
    searchIcon_ = search_->addAction(QIcon{}, QLineEdit::LeadingPosition);
    searchIcon_->setEnabled(false);
    toolbar_->addWidget(search_, 1);
    toolbar_->addSpacing(6);
    layout->addLayout(toolbar_);
}
void Window::appearanceActions() {
    auto *view = menuBar()->addMenu("&View");
    auto *layouts = view->addMenu("Layout");
    auto *group = new QActionGroup(this);
    const QStringList names{"List", "Compact", "Cards"};
    for (int i = 0; i < names.size(); ++i) {
        auto *action = layouts->addAction(names[i]);
        action->setObjectName("layout." + names[i]);
        action->setCheckable(true);
        action->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(i + 1)));
        group->addAction(action);
        layoutActions_.append(action);
        connect(action, &QAction::triggered, this, [this, i] { setLayout(AccountLayout(i)); });
        auto *button = new QToolButton;
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAccessibleName(names[i] + " layout");
        button->setToolTip(names[i] + " layout (" + action->shortcut().toString(QKeySequence::NativeText) +
                           ")");
        toolbar_->addWidget(button);
    }
    auto *themes = view->addMenu("Theme");
    auto *themeGroup = new QActionGroup(this);
    for (const auto &name : themeNames()) {
        auto *action = themes->addAction(name);
        action->setObjectName("theme." + name);
        action->setCheckable(true);
        themeGroup->addAction(action);
        themeActions_.append(action);
        connect(action, &QAction::triggered, this, [this, name] { setTheme(name); });
    }
    themeButton_ = new QToolButton;
    themeButton_->setText("Theme");
    themeButton_->setAccessibleName("Choose theme");
    themeButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    themeButton_->setPopupMode(QToolButton::InstantPopup);
    themeButton_->setMenu(themes);
    toolbar_->addSpacing(6);
    toolbar_->addWidget(themeButton_);
    QSettings settings;
    setTheme(settings.value("appearance/theme", "System").toString());
    setLayout(AccountLayout(qBound(0, settings.value("appearance/layout", 0).toInt(), 2)));
}
void Window::setTheme(const QString &theme) {
    theme_ = themeNames().contains(theme) ? theme : QStringLiteral("System");
    applyTheme(theme_);
    QSettings().setValue("appearance/theme", theme_);
    for (auto *action : themeActions_)
        action->setChecked(action->text() == theme_);
    refreshIcons();
    list_->viewport()->update();
}
void Window::setLayout(AccountLayout layout) {
    list_->setAccountLayout(layout);
    QSettings().setValue("appearance/layout", int(layout));
    for (int i = 0; i < layoutActions_.size(); ++i)
        layoutActions_[i]->setChecked(i == int(layout));
}
void Window::refreshIcons() {
    const auto color = palette().color(QPalette::Text);
    const QList<Glyph> glyphs{Glyph::List, Glyph::Compact, Glyph::Cards};
    for (int i = 0; i < layoutActions_.size(); ++i)
        layoutActions_[i]->setIcon(actionIcon(glyphs[i], color));
    searchIcon_->setIcon(actionIcon(Glyph::Search, palette().color(QPalette::PlaceholderText)));
    themeButton_->setIcon(actionIcon(Glyph::Palette, color));
    themeButton_->setToolTip("Theme: " + theme_);
}
} // namespace aeris
