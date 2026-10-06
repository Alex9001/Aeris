#pragma once
#include "crypto.h"
#include <QVector>
namespace aeris {
enum class TokenType { Totp, Steam };
class Entry final {
  public:
    Entry(QString issuer, QString account, QByteArray key, QString algorithm = "SHA1", int digits = 6,
          int period = 30, TokenType type = TokenType::Totp);
    const QString issuer, account, algorithm;
    const SecretPtr key;
    const int digits, period;
    const TokenType type;
};
using EntryPtr = std::shared_ptr<const Entry>;
using Entries = QVector<EntryPtr>;
} // namespace aeris
