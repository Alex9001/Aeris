#include "identity.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QTextBoundaryFinder>
namespace aeris {
static QString firstCharacter(const QString &text) {
    QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, text);
    return text.left(boundary.toNextBoundary());
}
static QString initials(const QString &text) {
    const auto words = text.trimmed().split(QRegularExpression("[\\s@._-]+"), Qt::SkipEmptyParts);
    if (words.isEmpty())
        return QStringLiteral("?");
    auto letters = firstCharacter(words.first());
    if (words.size() > 1)
        letters += firstCharacter(words.last());
    else {
        for (const auto character : words.first().mid(1)) {
            if (character.isUpper()) {
                letters += character;
                break;
            }
        }
    }
    return letters.toUpper();
}
AccountIdentity accountIdentity(const QString &issuer, const QString &account) {
    // Visual identity uses public labels only, never the OTP secret.
    const auto hash =
        QCryptographicHash::hash(issuer.toUtf8() + '\0' + account.toUtf8(), QCryptographicHash::Sha256);
    const auto hue = (quint8(hash[0]) * 256 + quint8(hash[1])) % 360;
    return {
        initials(issuer.isEmpty() ? account : issuer), QColor::fromHsv(hue, 170, 112), quint8(hash[2]), {}};
}
} // namespace aeris
