#include "ui/window.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImageReader>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <sodium.h>
int main(int argc, char **argv) {
    QElapsedTimer startup;
    startup.start();
    QApplication app(argc, argv);
    app.setApplicationName("aeris");
    app.setApplicationDisplayName("Aeris");
    app.setApplicationVersion(AERIS_VERSION);
    app.setOrganizationName("CYBER FRACTURE");
    app.setOrganizationDomain("cyberfracture.com");
    app.setDesktopFileName("com.cyberfracture.aeris");
    app.setQuitOnLastWindowClosed(true);
    if (sodium_init() < 0)
        return 1;
    QImageReader::setAllocationLimit(128);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(path)) {
        QMessageBox::critical(nullptr, "Aeris", "Cannot create the application data directory.");
        return 1;
    }
    QLockFile lock(QDir(path).filePath("aeris.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        QMessageBox::information(nullptr, "Aeris",
                                 "Aeris is already running, or its data directory is unavailable.");
        return 1;
    }
    aeris::Window window([path] {
        return std::make_shared<aeris::VaultStore>(std::make_shared<aeris::SystemCredentials>(),
                                                   std::make_shared<aeris::DiskFiles>(path));
    });
    window.show();
    QTimer::singleShot(0, &window, &aeris::Window::load);
    if (app.arguments().contains("--startup-report"))
        QObject::connect(&window, &aeris::Window::collectionReady, &app,
                         [&startup] { qInfo("Startup ready: %lld ms", startup.elapsed()); });
    if (app.arguments().contains("--smoke-test")) {
        QObject::connect(&window, &aeris::Window::collectionReady, &app,
                         [&app] { QTimer::singleShot(100, &app, &QApplication::quit); });
        QObject::connect(&window, &aeris::Window::operationFailed, &app, [&app] { app.exit(1); });
        QTimer::singleShot(30000, &app, [&app] { app.exit(2); });
    }
    return app.exec();
}
