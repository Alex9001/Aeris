#include "window.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace aeris {
// The bundled Qt fallback chooser must not inspect the host MIME database for
// every icon: older Qt can loop there with newer desktop MIME definitions.
// Native platform dialogs remain enabled and use their own icon providers.
class ExportIcons final : public QFileIconProvider {
  public:
    QIcon icon(const QFileInfo &info) const override {
        return QFileIconProvider::icon(info.isDir() ? Folder : File);
    }
    QString type(const QFileInfo &info) const override {
        return info.isDir() ? QStringLiteral("Folder") : QStringLiteral("File");
    }
};
Window::Window(StoreFactory factory, QWidget *parent)
    : QMainWindow(parent), factory_(std::move(factory)), model_(new AccountModel(this)),
      filter_(new QSortFilterProxyModel(this)) {
    setWindowTitle("Aeris");
    setWindowIcon(QIcon(":/aeris.png"));
    resize(760, 570);
    setMinimumSize(480, 360);
    setAcceptDrops(true);
    setup();
    actions();
    connect(&watcher_, &QFutureWatcher<TaskResult>::finished, this, [this] {
        busy_ = false;
        savingOrder_ = false;
        auto future = watcher_.future();
        auto result = future.takeResult();
        auto callback = std::move(completion_);
        refresh();
        callback(std::move(result));
        if (closePending_)
            close();
    });
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] {
        list_->viewport()->update();
        clipboard_.tick();
    });
    timer->start();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this] {
        list_->viewport()->update();
        clipboard_.tick();
    });
}
Window::~Window() {
    if (busy_) {
        disconnect(&watcher_, nullptr, this, nullptr);
        QEventLoop loop;
        connect(&watcher_, &QFutureWatcher<TaskResult>::finished, &loop, &QEventLoop::quit);
        if (!watcher_.isFinished())
            loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    clipboard_.clear();
    model_->replace({});
}
void Window::setup() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(18, 14, 18, 10);
    layout->setSpacing(12);
    setupHeader(layout);
    filter_->setSourceModel(model_);
    filter_->setFilterRole(AccountModel::Search);
    filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    list_ = new AccountView;
    list_->setModel(filter_);
    list_->setUniformItemSizes(true);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setAccessibleName("Authenticator accounts");
    stack_ = new QStackedWidget;
    stack_->addWidget(list_);
    empty_ = new QLabel("Import an authenticator export\n\nYour phone remains the source of truth.");
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setWordWrap(true);
    stack_->addWidget(empty_);
    layout->addWidget(stack_, 1);
    remove_ = new QPushButton("Delete all…");
    remove_->setObjectName("deleteAllButton");
    auto *statusRow = new QHBoxLayout;
    status_ = new QLabel;
    status_->setObjectName("importStatus");
    status_->setWordWrap(true);
    status_->setTextFormat(Qt::PlainText);
    statusRow->addWidget(status_, 1);
    retry_ = new QPushButton("Retry");
    retry_->hide();
    statusRow->addWidget(retry_);
    statusRow->addWidget(remove_);
    layout->addLayout(statusRow);
    setCentralWidget(central);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString &s) {
        filter_->setFilterFixedString(s);
        refresh();
    });
    connect(import_, &QPushButton::clicked, this, &Window::pick);
    connect(remove_, &QPushButton::clicked, this, &Window::deleteAll);
    connect(retry_, &QPushButton::clicked, this, [this] {
        if (retryAction_)
            retryAction_();
    });
    connect(list_, &AccountView::moveRequested, this, &Window::reorder);
    connect(list_, &QListView::activated, this, &Window::copyCurrent);
    connect(list_, &QListView::clicked, this, &Window::copyCurrent);
    refresh();
}
void Window::actions() {
    auto *file = menuBar()->addMenu("&File");
    file->addAction("&Import / Replace…", QKeySequence::Open, this, &Window::pick);
    file->addAction("&Delete all…", this, &Window::deleteAll);
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    auto *edit = menuBar()->addMenu("&Edit");
    edit->addAction("&Copy code", QKeySequence::Copy, this, &Window::copy);
    edit->addAction("&Search", QKeySequence::Find, search_, qOverload<>(&QWidget::setFocus));
    appearanceActions();
    auto *help = menuBar()->addMenu("&Help");
    auto *action = help->addAction("&About Aeris", this, &Window::about);
    action->setMenuRole(QAction::AboutRole);
}
void Window::refresh() {
    count_->setText(filter_->rowCount() == model_->rowCount()
                        ? QString::number(model_->rowCount())
                        : QStringLiteral("%1 of %2").arg(filter_->rowCount()).arg(model_->rowCount()));
    import_->setText(model_->rowCount() ? "Replace…" : "Import…");
    import_->setEnabled(!busy_ && ready_);
    remove_->setEnabled(!busy_ && (!ready_ || model_->rowCount() > 0));
    const bool canRead = ready_ && (!busy_ || savingOrder_);
    search_->setEnabled(canRead);
    list_->setEnabled(canRead);
    list_->setReorderingEnabled(!busy_);
    if (model_->rowCount() == 0)
        empty_->setText("Import an authenticator export\n\nYour phone remains the source of truth.");
    else
        empty_->setText("No matching accounts");
    stack_->setCurrentIndex(filter_->rowCount() ? 0 : 1);
}
void Window::start(std::function<TaskResult()> work, std::function<void(TaskResult)> done,
                   const QString &message) {
    if (busy_)
        return;
    busy_ = true;
    completion_ = std::move(done);
    status_->setText(message);
    retry_->hide();
    refresh();
    watcher_.setFuture(QtConcurrent::run([work = std::move(work)] {
        try {
            return work();
        } catch (const Error &e) {
            TaskResult r;
            r.error = e.message();
            r.kind = e.kind();
            return r;
        } catch (...) {
            TaskResult r;
            r.error = "The operation failed. Existing data has not been intentionally changed. Retry or "
                      "reopen Aeris.";
            return r;
        }
    }));
}
void Window::showError(const QString &message) {
    status_->setText(message);
    retry_->setVisible(bool(retryAction_));
    emit operationFailed();
}
void Window::load() {
    auto factory = factory_;
    ready_ = false;
    retryAction_ = [this] { load(); };
    start(
        [factory] {
            TaskResult r;
            r.vault = factory()->load();
            return r;
        },
        [this](TaskResult r) {
            if (!r.error.isEmpty()) {
                showError(r.error);
                return;
            }
            ready_ = true;
            apply(r.vault);
        },
        "Opening encrypted collection…");
}
void Window::apply(const VaultResult &r) {
    clipboard_.clear();
    list_->clearFeedback();
    model_->replace(r.entries);
    search_->clear();
    ready_ = true;
    refresh();
    if (r.warning.isEmpty())
        status_->setText(model_->rowCount() ? "Click to copy · Drag to reorder." : "Ready to import.");
    else {
        retryAction_ = [this] { load(); };
        showError(r.warning);
    }
    if (filter_->rowCount())
        list_->setCurrentIndex(filter_->index(0, 0));
    emit collectionReady();
}
void Window::pick() {
    if (busy_ || !ready_)
        return;
    ExportIcons icons;
    QFileDialog dialog(this, "Import authenticator export");
    dialog.setObjectName("exportFilePicker");
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter("Authenticator exports (*.json *.2fas *.txt *.csv *.zip *.aes "
                         "*.bin *.png *.jpg *.jpeg);;All files (*)");
    dialog.setIconProvider(&icons);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
        parse(dialog.selectedFiles());
}
void Window::parse(QStringList paths, ImportOptions options) {
    retryAction_ = [this, paths] { parse(paths); };
    start(
        [paths, options] {
            TaskResult r;
            r.imported = Importer::read(paths, options);
            return r;
        },
        [this, paths, options](TaskResult r) { parsed(std::move(r), paths, options); },
        "Reading and validating export…");
}
void Window::parsed(TaskResult r, QStringList paths, ImportOptions options) {
    if (r.error.isEmpty()) {
        confirm(r.imported);
        return;
    }
    if (r.kind == ErrorKind::Ambiguous) {
        bool ok = false;
        auto choice =
            QInputDialog::getItem(this, "Backup format", r.error,
                                  {"andOTP current (PBKDF2)", "andOTP legacy (SHA-256)"}, 0, false, &ok);
        if (ok) {
            options.hint = choice.contains("current") ? FormatHint::AndOtpCurrent : FormatHint::AndOtpLegacy;
            parse(paths, options);
        }
        return;
    }
    if (r.kind == ErrorKind::PasswordRequired) {
        bool ok = false;
        QString pw = QInputDialog::getText(this, "Export password",
                                           "Password for this export:", QLineEdit::Password, {}, &ok);
        if (ok) {
            options.password = secret(pw.toUtf8());
            pw.fill(QChar('\0'));
            parse(paths, options);
        }
        return;
    }
    showError(r.error);
}
static bool reviewReplacement(QWidget *parent, const ImportResult &result, int previous) {
    QDialog dialog(parent);
    dialog.setObjectName("replacementReview");
    dialog.setWindowTitle("Replace collection");
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *summary = new QLabel(QString("%1: %2 supported accounts.\n\nReplace all %3 existing accounts?")
                                   .arg(result.source)
                                   .arg(result.entries.size())
                                   .arg(previous));
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);
    layout->addWidget(summary);
    if (!result.unsupported.isEmpty()) {
        auto *excluded = new QTextBrowser;
        QStringList lines;
        for (const auto &entry : result.unsupported)
            lines.append(entry.label + " — " + entry.reason);
        excluded->setPlainText("The following entries will be EXCLUDED:\n\n" + lines.join('\n'));
        excluded->setMinimumSize(460, 200);
        layout->addWidget(excluded);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *accept = buttons->addButton(result.unsupported.isEmpty()
                                          ? "Replace"
                                          : QString("Replace, excluding %1").arg(result.unsupported.size()),
                                      QDialogButtonBox::AcceptRole);
    accept->setObjectName("confirmReplacement");
    accept->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    return dialog.exec() == QDialog::Accepted;
}
void Window::confirm(const ImportResult &r) {
    if (!reviewReplacement(this, r, model_->rowCount())) {
        status_->setText("Import cancelled. Existing accounts kept.");
        return;
    }
    auto factory = factory_;
    auto entries = r.entries;
    retryAction_ = [this] { load(); };
    start(
        [factory, entries] {
            TaskResult result;
            result.vault = factory()->replace(entries);
            return result;
        },
        [this](TaskResult result) {
            if (!result.error.isEmpty()) {
                showError(result.error);
                return;
            }
            apply(result.vault);
        },
        "Saving encrypted collection…");
}
void Window::deleteAll() {
    if (busy_ || (ready_ && model_->rowCount() == 0))
        return;
    auto response = QMessageBox::question(this, "Delete all accounts",
                                          "Delete Aeris’s imported accounts and saved encryption "
                                          "keys?\n\nYour source exports remain untouched.",
                                          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (response != QMessageBox::Yes)
        return;
    finishDeletion();
}
void Window::finishDeletion() {
    auto factory = factory_;
    clipboard_.clear();
    model_->replace({});
    ready_ = false;
    retryAction_ = [this] { finishDeletion(); };
    start(
        [factory] {
            factory()->clear();
            return TaskResult{};
        },
        [this](TaskResult r) {
            if (!r.error.isEmpty()) {
                showError(r.error + " Deletion is incomplete; retry to finish.");
                return;
            }
            apply({});
        },
        "Deleting imported data and credentials…");
}
void Window::copy() {
    if (list_->hasFocus())
        copyCurrent();
}
void Window::copyCurrent() {
    if ((busy_ && !savingOrder_) || !ready_)
        return;
    auto i = filter_->mapToSource(list_->currentIndex());
    auto entry = model_->entry(i.row());
    if (!entry)
        return;
    try {
        clipboard_.copy(*entry);
        list_->animateCopy(list_->currentIndex());
        status_->setText(savingOrder_ ? "Saving account order… · Copied"
                                      : "Copied · clipboard clears in 30 seconds.");
    } catch (const Error &e) {
        showError(e.message());
    }
}
void Window::dragEnterEvent(QDragEnterEvent *event) {
    if (!busy_ && ready_ && event->mimeData()->hasUrls())
        event->acceptProposedAction();
}
void Window::dropEvent(QDropEvent *event) {
    if (busy_ || !ready_)
        return;
    QStringList paths;
    for (const auto &url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            showError("Drop local export files only.");
            return;
        }
        paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty()) {
        event->acceptProposedAction();
        parse(paths);
    }
}
void Window::closeEvent(QCloseEvent *event) {
    clipboard_.clear();
    if (busy_) {
        closePending_ = true;
        status_->setText("Finishing save before closing…");
        event->ignore();
        return;
    }
    event->accept();
}
static QTextBrowser *legalText(const QString &path, const QString &name) {
    auto *view = new QTextBrowser;
    view->setObjectName(name);
    view->setOpenExternalLinks(true);
    view->setMinimumSize(360, 220);
    view->document()->setDocumentMargin(14);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly))
        view->setMarkdown(QString::fromUtf8(file.readAll()));
    return view;
}
void Window::about() {
    QDialog dialog(this);
    dialog.setWindowTitle("About Aeris");
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(36, 28, 36, 28);
    layout->setSpacing(16);
    auto *art = new QLabel;
    art->setPixmap(QPixmap(":/aeris.png").scaled(144, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    art->setAlignment(Qt::AlignCenter);
    layout->addWidget(art);
    auto *title = new QLabel("Aeris " AERIS_VERSION);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() + 8);
    font.setBold(true);
    title->setFont(font);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *credit = new QLabel("CYBER FRACTURE\n\n© 2026 Aleksandr Oreshkin");
    credit->setAlignment(Qt::AlignCenter);
    layout->addWidget(credit);
    auto *links = new QLabel(
        QString("<a style='color:%1' href='https://cyberfracture.com'>Website</a> · "
                "<a style='color:%1' href='https://github.com/Alex9001/Aeris'>Source</a> · "
                "<a style='color:%1' href='https://github.com/Alex9001/Aeris/releases'>Releases</a>")
            .arg(palette().color(QPalette::Link).name()));
    links->setOpenExternalLinks(true);
    links->setAlignment(Qt::AlignCenter);
    layout->addWidget(links);
    auto *notices = new QTabWidget;
    notices->addTab(legalText(":/LICENSE", "licenseText"), "MIT license");
    notices->addTab(legalText(":/THIRD_PARTY_NOTICES.md", "dependencyText"), "Dependencies");
    layout->addWidget(notices, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(580, 680);
    dialog.exec();
}
} // namespace aeris
