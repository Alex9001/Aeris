#pragma once
#include "core/entry.h"
#include "core/error.h"
#include <QStringList>
namespace aeris {
struct Unsupported {
    QString label;
    QString reason;
};
struct ImportResult {
    Entries entries;
    QString source;
    QVector<Unsupported> unsupported;
};
enum class FormatHint { Auto, AndOtpCurrent, AndOtpLegacy };
struct ImportOptions {
    SecretPtr password;
    FormatHint hint = FormatHint::Auto;
};
struct ImportFile {
    QByteArray data;
    bool image = false;
};
class Importer final {
  public:
    static constexpr qsizetype MaxFile = 16 * 1024 * 1024;
    static constexpr qsizetype MaxEntries = 10000;
    static ImportResult parse(const QVector<ImportFile> &files, const ImportOptions &options = {});
    static ImportResult read(const QStringList &paths, const ImportOptions &options = {});
};
} // namespace aeris
