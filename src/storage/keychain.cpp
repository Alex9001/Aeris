#include "core/error.h"
#include "vault.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>
#include <qt6keychain/keychain.h>
namespace aeris {
namespace {
enum class Operation { Read, Write, Remove };
struct Request {
    QEventLoop *waiter;
    QByteArray data;
    QString error;
    bool completed = false;
    void finish(QString message = {}) {
        if (completed)
            return;
        completed = true;
        error = std::move(message);
        QMetaObject::invokeMethod(waiter, "quit", Qt::QueuedConnection);
    }
};
// QtKeychain's process-wide executor and libsecret callbacks must stay on the
// application's continuously running event loop, never on expiring pool threads.
QPointer<QKeychain::Job> activeJob;
QKeychain::Job *makeJob(Operation operation, const QByteArray &key) {
    const QString service = "com.cyberfracture.aeris";
    if (operation == Operation::Read)
        return new QKeychain::ReadPasswordJob(service, QCoreApplication::instance());
    if (operation == Operation::Remove)
        return new QKeychain::DeletePasswordJob(service, QCoreApplication::instance());
    auto *job = new QKeychain::WritePasswordJob(service, QCoreApplication::instance());
    job->setBinaryData(key);
    return job;
}
void startRequest(const std::shared_ptr<Request> &request, Operation operation, const QString &id,
                  const QByteArray &key) {
    if (activeJob) {
        request->finish("The system keyring is still processing an earlier request. Unlock it and retry.");
        return;
    }
    auto *job = makeJob(operation, key);
    activeJob = job;
    job->setKey(id);
    job->setInsecureFallback(false);
    auto *deadline = new QTimer(job);
    deadline->setSingleShot(true);
    QObject::connect(deadline, &QTimer::timeout, job, [request] {
        // Keep the job alive for late backend callbacks. Block further keyring
        // mutations until it finishes, leaving the recovery journal intact.
        request->finish("The system keyring did not respond. Unlock it and retry.");
    });
    QObject::connect(
        job, &QKeychain::Job::finished, job, [request, operation, deadline](QKeychain::Job *done) {
            deadline->stop();
            activeJob.clear();
            if (request->completed)
                return;
            const bool missing = operation == Operation::Remove && done->error() == QKeychain::EntryNotFound;
            if (done->error() != QKeychain::NoError && !missing) {
                request->finish("The system keyring is unavailable or locked. Unlock it and retry.");
                return;
            }
            if (operation == Operation::Read)
                request->data = static_cast<QKeychain::ReadPasswordJob *>(done)->binaryData();
            request->finish();
        });
    deadline->start(25000);
    job->start();
}
QByteArray run(Operation operation, const QString &id, const QByteArray &key = {}) {
    QEventLoop loop;
    auto request = std::make_shared<Request>();
    request->waiter = &loop;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [request, operation, id, key] { startRequest(request, operation, id, key); }, Qt::QueuedConnection);
    loop.exec();
    if (!request->error.isEmpty())
        throw Error(request->error, ErrorKind::Storage);
    return request->data;
}
} // namespace
SecretPtr SystemCredentials::read(const QString &id) {
    auto data = run(Operation::Read, id);
    require(data.size() == 32, "The vault credential is invalid.");
    return secret(std::move(data));
}
void SystemCredentials::write(const QString &id, const QByteArray &key) {
    run(Operation::Write, id, key);
}
void SystemCredentials::remove(const QString &id) {
    if (!id.isEmpty())
        run(Operation::Remove, id);
}
} // namespace aeris
