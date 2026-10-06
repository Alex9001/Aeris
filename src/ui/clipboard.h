#pragma once
#include "core/otp.h"
#include <QElapsedTimer>
#include <QMimeData>
#include <QPointer>
namespace aeris {
class CodeClipboard final {
  public:
    using MillisecondClock = std::function<qint64()>;
    explicit CodeClipboard(OtpEngine::Clock clock = {}, MillisecondClock monotonic = {});
    ~CodeClipboard();
    void copy(const Entry &entry);
    void tick();
    void clear();
    bool owns() const;

  private:
    OtpEngine engine_;
    QPointer<QMimeData> owned_;
    MillisecondClock monotonic_;
    qint64 copiedAt_ = 0, wallDeadline_ = 0;
};
} // namespace aeris
