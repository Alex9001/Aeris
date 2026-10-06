#pragma once
#include <QByteArray>
#include <QString>
#include <memory>
namespace aeris {
void wipe(QByteArray &bytes);
class Secret final {
  public:
    explicit Secret(QByteArray bytes) : bytes_(std::move(bytes)) {
        bytes_.detach();
    }
    ~Secret() {
        wipe(bytes_);
    }
    Secret(const Secret &) = delete;
    Secret &operator=(const Secret &) = delete;
    const QByteArray &bytes() const {
        return bytes_;
    }

  private:
    QByteArray bytes_;
};
using SecretPtr = std::shared_ptr<const Secret>;
SecretPtr secret(QByteArray bytes);
namespace crypto {
QByteArray random(int size);
QByteArray base32(const QString &text);
QByteArray base64(const QString &text);
QByteArray hex(const QString &text);
QByteArray hmac(const QByteArray &key, const QByteArray &data, const QString &algorithm);
QByteArray hash(const QByteArray &data);
SecretPtr pbkdf(const QByteArray &password, const QByteArray &salt, int iterations, const QString &algorithm);
SecretPtr scrypt(const QByteArray &password, const QByteArray &salt, quint64 n, quint64 r, quint64 p);
SecretPtr argon(const QByteArray &password, const QByteArray &salt, quint64 ops, quint64 memory);
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &nonce,
                const QByteArray &aad = {});
QByteArray open(const QByteArray &cipher, const QByteArray &key, const QByteArray &nonce,
                const QByteArray &aad = {});
QByteArray enteOpen(const QByteArray &cipher, const QByteArray &key, const QByteArray &header);
} // namespace crypto
} // namespace aeris
