#pragma once
#include <QString>
#include <stdexcept>
namespace aeris {
enum class ErrorKind { Invalid, PasswordRequired, Ambiguous, Storage };
class Error final : public std::runtime_error {
  public:
    explicit Error(QString message, ErrorKind kind = ErrorKind::Invalid)
        : std::runtime_error(message.toStdString()), message_(std::move(message)), kind_(kind) {}
    const QString &message() const {
        return message_;
    }
    ErrorKind kind() const {
        return kind_;
    }

  private:
    QString message_;
    ErrorKind kind_;
};
inline void require(bool valid, const char *message) {
    if (!valid)
        throw Error(QString::fromUtf8(message));
}
} // namespace aeris
