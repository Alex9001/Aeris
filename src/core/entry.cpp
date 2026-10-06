#include "entry.h"
#include "error.h"
namespace aeris {
Entry::Entry(QString i, QString a, QByteArray k, QString alg, int d, int p, TokenType t)
    : issuer(std::move(i)), account(std::move(a)), algorithm(std::move(alg)), key(secret(std::move(k))),
      digits(d), period(p), type(t) {
    require(issuer.size() <= 1024 && account.size() <= 1024, "Account label exceeds supported length.");
    require(!key->bytes().isEmpty() && key->bytes().size() <= 1024, "Invalid secret length.");
    require(QStringList{"SHA1", "SHA256", "SHA512"}.contains(algorithm), "Unsupported OTP algorithm.");
    require(digits >= 1 && digits <= 10, "Unsupported OTP digit count (1–10 supported).");
    require(period >= 1 && period <= 86400, "Unsupported OTP period (1–86400 seconds supported).");
    if (type == TokenType::Steam)
        require(algorithm == "SHA1" && digits == 5, "Invalid Steam parameters.");
}
} // namespace aeris
