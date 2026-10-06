#pragma once
#include "entry.h"
#include <functional>
namespace aeris {
struct Code {
    QString text;
    int remaining;
};
class OtpEngine final {
  public:
    using Clock = std::function<qint64()>;
    explicit OtpEngine(Clock clock = {});
    Code current(const Entry &entry) const;
    static Code at(const Entry &entry, qint64 seconds);

  private:
    Clock clock_;
};
} // namespace aeris
