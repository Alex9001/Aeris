#pragma once
#include "account_view.h"
#include "clipboard.h"
#include "import/importer.h"
#include "storage/vault.h"
#include <QFutureWatcher>
#include <QMainWindow>
#include <functional>
class QLineEdit;
class QListView;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QHBoxLayout;
class QToolButton;
namespace aeris {
struct TaskResult {
    VaultResult vault;
    ImportResult imported;
    QString error;
    ErrorKind kind = ErrorKind::Invalid;
};
class Window final : public QMainWindow {
    Q_OBJECT
  public:
    using StoreFactory = std::function<std::shared_ptr<VaultStore>()>;
    explicit Window(StoreFactory factory, QWidget *parent = nullptr);
    ~Window() override;
    void load();
  signals:
    void collectionReady();
    void operationFailed();
    void orderSaved();

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

  private:
    StoreFactory factory_;
    AccountModel *model_;
    QSortFilterProxyModel *filter_;
    AccountView *list_;
    QLabel *count_;
    QHBoxLayout *toolbar_;
    QToolButton *themeButton_;
    QAction *searchIcon_;
    QList<QAction *> layoutActions_;
    QList<QAction *> themeActions_;
    QString theme_;
    QLineEdit *search_;
    QLabel *status_;
    QLabel *empty_;
    QPushButton *import_;
    QPushButton *remove_;
    QPushButton *retry_;
    QStackedWidget *stack_;
    QFutureWatcher<TaskResult> watcher_;
    std::function<void(TaskResult)> completion_;
    std::function<void()> retryAction_;
    CodeClipboard clipboard_;
    bool busy_ = false, ready_ = false;
    bool savingOrder_ = false, closePending_ = false;
    void setup();
    void actions();
    void setupHeader(QVBoxLayout *layout);
    void appearanceActions();
    void setTheme(const QString &theme);
    void setLayout(AccountLayout layout);
    void refreshIcons();
    void refresh();
    void pick();
    void copy();
    void copyCurrent();
    void reorder(const QModelIndex &source, int destination);
    void finishReorder(TaskResult result, int row, int target);
    void about();
    void deleteAll();
    void finishDeletion();
    void start(std::function<TaskResult()> work, std::function<void(TaskResult)> done,
               const QString &message);
    void apply(const VaultResult &result);
    void showError(const QString &message);
    void parse(QStringList paths, ImportOptions options = {});
    void parsed(TaskResult result, QStringList paths, ImportOptions options);
    void confirm(const ImportResult &result);
};
} // namespace aeris
