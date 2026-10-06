#include "helpers.h"
#include "migration.pb.h"
#include <QBuffer>
#include <QImageReader>
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ReaderOptions.h>
#if __has_include(<ZXing/Barcode.h>)
#include <ZXing/Barcode.h>
#else
#include <ZXing/Result.h>
#endif
namespace aeris::imports {
QStringList qr(const QByteArray &data) {
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    auto format = reader.format();
    require(format == "png" || format == "jpeg" || format == "jpg",
            "Only PNG/JPEG migration screenshots are supported.");
    auto size = reader.size();
    require(size.isValid() && qint64(size.width()) * size.height() <= 25000000,
            "Image exceeds the 25 megapixel limit.");
    auto image = reader.read().convertToFormat(QImage::Format_Grayscale8);
    require(!image.isNull(), "Unable to decode image.");
    auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode).setTryHarder(true);
    auto results = ZXing::ReadBarcodes({image.constBits(), image.width(), image.height(),
                                        ZXing::ImageFormat::Lum, int(image.bytesPerLine())},
                                       options);
    QStringList links;
    for (const auto &result : results) {
        require(result.isValid(), "Unreadable QR code.");
        auto value = QString::fromStdString(result.text());
        require(value.startsWith("otpauth-migration://"),
                "Only Google migration QR codes can be imported. Enrollment QR codes are not supported.");
        links.append(value);
    }
    require(!links.isEmpty(), "No Google migration QR code found.");
    return links;
}
static migration::Batch batch(const QString &link) {
    QUrl url(link, QUrl::StrictMode);
    require(url.isValid() && url.scheme() == "otpauth-migration" && url.host() == "offline",
            "Malformed Google migration link.");
    QUrlQuery query(url);
    auto values = query.allQueryItemValues("data", QUrl::FullyDecoded);
    require(values.size() == 1, "Migration link must contain one payload.");
    auto bytes = crypto::base64(values.first());
    require(bytes.size() <= 1024 * 1024, "Migration payload exceeds supported size.");
    migration::Batch b;
    require(b.ParseFromArray(bytes.constData(), int(bytes.size())), "Malformed Google migration payload.");
    require(b.version() == 1, "Unsupported Google migration version.");
    require(b.size() >= 1 && b.size() <= 100 && b.index() >= 0 && b.index() < b.size(),
            "Invalid Google batch metadata.");
    return b;
}
static void googleEntry(ImportResult &r, const migration::Account &a) {
    auto label = QString::fromStdString(a.name());
    auto issuer = QString::fromStdString(a.issuer());
    if (a.type() != 2) {
        r.unsupported.append({issuer + " / " + label,
                              a.type() == 1 ? "HOTP (counter-based)" : "Unsupported Google token type"});
        return;
    }
    require(a.algorithm() >= 1 && a.algorithm() <= 3, "Unsupported algorithm in Google account.");
    require(a.digits() == 1 || a.digits() == 2, "Unsupported digits in Google account.");
    if (label.startsWith(issuer + ':'))
        label = label.mid(issuer.size() + 1);
    const QStringList algorithms{"SHA1", "SHA256", "SHA512"};
    r.entries.append(std::make_shared<Entry>(issuer, label, QByteArray::fromStdString(a.secret()),
                                             algorithms.at(a.algorithm() - 1), a.digits() == 1 ? 6 : 8, 30));
}
ImportResult google(const QStringList &links) {
    require(!links.isEmpty() && links.size() <= 100, "Invalid migration batch size.");
    QMap<int, migration::Batch> batches;
    auto first = batch(links.first());
    for (const auto &link : links) {
        auto b = batch(link);
        require(b.id() == first.id() && b.size() == first.size(),
                "Select only parts of the same Google export.");
        require(!batches.contains(b.index()), "Duplicate Google export part.");
        batches.insert(b.index(), std::move(b));
    }
    require(batches.size() == first.size(), "Incomplete Google export. Select every part together.");
    ImportResult r;
    r.source = "Google Authenticator";
    for (const auto &b : batches) {
        for (const auto &a : b.accounts()) {
            require(r.entries.size() + r.unsupported.size() < Importer::MaxEntries, "Too many accounts.");
            googleEntry(r, a);
        }
    }
    return r;
}
} // namespace aeris::imports
