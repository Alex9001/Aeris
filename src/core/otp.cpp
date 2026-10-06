#include "otp.h"
#include "error.h"
#include <QDateTime>
#include <QtEndian>
namespace aeris {
OtpEngine::OtpEngine(Clock clock) : clock_(std::move(clock)) {
    if (!clock_)
        clock_ = [] { return QDateTime::currentSecsSinceEpoch(); };
}
Code OtpEngine::current(const Entry &e) const {
    return at(e, clock_());
}
static QString steam(quint32 value) {
    const QString alphabet = "23456789BCDFGHJKMNPQRTVWXY";
    QString out;
    for (int i = 0; i < 5; ++i) {
        out += alphabet.at(value % 26);
        value /= 26;
    }
    return out;
}
Code OtpEngine::at(const Entry &e, qint64 seconds) {
    require(seconds >= 0, "System clock is before the Unix epoch.");
    QByteArray counter(8, '\0');
    qToBigEndian<quint64>(quint64(seconds / e.period), counter.data());
    auto mac = crypto::hmac(e.key->bytes(), counter, e.algorithm);
    const auto offset = quint8(mac.back()) & 15;
    const quint32 value = qFromBigEndian<quint32>(mac.constData() + offset) & 0x7fffffff;
    wipe(mac);
    if (e.type == TokenType::Steam)
        return {steam(value), e.period - int(seconds % e.period)};
    quint64 modulus = 1;
    for (int i = 0; i < e.digits; ++i)
        modulus *= 10;
    return {QString::number(value % modulus).rightJustified(e.digits, '0'),
            e.period - int(seconds % e.period)};
}
} // namespace aeris
