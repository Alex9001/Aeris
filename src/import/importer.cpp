#include "helpers.h"
#include <QFile>
#include <QImageReader>
namespace aeris {
static bool isImage(const QByteArray &data) {
    return data.startsWith("\x89PNG") || data.startsWith("\xff\xd8\xff");
}
static ImportResult one(const QByteArray &bytes, const ImportOptions &options) {
    auto data = bytes.trimmed();
    if (data.startsWith("\xef\xbb\xbf"))
        data = data.mid(3);
    if (data.startsWith('{') || data.startsWith('['))
        return imports::jsonImport(imports::json(data), options);
    if (bytes.startsWith("PK\x03\x04"))
        return imports::raivoZip(bytes, options);
    if (data.startsWith("otpauth://"))
        return imports::text(data);
    if (data.startsWith("folder,") || data.startsWith("\"folder\""))
        return imports::csv(data);
    require(!data.startsWith("-----BEGIN PGP"), "PGP-encrypted andOTP exports are outside this release.");
    return imports::andOtp(bytes, options);
}
ImportResult Importer::parse(const QVector<ImportFile> &files, const ImportOptions &options) {
    require(!files.isEmpty() && files.size() <= 100, "Select between 1 and 100 export files.");
    QStringList links;
    qsizetype total = 0;
    for (const auto &file : files) {
        require(file.data.size() <= MaxFile, "Export exceeds the 16 MiB limit.");
        total += file.data.size();
        require(total <= 64 * 1024 * 1024, "Selected files exceed 64 MiB.");
        if (file.image || isImage(file.data)) {
            links.append(imports::qr(file.data));
            continue;
        }
        if (file.data.trimmed().startsWith("otpauth-migration://")) {
            for (const auto &line : QString::fromUtf8(file.data).split('\n', Qt::SkipEmptyParts))
                links.append(line.trimmed());
        } else
            require(files.size() == 1, "Multiple files are supported only for Google migration batches.");
    }
    auto r = links.isEmpty() ? one(files.first().data, options) : imports::google(links);
    if (r.entries.isEmpty()) {
        QStringList reasons;
        for (const auto &entry : r.unsupported)
            reasons.append(entry.reason);
        reasons.removeDuplicates();
        throw Error("The export contains no supported accounts. Existing accounts have been kept. " +
                    reasons.join("; "));
    }
    return r;
}
ImportResult Importer::read(const QStringList &paths, const ImportOptions &options) {
    require(paths.size() <= 100, "Too many export files.");
    QVector<ImportFile> files;
    qsizetype total = 0;
    for (const auto &path : paths) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Unable to open export file.");
        require(file.size() <= MaxFile, "Export exceeds the 16 MiB limit.");
        auto data = file.read(MaxFile + 1);
        total += data.size();
        require(file.error() == QFileDevice::NoError && data.size() <= MaxFile,
                "Unable to read export within limits.");
        require(total <= 64 * 1024 * 1024, "Selected files exceed 64 MiB.");
        files.append({std::move(data), false});
    }
    try {
        auto result = parse(files, options);
        for (auto &f : files)
            wipe(f.data);
        return result;
    } catch (...) {
        for (auto &f : files)
            wipe(f.data);
        throw;
    }
}
} // namespace aeris
