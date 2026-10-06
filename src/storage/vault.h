#pragma once
#include "core/entry.h"
#include <QStringList>
namespace aeris {
class CredentialStore {
  public:
    virtual ~CredentialStore() = default;
    virtual SecretPtr read(const QString &id) = 0;
    virtual void write(const QString &id, const QByteArray &key) = 0;
    virtual void remove(const QString &id) = 0;
};
class SystemCredentials final : public CredentialStore {
  public:
    SecretPtr read(const QString &id) override;
    void write(const QString &id, const QByteArray &key) override;
    void remove(const QString &id) override;
};
class SnapshotFiles {
  public:
    virtual ~SnapshotFiles() = default;
    virtual QByteArray read(const QString &name) = 0;
    virtual void write(const QString &name, const QByteArray &data) = 0;
    virtual void remove(const QString &name) = 0;
};
class DiskFiles final : public SnapshotFiles {
  public:
    explicit DiskFiles(QString directory);
    QByteArray read(const QString &name) override;
    void write(const QString &name, const QByteArray &data) override;
    void remove(const QString &name) override;

  private:
    QString directory_;
};
struct VaultResult {
    Entries entries;
    QString warning;
};
class VaultStore final {
  public:
    VaultStore(std::shared_ptr<CredentialStore> credentials, std::shared_ptr<SnapshotFiles> files);
    VaultResult load();
    VaultResult replace(const Entries &entries);
    void clear();

  private:
    std::shared_ptr<CredentialStore> credentials_;
    std::shared_ptr<SnapshotFiles> files_;
    void recover();
    QString activeId();
    void journal(const QString &oldId, const QString &newId, bool deleting = false);
    void retire(const QStringList &ids, const QString &keep);
};
} // namespace aeris
