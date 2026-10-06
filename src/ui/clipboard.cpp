#include "clipboard.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <chrono>
namespace aeris {
CodeClipboard::CodeClipboard(OtpEngine::Clock clock, MillisecondClock monotonic)
    : engine_(std::move(clock)), monotonic_(std::move(monotonic)) {
    if (!monotonic_)
        monotonic_ = [] {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        };
}
CodeClipboard::~CodeClipboard() {
    clear();
}
void CodeClipboard::copy(const Entry &entry) {
    auto code = engine_.current(entry).text;
    auto *mime = new QMimeData;
    mime->setText(code);
    owned_ = mime;
    QApplication::clipboard()->setMimeData(mime, QClipboard::Clipboard);
    copiedAt_ = monotonic_();
    wallDeadline_ = QDateTime::currentMSecsSinceEpoch() + 30000;
    code.fill(QChar('\0'));
}
bool CodeClipboard::owns() const {
    return owned_ && QApplication::clipboard()->mimeData(QClipboard::Clipboard) == owned_;
}
void CodeClipboard::clear() {
    if (owns())
        QApplication::clipboard()->clear(QClipboard::Clipboard);
    owned_.clear();
    wallDeadline_ = 0;
}
void CodeClipboard::tick() {
    if (wallDeadline_ != 0 &&
        (monotonic_() - copiedAt_ >= 30000 || QDateTime::currentMSecsSinceEpoch() >= wallDeadline_))
        clear();
}
} // namespace aeris
