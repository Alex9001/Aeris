#include "core/error.h"
#include "storage/vault.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QUuid>
#include <QtConcurrent>
#include <cstdio>
using namespace aeris;
static void worker(const std::function<void()> &operation) {
    QThreadPool pool;
    QFutureWatcher<void> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
    auto future = QtConcurrent::run(&pool, operation);
    watcher.setFuture(future);
    loop.exec();
    future.waitForFinished();
}
static void vaultRoundTrip() {
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create synthetic vault directory.");
    auto store = std::make_shared<VaultStore>(std::make_shared<SystemCredentials>(),
                                              std::make_shared<DiskFiles>(directory.path()));
    Entries entries{std::make_shared<Entry>("Synthetic A", "test", "first test key"),
                    std::make_shared<Entry>("Synthetic B", "test", "second test key")};
    try {
        worker([&] { store->replace(entries); });
        for (int i = 0; i < 3; ++i) {
            worker([&] {
                require(store->load().entries.first()->issuer == entries.first()->issuer,
                        "Stored order mismatch.");
            });
            entries.move(0, 1);
            worker([&] { store->replace(entries); });
        }
        worker([&] {
            require(store->load().entries.first()->issuer == entries.first()->issuer,
                    "Reordered vault mismatch.");
        });
        worker([&] { store->clear(); });
    } catch (...) {
        worker([&] { store->clear(); });
        throw;
    }
    std::puts("Synthetic vault import/load/reorder/delete passed across fresh worker threads.");
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("aeris-keychain-test");
    const bool unavailable = app.arguments().contains("--expect-unavailable");
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    SystemCredentials keys;
    try {
        auto key = secret(crypto::random(32));
        worker([&] { keys.write(id, key->bytes()); });
        std::puts("Worker write completed.");
        worker([&] { require(keys.read(id)->bytes() == key->bytes(), "Keychain read mismatch."); });
        worker([&] { keys.remove(id); });
        if (unavailable)
            return 1;
        std::puts("Keychain write/read/delete passed.");
        vaultRoundTrip();
        return 0;
    } catch (...) {
        try {
            worker([&] { keys.remove(id); });
        } catch (...) {
        }
        if (unavailable) {
            std::puts("Unavailable keyring rejected; insecure fallback disabled.");
            return 0;
        }
        std::puts("System keyring unavailable; native round trip not verified.");
        return 77;
    }
}
