#include "ui/window.h"
#include <QAbstractItemModelTester>
#include <QAction>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSemaphore>
#include <QSettings>
#include <QtTest>
using namespace aeris;

class OrderKeys final : public CredentialStore {
  public:
    QMap<QString, SecretPtr> values;
    bool failWrite = false, failRemove = false;
    bool delayWrite = false;
    QSemaphore entered, proceed;
    SecretPtr read(const QString &id) override {
        return values.value(id);
    }
    void write(const QString &id, const QByteArray &key) override {
        if (delayWrite) {
            entered.release();
            proceed.acquire();
        }
        if (failWrite)
            throw Error("Keyring locked.");
        values[id] = secret(key);
    }
    void remove(const QString &id) override {
        if (failRemove)
            throw Error("Keyring cleanup failed.");
        values.remove(id);
    }
};
class OrderFiles final : public SnapshotFiles {
  public:
    explicit OrderFiles(const QString &path) : disk(path) {}
    DiskFiles disk;
    QString failWrite;
    QByteArray read(const QString &name) override {
        return disk.read(name);
    }
    void write(const QString &name, const QByteArray &data) override {
        if (name == failWrite)
            throw Error("Disk full.");
        disk.write(name, data);
    }
    void remove(const QString &name) override {
        disk.remove(name);
    }
};
static Entries accounts() {
    return {std::make_shared<Entry>("Google", "duplicate", "synthetic first"),
            std::make_shared<Entry>("GitHub", "hidden", "synthetic second"),
            std::make_shared<Entry>("Google", "duplicate", "synthetic third"),
            std::make_shared<Entry>("Other", "hidden", "synthetic fourth"),
            std::make_shared<Entry>("Google", "last", "synthetic fifth", "SHA256", 8, 60)};
}
struct Harness {
    QTemporaryDir directory;
    std::shared_ptr<OrderKeys> keys = std::make_shared<OrderKeys>();
    std::shared_ptr<OrderFiles> files = std::make_shared<OrderFiles>(directory.path());
    Window::StoreFactory factory() const {
        return [keys = keys, files = files] { return std::make_shared<VaultStore>(keys, files); };
    }
};
static void openWindow(Window &window, int layout) {
    window.resize(920, 850);
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QSignalSpy ready(&window, &Window::collectionReady);
    window.load();
    QTRY_COMPARE(ready.count(), 1);
    const QStringList names{"List", "Compact", "Cards"};
    window.findChild<QAction *>("layout." + names[layout])->trigger();
    QTest::qWait(30);
}
static QPoint dropPoint(AccountView &view, int row, bool after) {
    auto rect = view.visualRect(view.model()->index(row, 0));
    if (view.accountLayout() == AccountLayout::Cards)
        return QPoint(after ? rect.right() - 6 : rect.left() + 6, rect.center().y());
    return QPoint(rect.center().x(), after ? rect.bottom() - 6 : rect.top() + 6);
}
static void beginDrag(AccountView &view, int row, QPoint destination) {
    const auto start = view.visualRect(view.model()->index(row, 0)).center();
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent move(QEvent::MouseMove, destination, view.viewport()->mapToGlobal(destination), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &move);
    QTest::qWait(20);
}
static void release(AccountView &view, QPoint destination) {
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, destination);
}
static void compareEntries(const Entries &actual, const Entries &expected) {
    QCOMPARE(actual.size(), expected.size());
    for (int i = 0; i < actual.size(); ++i) {
        QCOMPARE(actual[i]->issuer, expected[i]->issuer);
        QCOMPARE(actual[i]->account, expected[i]->account);
        QCOMPARE(actual[i]->key->bytes(), expected[i]->key->bytes());
        QCOMPARE(actual[i]->algorithm, expected[i]->algorithm);
        QCOMPARE(actual[i]->digits, expected[i]->digits);
        QCOMPARE(actual[i]->period, expected[i]->period);
        QCOMPARE(actual[i]->type, expected[i]->type);
    }
}
class OrderTest final : public QObject {
    Q_OBJECT
    QTemporaryDir preferences_;
  private slots:
    void initTestCase();
    void model();
    void gestures_data();
    void gestures();
    void persistence_data();
    void persistence();
    void filteredPlacement();
    void saveFailure_data();
    void saveFailure();
    void autoscroll();
    void pendingSave();
};
void OrderTest::initTestCase() {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences_.path());
}
void OrderTest::model() {
    AccountModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    auto original = accounts();
    model.replace(original);
    QPersistentModelIndex first(model.index(0, 0));
    const auto color = first.data(AccountModel::IconColor);
    const auto brand = first.data(AccountModel::BrandIdentifier);
    QCOMPARE(brand.toString(), QString("google"));
    auto proposed = model.reordered(0, 5);
    QCOMPARE(model.entry(0), original[0]);
    QCOMPARE(proposed.last(), original[0]);
    QVERIFY(model.moveRow({}, 0, {}, 5));
    QCOMPARE(first.row(), 4);
    QCOMPARE(first.data(AccountModel::IconColor), color);
    QCOMPARE(first.data(AccountModel::BrandIdentifier), brand);
    QCOMPARE(model.entry(4), original[0]);
    QCOMPARE(model.entry(1), original[2]); // Same issuer/account, different secret.
    QVERIFY(model.moveRow({}, 4, {}, 0));
    QCOMPARE(first.row(), 0);
    QVERIFY(!model.moveRow({}, -1, {}, 1));
    QVERIFY(!model.moveRow({}, 5, {}, 0));
    QVERIFY(!model.moveRow({}, 0, {}, 6));
    QVERIFY(!model.moveRow({}, 0, {}, 0));
    QVERIFY(!model.moveRow({}, 0, {}, 1));
    QVERIFY(!model.moveRows({}, 0, 2, {}, 5));
    QVERIFY(!model.moveRow(model.index(0, 0), 0, {}, 2));
    QVERIFY(model.reordered(0, 1).isEmpty());
}
void OrderTest::gestures_data() {
    QTest::addColumn<int>("layout");
    for (int layout = 0; layout < 3; ++layout)
        QTest::newRow(qPrintable(QString::number(layout))) << layout;
}
void OrderTest::gestures() {
    QFETCH(int, layout);
    AccountModel model;
    model.replace(accounts());
    AccountView view;
    view.setModel(&model);
    view.setAccountLayout(AccountLayout(layout));
    view.resize(900, 650);
    view.show();
    QTest::qWait(30);
    QSignalSpy moved(&view, &AccountView::moveRequested);
    QSignalSpy clicked(&view, &QListView::clicked);
    auto end = dropPoint(view, 4, true);
    beginDrag(view, 0, end);
    if (qEnvironmentVariableIsSet("AERIS_QA_DIR"))
        view.grab().save(qEnvironmentVariable("AERIS_QA_DIR") + "/drag-" + QString::number(layout) + ".png");
    release(view, end);
    QCOMPARE(moved.count(), 1);
    QCOMPARE(qvariant_cast<QModelIndex>(moved[0][0]).row(), 0);
    QCOMPARE(moved[0][1].toInt(), 5);
    QCOMPARE(clicked.count(), 0);
    auto start = dropPoint(view, 0, false);
    beginDrag(view, 4, start);
    release(view, start);
    QCOMPARE(moved.count(), 2);
    QCOMPARE(moved[1][1].toInt(), 0);
    beginDrag(view, 0, end);
    QTest::keyClick(&view, Qt::Key_Escape);
    release(view, end);
    beginDrag(view, 0, end);
    release(view, QPoint(-30, -30));
    beginDrag(view, 0, dropPoint(view, 0, true));
    release(view, dropPoint(view, 0, true));
    QCOMPARE(moved.count(), 2);
    QCOMPARE(clicked.count(), 0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, dropPoint(view, 1, false));
    QCOMPARE(clicked.count(), 1);
}
void OrderTest::persistence_data() {
    QTest::addColumn<int>("layout");
    QTest::addColumn<bool>("filtered");
    for (int layout = 0; layout < 3; ++layout) {
        QTest::newRow(qPrintable(QString::number(layout) + "-all")) << layout << false;
        QTest::newRow(qPrintable(QString::number(layout) + "-search")) << layout << true;
    }
}
void OrderTest::persistence() {
    QFETCH(int, layout);
    QFETCH(bool, filtered);
    Harness h;
    auto expected = accounts();
    h.factory()()->replace(expected);
    Window window(h.factory());
    openWindow(window, layout);
    auto *view = window.findChild<AccountView *>();
    auto *search = window.findChild<QLineEdit *>("searchAccounts");
    const auto query = filtered ? QString("Google") : QString();
    search->setText(query);
    QTest::qWait(20);
    QCOMPARE(view->model()->rowCount(), filtered ? 3 : 5);
    QSignalSpy saved(&window, &Window::orderSaved);
    QApplication::clipboard()->setText("user clipboard");
    auto end = dropPoint(*view, view->model()->rowCount() - 1, true);
    beginDrag(*view, 0, end);
    release(*view, end);
    QTRY_COMPARE(saved.count(), 1);
    QTRY_VERIFY(view->hasFocus());
    expected.move(0, 4);
    compareEntries(h.factory()()->load().entries, expected);
    QCOMPARE(search->text(), query);
    QCOMPARE(view->currentIndex().row(), view->model()->rowCount() - 1);
    QCOMPARE(QApplication::clipboard()->text(), QString("user clipboard"));
    QVERIFY(!view->copyFeedbackActive());
    const auto start = dropPoint(*view, 0, false);
    beginDrag(*view, view->model()->rowCount() - 1, start);
    release(*view, start);
    QTRY_COMPARE(saved.count(), 2);
    expected.move(4, filtered ? 1 : 0);
    compareEntries(h.factory()()->load().entries, expected);
    QCOMPARE(search->text(), query);
    Window reopened(h.factory());
    openWindow(reopened, layout);
    const auto *loaded = reopened.findChild<AccountModel *>();
    for (int row = 0; row < expected.size(); ++row)
        compareEntries({loaded->entry(row)}, {expected[row]});
    QCOMPARE(h.keys->values.size(), 1);
}
void OrderTest::filteredPlacement() {
    Harness h;
    auto expected = accounts().first(4);
    h.factory()()->replace(expected);
    Window window(h.factory());
    openWindow(window, 0);
    auto *view = window.findChild<AccountView *>();
    window.findChild<QLineEdit *>("searchAccounts")->setText("Google");
    QTest::qWait(20);
    QCOMPARE(view->model()->rowCount(), 2);
    QSignalSpy saved(&window, &Window::orderSaved);
    const auto end = QPoint(100, view->viewport()->height() - 50);
    beginDrag(*view, 0, end);
    release(*view, end);
    QTRY_COMPARE(saved.count(), 1);
    expected.move(0, 2); // After the last visible result, ahead of the hidden tail.
    compareEntries(h.factory()()->load().entries, expected);
    QCOMPARE(expected.last()->issuer, QString("Other"));
    // A no-op must not rotate the credential or write a new encrypted snapshot.
    const auto snapshot = h.files->read("vault.json");
    const auto same = dropPoint(*view, 1, true);
    beginDrag(*view, 1, same);
    release(*view, same);
    QTest::qWait(50);
    QCOMPARE(saved.count(), 1);
    QCOMPARE(h.files->read("vault.json"), snapshot);
}
void OrderTest::saveFailure_data() {
    QTest::addColumn<QString>("failure");
    QTest::addColumn<bool>("committed");
    QTest::newRow("snapshot") << QString("vault.json") << false;
    QTest::newRow("journal") << QString("transaction.json") << false;
    QTest::newRow("keyring") << QString("keyring") << false;
    QTest::newRow("cleanup") << QString("cleanup") << true;
}
void OrderTest::saveFailure() {
    QFETCH(QString, failure);
    QFETCH(bool, committed);
    Harness h;
    auto expected = accounts();
    h.factory()()->replace(expected);
    Window window(h.factory());
    openWindow(window, 0);
    auto *view = window.findChild<AccountView *>();
    const auto before = h.files->read("vault.json");
    h.files->failWrite = failure;
    h.keys->failWrite = failure == "keyring";
    h.keys->failRemove = failure == "cleanup";
    QSignalSpy failed(&window, &Window::operationFailed);
    QSignalSpy saved(&window, &Window::orderSaved);
    const auto end = dropPoint(*view, 4, true);
    beginDrag(*view, 0, end);
    release(*view, end);
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(saved.count(), committed ? 1 : 0);
    QVERIFY(view->isEnabled());
    if (committed)
        expected.move(0, 4);
    else
        QCOMPARE(h.files->read("vault.json"), before);
    auto *model = window.findChild<AccountModel *>();
    compareEntries({model->entry(0)}, {expected[0]});
    h.files->failWrite.clear();
    h.keys->failWrite = h.keys->failRemove = false;
    compareEntries(h.factory()()->load().entries, expected);
    if (!committed) {
        beginDrag(*view, 0, end);
        release(*view, end);
        QTRY_COMPARE(saved.count(), 1);
        expected.move(0, 4);
        compareEntries(h.factory()()->load().entries, expected);
    }
}
void OrderTest::pendingSave() {
    Harness h;
    auto expected = accounts();
    h.factory()()->replace(expected);
    Window window(h.factory());
    openWindow(window, 0);
    auto *view = window.findChild<AccountView *>();
    auto *search = window.findChild<QLineEdit *>("searchAccounts");
    QSignalSpy saved(&window, &Window::orderSaved);
    h.keys->delayWrite = true;
    // Always release the worker, including when an assertion fails.
    auto unblock = qScopeGuard([&] { h.keys->proceed.release(); });
    const auto end = dropPoint(*view, 4, true);
    beginDrag(*view, 0, end);
    release(*view, end);
    QTRY_VERIFY(h.keys->entered.available() > 0);
    QVERIFY(view->isEnabled());
    QVERIFY(search->isEnabled());
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->visualRect(view->model()->index(0, 0)).center());
    QVERIFY(view->copyFeedbackActive());
    QVERIFY(window.findChild<QLabel *>("importStatus")->text().contains("Saving account order"));
    QSignalSpy moves(view, &AccountView::moveRequested);
    beginDrag(*view, 1, end);
    release(*view, end);
    QCOMPARE(moves.count(), 0);
    search->setText("GitHub");
    QCOMPARE(view->model()->rowCount(), 1);
    window.close();
    QVERIFY(window.isVisible()); // Closing must keep the event loop servicing the save.
    h.keys->proceed.release();
    unblock.dismiss();
    QTRY_COMPARE(saved.count(), 1);
    QTRY_VERIFY(!window.isVisible());
    expected.move(0, 4);
    compareEntries(h.factory()()->load().entries, expected);
}
void OrderTest::autoscroll() {
    Entries entries;
    for (int i = 0; i < 100; ++i)
        entries.append(std::make_shared<Entry>("Account", QString::number(i), "synthetic"));
    AccountModel model;
    model.replace(entries);
    AccountView view;
    view.setModel(&model);
    view.resize(700, 250);
    view.show();
    QTest::qWait(30);
    const auto bottom = QPoint(200, view.viewport()->height() - 2);
    beginDrag(view, 0, bottom);
    QTRY_VERIFY(view.verticalScrollBar()->value() > 0);
    const QPoint top(200, 2);
    QMouseEvent move(QEvent::MouseMove, top, view.viewport()->mapToGlobal(top), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(view.viewport(), &move);
    QTRY_COMPARE(view.verticalScrollBar()->value(), 0);
    QTest::keyClick(&view, Qt::Key_Escape);
    release(view, bottom);
    const int stopped = view.verticalScrollBar()->value();
    QTest::qWait(100);
    QCOMPARE(view.verticalScrollBar()->value(), stopped);
}
QTEST_MAIN(OrderTest)
#include "test_order.moc"
