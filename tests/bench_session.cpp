#include "ui/window.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
using namespace aeris;
class BenchmarkKeys final : public CredentialStore {
  public:
    QMap<QString, SecretPtr> values;
    SecretPtr read(const QString &id) override {
        return values.value(id);
    }
    void write(const QString &id, const QByteArray &key) override {
        values[id] = secret(key);
    }
    void remove(const QString &id) override {
        values.remove(id);
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    auto keys = std::make_shared<BenchmarkKeys>();
    auto files = std::make_shared<DiskFiles>(directory.path());
    auto store = std::make_shared<VaultStore>(keys, files);
    Entries entries;
    for (int i = 0; i < 1000; ++i)
        entries.append(std::make_shared<Entry>("Synthetic issuer " + QString::number(i),
                                               "synthetic@example.test", "12345678901234567890"));
    store->replace(entries);
    entries.clear();
    QElapsedTimer timer;
    timer.start();
    Window window([store] { return store; });
    QObject::connect(&window, &Window::collectionReady, &app, [&timer] {
        std::printf("READY %lld\n", timer.elapsed());
        std::fflush(stdout);
    });
    window.show();
    window.load();
    QTimer::singleShot(12000, &app, &QApplication::quit);
    return app.exec();
}
