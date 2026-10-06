#include "helpers.h"
#include <QtEndian>
#include <zip.h>
namespace aeris::imports {
ImportResult andOtp(const QByteArray &data, const ImportOptions &options) {
    if (options.hint == FormatHint::Auto)
        throw Error("This binary file has no format marker. Select the andOTP backup format.",
                    ErrorKind::Ambiguous);
    const auto &pw = password(options);
    SecretPtr key;
    int offset = 0;
    if (options.hint == FormatHint::AndOtpLegacy)
        key = secret(crypto::hash(pw));
    else {
        require(data.size() >= 44, "Truncated andOTP backup.");
        auto iterations = qFromBigEndian<quint32>(data.constData());
        require(iterations <= 10000000, "Unsupported andOTP KDF work requirement.");
        key = crypto::pbkdf(pw, data.mid(4, 12), int(iterations), "SHA1");
        offset = 16;
    }
    require(data.size() >= offset + 28, "Truncated andOTP backup.");
    auto plain = secret(crypto::open(data.mid(offset + 12), key->bytes(), data.mid(offset, 12)));
    auto doc = json(plain->bytes());
    require(doc.isArray(), "Malformed andOTP backup.");
    return records(doc.array());
}
static zip_uint64_t findRaivo(zip_t *archive) {
    const auto count = zip_get_num_entries(archive, 0);
    require(count >= 1 && count <= 64, "ZIP contains too many members.");
    zip_uint64_t match = 0;
    int found = 0;
    quint64 total = 0;
    for (zip_int64_t i = 0; i < count; ++i) {
        zip_stat_t st;
        zip_stat_init(&st);
        require(zip_stat_index(archive, zip_uint64_t(i), 0, &st) == 0, "Invalid ZIP directory.");
        require(st.size <= quint64(Importer::MaxFile), "ZIP member exceeds the 16 MiB limit.");
        total += st.size;
        if (QString::fromUtf8(st.name) == "raivo-otp-export.json") {
            match = zip_uint64_t(i);
            ++found;
        }
    }
    require(total <= 64 * 1024 * 1024, "ZIP expansion exceeds the 64 MiB limit.");
    require(found == 1, "ZIP must contain one raivo-otp-export.json member.");
    return match;
}
ImportResult raivoZip(const QByteArray &data, const ImportOptions &options) {
    zip_error_t error;
    zip_error_init(&error);
    auto *source = zip_source_buffer_create(data.constData(), zip_uint64_t(data.size()), 0, &error);
    require(source != nullptr, "Unable to read ZIP.");
    auto *raw = zip_open_from_source(source, ZIP_RDONLY, &error);
    zip_error_fini(&error);
    if (!raw) {
        zip_source_free(source);
        throw Error("Malformed ZIP archive.");
    }
    std::unique_ptr<zip_t, decltype(&zip_discard)> archive(raw, zip_discard);
    auto index = findRaivo(raw);
    zip_stat_t st;
    zip_stat_init(&st);
    require(zip_stat_index(raw, index, 0, &st) == 0, "Invalid ZIP member.");
    const char *pw = nullptr;
    if (st.encryption_method != ZIP_EM_NONE) {
        const auto &p = password(options);
        require(!p.contains('\0'), "ZIP passwords cannot contain a NUL character.");
        pw = p.constData();
    }
    std::unique_ptr<zip_file_t, decltype(&zip_fclose)> file(zip_fopen_index_encrypted(raw, index, 0, pw),
                                                            zip_fclose);
    require(bool(file), "Wrong password or damaged Raivo ZIP.");
    QByteArray bytes(qsizetype(st.size), '\0');
    zip_uint64_t done = 0;
    while (done < st.size) {
        auto n = zip_fread(file.get(), bytes.data() + done, st.size - done);
        if (n <= 0) {
            wipe(bytes);
            throw Error("Wrong password or damaged Raivo ZIP.");
        }
        done += zip_uint64_t(n);
    }
    char end = 0;
    require(zip_fread(file.get(), &end, 1) == 0, "Damaged Raivo ZIP member.");
    auto plain = secret(std::move(bytes));
    auto doc = json(plain->bytes());
    require(doc.isArray(), "Malformed Raivo export.");
    auto r = records(doc.array());
    require(r.source == "Raivo OTP", "ZIP does not contain a Raivo export.");
    return r;
}
} // namespace aeris::imports
