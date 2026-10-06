#include "core/error.h"
#include "vault.h"
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif
namespace aeris {
static void syncDirectory(const QString &path) {
#ifdef Q_OS_UNIX
    int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY);
    require(fd >= 0, "Unable to synchronize vault directory.");
    int result = ::fsync(fd);
    ::close(fd);
    require(result == 0, "Unable to synchronize vault directory.");
#else
    Q_UNUSED(path)
#endif
}
static void removeStaging(const QString &directory) {
    const QRegularExpression owned(R"(^\.?(vault|transaction)\.json\.[A-Za-z0-9]{6}$)");
    QDir dir(directory);
    for (const auto &name : dir.entryList(QDir::Files | QDir::Hidden)) {
        if (!owned.match(name).hasMatch())
            continue;
        require(dir.remove(name), "Unable to remove an interrupted encrypted staging file.");
    }
}
DiskFiles::DiskFiles(QString directory) : directory_(std::move(directory)) {
    require(QDir().mkpath(directory_), "Unable to create application data directory.");
    require(QFile::setPermissions(directory_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner),
            "Unable to protect application data directory.");
    removeStaging(directory_);
}
QByteArray DiskFiles::read(const QString &name) {
    QFile file(QDir(directory_).filePath(name));
    if (!file.exists())
        return {};
    require(file.open(QIODevice::ReadOnly), "Unable to read encrypted vault.");
    require(file.size() <= 32 * 1024 * 1024, "Stored vault exceeds supported size.");
    auto bytes = file.read(32 * 1024 * 1024 + 1);
    require(file.error() == QFileDevice::NoError && bytes.size() <= 32 * 1024 * 1024,
            "Unable to read encrypted vault.");
    return bytes;
}
void DiskFiles::write(const QString &name, const QByteArray &data) {
    QSaveFile file(QDir(directory_).filePath(name));
    file.setDirectWriteFallback(false);
    require(file.open(QIODevice::WriteOnly),
            "Unable to stage encrypted vault. Check free space and permissions.");
    require(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner), "Unable to protect encrypted vault.");
    require(file.write(data) == data.size(),
            "Unable to write encrypted vault. Existing snapshot has been kept.");
    require(file.commit(), "Unable to commit encrypted vault. Existing snapshot has been kept.");
    syncDirectory(directory_);
}
void DiskFiles::remove(const QString &name) {
    QFile file(QDir(directory_).filePath(name));
    if (!file.exists())
        return;
    require(file.remove(), "Unable to remove application data. Retry deletion.");
    syncDirectory(directory_);
}
} // namespace aeris
