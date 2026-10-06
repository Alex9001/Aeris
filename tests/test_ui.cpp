#include "ui/accounts.h"
#include "ui/appearance.h"
#include "ui/brands.h"
#include "ui/clipboard.h"
#include "ui/identity.h"
#include "ui/window.h"
#include <QAction>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QtTest>
using namespace aeris;
class UiTest final : public QObject {
    Q_OBJECT
  private:
    QTemporaryDir preferences_;
  private slots:
    void initTestCase();
    void themes();
    void identities();
    void brands();
    void brandAssets();
    void reportedBrands_data();
    void reportedBrands();
    void brandFallback();
    void brandCache();
    void gallery();
    void presentation_data();
    void presentation();
    void copyAnimation();
    void appearancePersistence();
    void model();
    void filtering();
    void clipboardBoundary();
    void clipboardOwnership();
    void clipboardExpiry();
    void windowEmpty();
    void rendering_data();
    void rendering();
    void about();
    void workflow();
    void filePicker();
};
void UiTest::initTestCase() {
    QVERIFY(preferences_.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences_.path());
}
void UiTest::themes() {
    auto broken = QApplication::palette();
    broken.setColor(QPalette::Base, Qt::black);
    broken.setColor(QPalette::Text, Qt::black);
    for (const auto &name : themeNames()) {
        const auto p = themePalette(name, broken);
        QVERIFY2(contrast(p.color(QPalette::Text), p.color(QPalette::Base)) >= 4.5, qPrintable(name));
        QVERIFY(contrast(p.color(QPalette::PlaceholderText), p.color(QPalette::Base)) >= 4.5);
        QVERIFY(contrast(p.color(QPalette::HighlightedText), p.color(QPalette::Highlight)) >= 4.5);
        QVERIFY(contrast(p.color(QPalette::WindowText), p.color(QPalette::Window)) >= 4.5);
    }
}
void UiTest::identities() {
    const auto first = accountIdentity("Google", "work@example.test");
    const auto again = accountIdentity("Google", "work@example.test");
    const auto other = accountIdentity("Google", "personal@example.test");
    QCOMPARE(first.color, again.color);
    QCOMPARE(first.mark, again.mark);
    QCOMPARE(accountIdentity("GitHub", "alex").letters, QString("GH"));
    QVERIFY(contrast(first.color, Qt::white) >= 4.5);
    QVERIFY(first.color != other.color || first.mark != other.mark);
    QVERIFY(!accountIdentity("日本", "Unicode").letters.isEmpty());
}
void UiTest::brands() {
    QCOMPARE(normalizedBrandName(" GiT_hub- "), QString("github"));
    QCOMPARE(brandIdentifier("git HUB"), QString("github"));
    QCOMPARE(brandIdentifier("github.com"), QString("github"));
    QCOMPARE(brandIdentifier("AWS"), QString("amazon-web-services"));
    QCOMPARE(brandIdentifier("Amazon Web Services"), QString("amazon-web-services"));
    QCOMPARE(brandIdentifier("ProtonMail"), QString("proton-mail"));
    QCOMPARE(brandIdentifier("Proton"), QString("proton"));
    QVERIFY(brandIdentifier("My GitHub account").isEmpty());
    QVERIFY(brandIdentifier("github.com.evil.test").isEmpty());
    QVERIFY(brandIdentifier("Unknown service").isEmpty());
    QVERIFY(brandIdentifier("").isEmpty());
    QCOMPARE(brandIdentifier("Unrelated", true), QString("steam"));
    BrandCatalog ambiguous(QJsonArray{QJsonObject{{"id", "one"}, {"name", "same-name"}},
                                      QJsonObject{{"id", "two"}, {"aliases", QJsonArray{"same name"}}},
                                      QJsonObject{{"id", "three"}, {"name", "same_name"}}});
    QVERIFY(ambiguous.match("Same Name").isEmpty());
    QCOMPARE(ambiguous.match("one"), QString("one"));
    AccountModel model;
    model.replace({std::make_shared<Entry>("GitHub", "one", "synthetic"),
                   std::make_shared<Entry>("GitHub", "two", "synthetic"),
                   std::make_shared<Entry>("", "user@github.com", "synthetic"),
                   std::make_shared<Entry>("Other", "three", "synthetic", "SHA1", 5, 30, TokenType::Steam)});
    QCOMPARE(model.index(0).data(AccountModel::BrandIdentifier).toString(), QString("github"));
    QCOMPARE(model.index(1).data(AccountModel::BrandIdentifier),
             model.index(0).data(AccountModel::BrandIdentifier));
    QVERIFY(model.index(1).data(AccountModel::IconColor) != model.index(0).data(AccountModel::IconColor));
    QVERIFY(model.index(2).data(AccountModel::BrandIdentifier).toString().isEmpty());
    QCOMPARE(model.index(3).data(AccountModel::BrandIdentifier).toString(), QString("steam"));
    QVERIFY(brandPixmap("missing-artwork", 36, 1).isNull());
    QVERIFY(brandPixmap("catalog", 36, 1).isNull());
    const auto normal = brandPixmap("github", 28, 1);
    const auto retina = brandPixmap("github", 28, 2);
    QCOMPARE(normal.devicePixelRatio(), 1.0);
    QCOMPARE(retina.devicePixelRatio(), 2.0);
    QCOMPARE(retina.size(), normal.size() * 2);
    QCOMPARE(brandPixmap("github", 28, 1).cacheKey(), normal.cacheKey());
}
void UiTest::reportedBrands_data() {
    QTest::addColumn<QString>("issuer");
    QTest::addColumn<QString>("identifier");
    const QList<QPair<QString, QString>> cases{{"wordpress.org", "wordpress"},
                                               {"monogodb", "mongodb"},
                                               {"wordpress.com", "wordpress"},
                                               {"godaddy", "godaddy"},
                                               {"patelco", "patelco"},
                                               {"fidelity", "fidelity"},
                                               {"gog", "gog"},
                                               {"chase", "chase"},
                                               {"dreamhost", "dreamhost"},
                                               {"rocket-mortgage", "rocket-mortgage"},
                                               {"rocket-money", "rocket-money"},
                                               {"spaceship.com", "spaceship"},
                                               {"zoho", "zoho"},
                                               {"intuit", "intuit"},
                                               {"rockstar", "rockstar"},
                                               {"ubisoft", "ubisoft"},
                                               {"tastyworks", "tastytrade"},
                                               {"mysonicwall", "sonicwall"},
                                               {"id.me", "id-me"},
                                               {"dwservice.net", "dwservice"},
                                               {"MongoDB", "mongodb"},
                                               {"MongoDB Atlas", "mongodb"},
                                               {"Fidelity Investments", "fidelity"},
                                               {"fidelity.com", "fidelity"},
                                               {"Chase Bank", "chase"},
                                               {"chase.com", "chase"},
                                               {"Patelco Credit Union", "patelco"},
                                               {"patelco.org", "patelco"},
                                               {"Rocket Money", "rocket-money"},
                                               {"rocketmoney.com", "rocket-money"},
                                               {"rocketmortgage.com", "rocket-mortgage"},
                                               {"Rockstar Games", "rockstar"},
                                               {"rockstargames.com", "rockstar"},
                                               {"tastytrade", "tastytrade"},
                                               {"tastyworks.com", "tastytrade"},
                                               {"mysonicwall.com", "sonicwall"},
                                               {"SonicWall", "sonicwall"},
                                               {"DWService", "dwservice"},
                                               {"Zoho Mail", "zoho-mail"},
                                               {"Gogs", "gogs"}};
    for (const auto &item : cases)
        QTest::newRow(qPrintable(item.first)) << item.first << item.second;
}
void UiTest::reportedBrands() {
    QFETCH(QString, issuer);
    QFETCH(QString, identifier);
    QCOMPARE(brandIdentifier(issuer), identifier);
    QCOMPARE(brandIdentifier("  " + issuer.toUpper() + "  "), identifier);
    QVERIFY(brandIdentifier(issuer + ".unrelated.test").isEmpty());
    QVERIFY(brandIdentifier("my " + issuer + " account").isEmpty());
    QVERIFY(!brandPixmap(identifier, 28, 1).isNull());
    QVERIFY(!brandPixmap(identifier, 36, 2).isNull());
    AccountModel model;
    model.replace({std::make_shared<Entry>(issuer, "synthetic", "synthetic"),
                   std::make_shared<Entry>("", "user@" + issuer, "synthetic")});
    QCOMPARE(model.index(0).data(AccountModel::BrandIdentifier).toString(), identifier);
    QVERIFY(model.index(1).data(AccountModel::BrandIdentifier).toString().isEmpty());
}
void UiTest::brandCache() {
    // Three 4 MiB renders exceed the cache budget; the oldest must be evicted.
    const auto first = brandPixmap("github", 1024, 1);
    QCOMPARE(first.size(), QSize(1024, 1024));
    QCOMPARE(brandPixmap("github", 1024, 1).cacheKey(), first.cacheKey());
    QCOMPARE(brandPixmap("google", 1024, 1).size(), QSize(1024, 1024));
    QCOMPARE(brandPixmap("microsoft", 1024, 1).size(), QSize(1024, 1024));
    QVERIFY(brandPixmap("github", 1024, 1).cacheKey() != first.cacheKey());
    QVERIFY(brandPixmap("github", 28, 1).cacheKey() != brandPixmap("github", 36, 1).cacheKey());
}
void UiTest::brandFallback() {
    QStandardItemModel model(1, 1);
    auto index = model.index(0, 0);
    model.setData(index, QColor("#306080"), AccountModel::IconColor);
    model.setData(index, "AB", AccountModel::IconLetters);
    AccountDelegate delegate;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 600, 74);
    option.font = QApplication::font();
    option.fontMetrics = QFontMetrics(option.font);
    auto render = [&](const QString &id) {
        model.setData(index, id, AccountModel::BrandIdentifier);
        QImage image(600, 74, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        delegate.paint(&painter, option, index);
        return image;
    };
    const auto badge = render("");
    QCOMPARE(render("missing-artwork"), badge);
    QCOMPARE(render("catalog"), badge); // Existing JSON resource is not readable artwork.
    QVERIFY(render("github") != badge);
}
void UiTest::brandAssets() {
    QFile catalog(":/brands/catalog.json");
    QVERIFY(catalog.open(QIODevice::ReadOnly));
    const auto icons = QJsonDocument::fromJson(catalog.readAll()).object().value("icons").toArray();
    QCOMPARE(icons.size(), 2964);
    QSet<QString> identifiers;
    for (const auto &value : icons) {
        const auto row = value.toObject();
        const auto id = row.value("id").toString();
        QVERIFY(!identifiers.contains(id));
        identifiers.insert(id);
        QFile file(":/brands/" + id + ".png");
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(id));
        const auto data = file.readAll();
        QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()),
                 row.value("sha256").toString());
        const auto image = QImage::fromData(data, "PNG");
        QVERIFY2(!image.isNull(), qPrintable(id));
        QVERIFY(image.hasAlphaChannel());
        QVERIFY(image.width() <= 128 && image.height() <= 128);
        QVERIFY2(!brandPixmap(id, 28, 2).isNull(), qPrintable(id));
    }
}
void UiTest::model() {
    qint64 now = 59;
    AccountModel model(nullptr, [&now] { return now; });
    model.replace({std::make_shared<Entry>("Issuer", "account", "12345678901234567890")});
    QCOMPARE(model.rowCount(), 1);
    auto i = model.index(0, 0);
    auto before = i.data(AccountModel::CodeText).toString();
    now = 60;
    QVERIFY(i.data(AccountModel::CodeText).toString() != before);
    QCOMPARE(i.data(AccountModel::Remaining).toInt(), 30);
    QVERIFY(i.data(Qt::AccessibleTextRole).toString().contains("Press Enter"));
    model.replace({});
    QCOMPARE(model.rowCount(), 0);
}
void UiTest::filtering() {
    Entries entries;
    for (int i = 0; i < 1000; ++i)
        entries.append(std::make_shared<Entry>("Étoile " + QString::number(i), "account 日本", "synthetic"));
    AccountModel model;
    model.replace(entries);
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setFilterRole(AccountModel::Search);
    proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
    QElapsedTimer timer;
    timer.start();
    proxy.setFilterFixedString("étoile 999");
    QCOMPARE(proxy.rowCount(), 1);
    auto nanos = timer.nsecsElapsed();
    qInfo("Filtering 1000 entries: %.3f ms", double(nanos) / 1000000.0);
    QVERIFY(nanos < 50000000);
}
void UiTest::clipboardBoundary() {
    qint64 now = 59;
    CodeClipboard clipboard([&now] { return now; });
    Entry e("", "", "12345678901234567890");
    clipboard.copy(e);
    auto old = QApplication::clipboard()->text();
    now = 60;
    clipboard.copy(e);
    QVERIFY(QApplication::clipboard()->text() != old);
    QCOMPARE(QApplication::clipboard()->text(), OtpEngine::at(e, 60).text);
    clipboard.clear();
    QVERIFY(QApplication::clipboard()->text().isEmpty());
}
void UiTest::clipboardOwnership() {
    CodeClipboard clipboard;
    Entry e("", "", "12345678901234567890");
    clipboard.copy(e);
    QVERIFY(clipboard.owns());
    auto code = QApplication::clipboard()->text();
    QApplication::clipboard()->setText(code);
    QVERIFY(!clipboard.owns());
    clipboard.clear();
    QCOMPARE(QApplication::clipboard()->text(), code);
    clipboard.copy(e);
    QApplication::clipboard()->setText("user content");
    clipboard.clear();
    QCOMPARE(QApplication::clipboard()->text(), QString("user content"));
}
void UiTest::clipboardExpiry() {
    qint64 milliseconds = 0;
    CodeClipboard clipboard({}, [&milliseconds] { return milliseconds; });
    Entry e("", "", "synthetic");
    clipboard.copy(e);
    milliseconds = 29999;
    clipboard.tick();
    QVERIFY(clipboard.owns());
    milliseconds = 30000;
    clipboard.tick();
    QVERIFY(QApplication::clipboard()->text().isEmpty());
    clipboard.copy(e);
    QApplication::clipboard()->setText("user content");
    milliseconds = 60000;
    clipboard.tick();
    QCOMPARE(QApplication::clipboard()->text(), QString("user content"));
}
class EmptyKeys final : public CredentialStore {
  public:
    SecretPtr read(const QString &) override {
        throw Error("Unavailable");
    }
    void write(const QString &, const QByteArray &) override {
        throw Error("Unavailable");
    }
    void remove(const QString &) override {
        throw Error("Unavailable");
    }
};
void UiTest::windowEmpty() {
    QTemporaryDir directory;
    Window w([&directory] {
        return std::make_shared<VaultStore>(std::make_shared<EmptyKeys>(),
                                            std::make_shared<DiskFiles>(directory.path()));
    });
    w.show();
    w.load();
    auto *search = w.findChild<QLineEdit *>();
    QVERIFY(search);
    QTRY_VERIFY(search->isEnabled());
    QTest::keyClick(&w, Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(search->hasFocus());
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
        w.grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/empty.png");
    w.close();
}
void UiTest::rendering_data() {
    QTest::addColumn<bool>("dark");
    QTest::newRow("light") << false;
    QTest::newRow("dark") << true;
}
void UiTest::rendering() {
    QFETCH(bool, dark);
    AccountModel model(nullptr, [] { return 59; });
    model.replace({std::make_shared<Entry>(QString(120, 'W'), "long account 日本 · " + QString(160, 'm'),
                                           "12345678901234567890")});
    QListView view;
    view.setModel(&model);
    view.setItemDelegate(new AccountDelegate(&view));
    applyTheme(dark ? "Dark" : "Light");
    view.resize(600, 220);
    view.show();
    QTest::qWait(30);
    auto image = view.grab().toImage();
    QVERIFY(!image.isNull());
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
        image.save(qEnvironmentVariable("AERIS_QA_DIR") +
                   (dark ? "/accounts-dark.png" : "/accounts-light.png"));
}
class UiKeys final : public CredentialStore {
  public:
    QMap<QString, SecretPtr> keys;
    SecretPtr read(const QString &id) override {
        return keys.value(id);
    }
    void write(const QString &id, const QByteArray &key) override {
        keys[id] = secret(key);
    }
    void remove(const QString &id) override {
        keys.remove(id);
    }
};
static Entries previewEntries() {
    Entries entries;
    const QList<QPair<QString, QString>> accounts{
        {"Google", "user@example.com"},     {"GitHub", "user@example.com"},
        {"Microsoft", "user@example.com"},  {"Proton", "user@example.com"},
        {"Steam", "user@example.com"},      {"Google", "user@example.com"},
        {"Cloudflare", "user@example.com"}, {"My server", "user@example.com"}};
    for (const auto &account : accounts) {
        const auto type = account.first == "Steam" ? TokenType::Steam : TokenType::Totp;
        const auto key = "synthetic preview key " + account.first.toUtf8() + account.second.toUtf8();
        entries.append(std::make_shared<Entry>(account.first, account.second, key, "SHA1",
                                               type == TokenType::Steam ? 5 : 6, 30, type));
    }
    return entries;
}
static void chooseAction(Window &window, const QString &name) {
    auto *action = window.findChild<QAction *>(name);
    QVERIFY2(action, qPrintable(name));
    action->trigger();
}
static Entries galleryEntries() {
    Entries entries;
    for (const QString &issuer : {"Google", "GitHub", "Microsoft", "Proton", "Steam", "Cloudflare",
                                  "WordPress", "MongoDB", "Bitwarden"}) {
        const bool steam = issuer == "Steam";
        entries.append(
            std::make_shared<Entry>(issuer, "user@example.com", "synthetic gallery key " + issuer.toUtf8(),
                                    "SHA1", steam ? 5 : 6, 30, steam ? TokenType::Steam : TokenType::Totp));
    }
    return entries;
}
static void captureGalleryWindow(Window &window, const QString &path) {
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QTest::qWait(200);
    const auto frame = window.frameGeometry();
    QVERIFY2(frame.height() > window.height(), "A window manager with native decorations is required.");
    const auto image = window.screen()->grabWindow(0, frame.x(), frame.y(), frame.width(), frame.height());
    QVERIFY2(image.width() >= 2000, "Capture at 2x display scale, without image upscaling.");
    QVERIFY(image.save(path));
}
static void captureScene(const std::shared_ptr<VaultStore> &store, const QString &theme,
                         const QString &layout, const QString &output) {
    Window window([store] { return store; });
    chooseAction(window, "theme." + theme);
    chooseAction(window, "layout." + layout);
    window.resize(1000, layout == "List" ? 820 : 660);
    window.move(100, 70);
    window.show();
    QSignalSpy ready(&window, &Window::collectionReady);
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    // Apply after the native window is exposed so palette-derived toolbar icons refresh too.
    chooseAction(window, "theme." + theme);
    window.findChild<AccountView *>()->clearSelection();
    window.findChild<QLineEdit *>("searchAccounts")->setFocus();
    captureGalleryWindow(window, output + "/" + theme.toLower() + "-" + layout.toLower() + ".png");
}
void UiTest::gallery() {
    const auto output = qEnvironmentVariable("AERIS_SCREENSHOT_DIR");
    if (output.isEmpty())
        QSKIP("Set AERIS_SCREENSHOT_DIR under a decorated X11 desktop to capture the gallery.");
    QVERIFY(QDir().mkpath(output));
    QTemporaryDir directory;
    auto store = std::make_shared<VaultStore>(std::make_shared<UiKeys>(),
                                              std::make_shared<DiskFiles>(directory.path()));
    store->replace(galleryEntries());
    const QList<QPair<QString, QString>> scenes{
        {"Light", "Compact"}, {"Light", "Cards"},    {"Dark", "Cards"},   {"Midnight", "List"},
        {"Ocean", "Cards"},   {"Forest", "Compact"}, {"Violet", "Cards"}, {"Rose", "List"},
        {"Paper", "Compact"}, {"System", "List"}};
    const auto selected = qEnvironmentVariable("AERIS_SCREENSHOT_SCENE");
    for (const auto &[theme, layout] : scenes) {
        if (selected.isEmpty() || selected == theme + "-" + layout)
            captureScene(store, theme, layout, output);
    }
}
void UiTest::presentation_data() {
    QTest::addColumn<QString>("theme");
    QTest::addColumn<QString>("layout");
    for (const auto &theme : themeNames())
        for (const auto &layout : {"List", "Compact", "Cards"})
            QTest::newRow(qPrintable(theme + "-" + layout)) << theme << QString(layout);
}
void UiTest::presentation() {
    QFETCH(QString, theme);
    QFETCH(QString, layout);
    QTemporaryDir directory;
    auto store = std::make_shared<VaultStore>(std::make_shared<UiKeys>(),
                                              std::make_shared<DiskFiles>(directory.path()));
    store->replace(previewEntries());
    Window window([store] { return store; });
    window.show();
    QSignalSpy ready(&window, &Window::collectionReady);
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    chooseAction(window, "theme." + theme);
    chooseAction(window, "layout." + layout);
    auto *search = window.findChild<QLineEdit *>("searchAccounts");
    search->setText("google");
    QTRY_COMPARE(window.findChild<AccountView *>()->model()->rowCount(), 2);
    QTest::qWait(20);
    QVERIFY(contrast(search->palette().color(QPalette::Text), search->palette().color(QPalette::Base)) >=
            4.5);
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR") && layout == "List")
        window.grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/search-" + theme + ".png");
    search->clear();
    auto *preview = window.findChild<AccountView *>();
    preview->setCurrentIndex(preview->model()->index(0, 0));
    static_cast<AccountDelegate *>(preview->itemDelegate())->setFeedback(preview->model()->index(1, 0), .2);
    preview->viewport()->update();
    QTest::qWait(20);
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
        window.grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/" + theme + "-" + layout + ".png");
    if (layout == "Cards") {
        auto *v = window.findChild<AccountView *>();
        const auto first = v->visualRect(v->model()->index(0, 0));
        const auto second = v->visualRect(v->model()->index(1, 0));
        QCOMPARE(first.y(), second.y());
        QVERIFY(second.x() > first.x());
    }
    window.resize(480, 360);
    QTest::qWait(20);
    auto *list = window.findChild<AccountView *>();
    QVERIFY(list->visualRect(list->model()->index(0, 0)).right() <= list->viewport()->width());
}
void UiTest::copyAnimation() {
    QTemporaryDir directory;
    auto store = std::make_shared<VaultStore>(std::make_shared<UiKeys>(),
                                              std::make_shared<DiskFiles>(directory.path()));
    store->replace(previewEntries());
    Window window([store] { return store; });
    window.show();
    QSignalSpy ready(&window, &Window::collectionReady);
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    chooseAction(window, "theme.Dark");
    chooseAction(window, "layout.Cards");
    auto *view = window.findChild<AccountView *>();
    QTest::qWait(30);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect(view->model()->index(0, 0)).center());
    QVERIFY(view->copyFeedbackActive());
    QVERIFY(!QApplication::clipboard()->text().isEmpty());
    QTest::qWait(180);
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
        window.grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/copied.png");
    QTRY_VERIFY(!view->copyFeedbackActive());
    QTest::keyClick(view, Qt::Key_Return);
    QVERIFY(view->copyFeedbackActive());
}
void UiTest::appearancePersistence() {
    QSettings().setValue("appearance/theme", "Paper");
    QSettings().setValue("appearance/layout", 2);
    Window window([] { return std::shared_ptr<VaultStore>{}; });
    QVERIFY(window.findChild<QAction *>("theme.Paper")->isChecked());
    QCOMPARE(window.findChild<AccountView *>()->accountLayout(), AccountLayout::Cards);
    chooseAction(window, "theme.Dark");
    chooseAction(window, "layout.Compact");
    QCOMPARE(QSettings().value("appearance/theme").toString(), QString("Dark"));
    QCOMPARE(QSettings().value("appearance/layout").toInt(), 1);
    QSettings().clear();
}
static void dropFixture(Window &window, const QString &name) {
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QString(FIXTURE_DIR) + "/" + name)});
    QDragEnterEvent enter(QPoint(20, 120), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(20, 120), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&window, &drop);
}
void UiTest::filePicker() {
    QTemporaryDir directory;
    QFile sample(directory.filePath("example.json"));
    QVERIFY(sample.open(QIODevice::WriteOnly));
    sample.write("{}");
    sample.close();
    auto store = std::make_shared<VaultStore>(std::make_shared<UiKeys>(),
                                              std::make_shared<DiskFiles>(directory.path()));
    Window window([store] { return store; });
    QSignalSpy ready(&window, &Window::collectionReady);
    window.show();
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    int ticks = 0;
    bool listed = false;
    QTimer cancel;
    connect(&cancel, &QTimer::timeout, &window, [&] {
        auto *dialog = window.findChild<QFileDialog *>("exportFilePicker");
        if (!dialog)
            return;
        if (++ticks == 1)
            dialog->setDirectory(directory.path());
        auto *model = dialog->findChild<QFileSystemModel *>();
        listed = model && model->rowCount(model->index(directory.path())) > 0;
        if (ticks >= 10 && listed)
            dialog->reject();
    });
    cancel.start(50);
    window.findChild<QPushButton *>("importButton")->click();
    cancel.stop();
    QVERIFY(listed);
    QVERIFY(ticks >= 10);
    QVERIFY(window.findChild<QPushButton *>("importButton")->isEnabled());
    QVERIFY(store->load().entries.isEmpty());
}
static void answerReview(QTimer &timer, bool accept, bool &seen) {
    QObject::connect(&timer, &QTimer::timeout, &timer, [&timer, accept, &seen] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != "replacementReview")
            return;
        auto *exclusions = dialog->findChild<QTextBrowser *>();
        QVERIFY(exclusions);
        QVERIFY(exclusions->toPlainText().contains("HOTP"));
        QVERIFY(exclusions->toPlainText().contains("Counter account"));
        seen = true;
        timer.stop();
        if (accept)
            dialog->findChild<QPushButton *>("confirmReplacement")->click();
        else
            dialog->reject();
    });
    timer.start(10);
}
void UiTest::workflow() {
    QTemporaryDir directory;
    auto keys = std::make_shared<UiKeys>();
    auto files = std::make_shared<DiskFiles>(directory.path());
    auto store = std::make_shared<VaultStore>(keys, files);
    store->replace({std::make_shared<Entry>("Original issuer", "Original account", "synthetic")});
    Window window([store] { return store; });
    QSignalSpy ready(&window, &Window::collectionReady);
    window.show();
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    auto *list = window.findChild<QListView *>();
    list->setFocus();
    QTRY_VERIFY(list->hasFocus());
    QTest::keyClick(list, Qt::Key_Return);
    QVERIFY(!QApplication::clipboard()->text().isEmpty());
    QApplication::clipboard()->setText("user content");
    QTest::keyClick(list, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(QApplication::clipboard()->text() != "user content");
    dropFixture(window, "corrupt-secret.json");
    auto *status = window.findChild<QLabel *>("importStatus");
    QTRY_VERIFY(status->text().contains("Malformed"));
    QCOMPARE(store->load().entries.first()->issuer, QString("Original issuer"));
    bool cancelled = false;
    QTimer cancel;
    answerReview(cancel, false, cancelled);
    dropFixture(window, "unsupported.json");
    QTRY_VERIFY(cancelled);
    QCOMPARE(store->load().entries.first()->issuer, QString("Original issuer"));
    bool accepted = false;
    QTimer accept;
    answerReview(accept, true, accepted);
    dropFixture(window, "unsupported.json");
    QTRY_COMPARE(ready.count(), 2);
    QVERIFY(accepted);
    QCOMPARE(store->load().entries.first()->issuer, QString::fromUtf8("Étoile 日本"));
    list->setFocus();
    QTest::keyClick(list, Qt::Key_Return);
    QTimer deletion;
    QObject::connect(&deletion, &QTimer::timeout, &deletion, [&deletion] {
        auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        deletion.stop();
        dialog->button(QMessageBox::Yes)->click();
    });
    deletion.start(10);
    window.findChild<QPushButton *>("deleteAllButton")->click();
    QTRY_COMPARE(ready.count(), 3);
    QVERIFY(store->load().entries.isEmpty());
    QVERIFY(keys->keys.isEmpty());
    QVERIFY(QApplication::clipboard()->text().isEmpty());
}
void UiTest::about() {
    QTemporaryDir directory;
    Window w([&directory] {
        return std::make_shared<VaultStore>(std::make_shared<EmptyKeys>(),
                                            std::make_shared<DiskFiles>(directory.path()));
    });
    w.show();
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, &responder, [&responder] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        responder.stop();
        auto *license = dialog->findChild<QTextBrowser *>("licenseText");
        auto *dependencies = dialog->findChild<QTextBrowser *>("dependencyText");
        QVERIFY(license);
        QVERIFY(dependencies);
        QVERIFY(license->toPlainText().contains("obtaining a copy of this software"));
        QVERIFY(license->toPlainText().endsWith("SOFTWARE."));
        QVERIFY(dependencies->toPlainText().contains("not app runtime components."));
        QCOMPARE(license->horizontalScrollBar()->maximum(), 0);
        license->verticalScrollBar()->setValue(license->verticalScrollBar()->maximum());
        QVERIFY(license->cursorForPosition(license->viewport()->rect().bottomRight()).atEnd());
        license->verticalScrollBar()->setValue(0);
        if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
            dialog->grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/about.png");
        responder.stop();
        dialog->accept();
    });
    responder.start(10);
    for (auto *action : w.findChildren<QAction *>())
        if (action->text().contains("About Aeris")) {
            action->trigger();
            return;
        }
    QFAIL("About action not found");
}
QTEST_MAIN(UiTest)
#include "test_ui.moc"
