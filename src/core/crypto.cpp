#include "crypto.h"
#include "error.h"
#include <QRegularExpression>
#include <limits>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <sodium.h>
namespace aeris {
void wipe(QByteArray &b) {
    if (!b.isEmpty())
        sodium_memzero(b.data(), size_t(b.size()));
    b.clear();
}
SecretPtr secret(QByteArray b) {
    return std::make_shared<Secret>(std::move(b));
}
namespace crypto {
using Context = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
static const EVP_MD *digest(const QString &a) {
    if (a == "SHA1")
        return EVP_sha1();
    if (a == "SHA256")
        return EVP_sha256();
    if (a == "SHA512")
        return EVP_sha512();
    throw Error("Unsupported hash algorithm.");
}
QByteArray random(int size) {
    QByteArray out(size, '\0');
    require(RAND_bytes(reinterpret_cast<unsigned char *>(out.data()), size) == 1, "Random generator failed.");
    return out;
}
QByteArray base32(const QString &text) {
    auto s = text.toUpper();
    s.remove(' ');
    s.remove('-');
    require(s.size() <= 2048, "Secret exceeds the supported size.");
    static const QRegularExpression pattern("^[A-Z2-7]+={0,6}$");
    require(pattern.match(s).hasMatch(), "Malformed base32 secret.");
    auto raw = s.split('=').first();
    require(QList<int>{0, 2, 4, 5, 7}.contains(raw.size() % 8), "Malformed base32 length.");
    if (s.contains('='))
        require(s.size() % 8 == 0, "Malformed base32 padding.");
    QByteArray out;
    quint32 buffer = 0;
    int bits = 0;
    const QByteArray alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567");
    for (auto c : raw.toLatin1()) {
        buffer = (buffer << 5) | quint32(alphabet.indexOf(c));
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(char((buffer >> bits) & 255));
        }
    }
    require((buffer & ((1U << bits) - 1U)) == 0, "Malformed base32 trailing bits.");
    require(!out.isEmpty(), "Empty secret.");
    return out;
}
QByteArray base64(const QString &text) {
    auto r = QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    require(bool(r), "Malformed base64 data.");
    return r.decoded;
}
QByteArray hex(const QString &text) {
    static const QRegularExpression pattern("^(?:[a-fA-F0-9]{2})*$");
    require(pattern.match(text).hasMatch(), "Malformed hexadecimal data.");
    return QByteArray::fromHex(text.toLatin1());
}
QByteArray hmac(const QByteArray &key, const QByteArray &data, const QString &algorithm) {
    QByteArray out(EVP_MAX_MD_SIZE, '\0');
    unsigned int length = 0;
    require(HMAC(digest(algorithm), key.constData(), int(key.size()),
                 reinterpret_cast<const unsigned char *>(data.constData()), size_t(data.size()),
                 reinterpret_cast<unsigned char *>(out.data()), &length) != nullptr,
            "OTP computation failed.");
    out.resize(length);
    return out;
}
QByteArray hash(const QByteArray &data) {
    QByteArray out(32, '\0');
    unsigned int size = 0;
    require(EVP_Digest(data.constData(), size_t(data.size()), reinterpret_cast<unsigned char *>(out.data()),
                       &size, EVP_sha256(), nullptr) == 1,
            "Hash computation failed.");
    return out;
}
SecretPtr pbkdf(const QByteArray &password, const QByteArray &salt, int iterations,
                const QString &algorithm) {
    require(iterations > 0 && iterations <= 10000000, "PBKDF work requirement exceeds supported limits.");
    require(!salt.isEmpty() && salt.size() <= 1024, "Invalid KDF salt.");
    QByteArray key(32, '\0');
    require(PKCS5_PBKDF2_HMAC(password.constData(), int(password.size()),
                              reinterpret_cast<const unsigned char *>(salt.constData()), int(salt.size()),
                              iterations, digest(algorithm), 32,
                              reinterpret_cast<unsigned char *>(key.data())) == 1,
            "Key derivation failed.");
    return secret(std::move(key));
}
SecretPtr scrypt(const QByteArray &password, const QByteArray &salt, quint64 n, quint64 r, quint64 p) {
    require(n >= 2 && n <= 1048576 && (n & (n - 1)) == 0, "Unsupported scrypt cost.");
    require(r >= 1 && r <= 32 && p >= 1 && p <= 16, "Unsupported scrypt parameters.");
    require(n * r <= 2097152 && n * r * p <= 8388608, "scrypt work requirement exceeds supported limits.");
    require(salt.size() >= 8 && salt.size() <= 1024, "Invalid scrypt salt.");
    QByteArray key(32, '\0');
    require(EVP_PBE_scrypt(password.constData(), size_t(password.size()),
                           reinterpret_cast<const unsigned char *>(salt.constData()), size_t(salt.size()), n,
                           r, p, 300ULL * 1024 * 1024, reinterpret_cast<unsigned char *>(key.data()),
                           32) == 1,
            "Key derivation failed.");
    return secret(std::move(key));
}
SecretPtr argon(const QByteArray &password, const QByteArray &salt, quint64 ops, quint64 memory) {
    require(salt.size() == crypto_pwhash_SALTBYTES, "Invalid Argon2 salt.");
    require(ops >= 1 && ops <= 10, "Unsupported Argon2 work requirement.");
    require(memory >= 8192 && memory <= 1073741824 && memory * ops <= 4294967296ULL,
            "Argon2 memory requirement exceeds supported limits.");
    QByteArray key(32, '\0');
    require(crypto_pwhash(reinterpret_cast<unsigned char *>(key.data()), 32, password.constData(),
                          size_t(password.size()), reinterpret_cast<const unsigned char *>(salt.constData()),
                          ops, size_t(memory), crypto_pwhash_ALG_ARGON2ID13) == 0,
            "Insufficient memory for key derivation.");
    return secret(std::move(key));
}
static Context context(const QByteArray &key, const QByteArray &nonce, bool encrypt) {
    require(key.size() == 32 && nonce.size() == 12, "Invalid encryption parameters.");
    Context ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    require(bool(ctx), "Unable to allocate cipher.");
    require(EVP_CipherInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                              reinterpret_cast<const unsigned char *>(key.constData()),
                              reinterpret_cast<const unsigned char *>(nonce.constData()), int(encrypt)) == 1,
            "Unable to initialize cipher.");
    return ctx;
}
static void associated(EVP_CIPHER_CTX *ctx, const QByteArray &aad) {
    int n = 0;
    if (!aad.isEmpty())
        require(EVP_CipherUpdate(ctx, nullptr, &n, reinterpret_cast<const unsigned char *>(aad.constData()),
                                 int(aad.size())) == 1,
                "Invalid associated data.");
}
QByteArray seal(const QByteArray &plain, const QByteArray &key, const QByteArray &nonce,
                const QByteArray &aad) {
    auto ctx = context(key, nonce, true);
    associated(ctx.get(), aad);
    QByteArray out(plain.size() + 32, '\0');
    int n = 0;
    int tail = 0;
    require(EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &n,
                              reinterpret_cast<const unsigned char *>(plain.constData()),
                              int(plain.size())) == 1,
            "Encryption failed.");
    require(EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data() + n), &tail) == 1,
            "Encryption failed.");
    out.resize(n + tail + 16);
    require(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, out.data() + n + tail) == 1,
            "Encryption failed.");
    return out;
}
QByteArray open(const QByteArray &cipher, const QByteArray &key, const QByteArray &nonce,
                const QByteArray &aad) {
    require(cipher.size() >= 16, "Truncated encrypted data.");
    auto ctx = context(key, nonce, false);
    associated(ctx.get(), aad);
    QByteArray out(cipher.size(), '\0');
    int n = 0;
    int tail = 0;
    require(EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &n,
                              reinterpret_cast<const unsigned char *>(cipher.constData()),
                              int(cipher.size() - 16)) == 1,
            "Decryption failed.");
    auto tag = cipher.right(16);
    require(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag.data()) == 1,
            "Invalid authentication tag.");
    if (EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data() + n), &tail) != 1) {
        wipe(out);
        throw Error("Wrong password or damaged encrypted data.");
    }
    out.resize(n + tail);
    return out;
}
QByteArray enteOpen(const QByteArray &cipher, const QByteArray &key, const QByteArray &header) {
    require(key.size() == 32 && header.size() == crypto_secretstream_xchacha20poly1305_HEADERBYTES,
            "Invalid Ente encryption parameters.");
    require(cipher.size() >= crypto_secretstream_xchacha20poly1305_ABYTES, "Truncated Ente export.");
    crypto_secretstream_xchacha20poly1305_state state;
    require(crypto_secretstream_xchacha20poly1305_init_pull(
                &state, reinterpret_cast<const unsigned char *>(header.constData()),
                reinterpret_cast<const unsigned char *>(key.constData())) == 0,
            "Invalid Ente stream.");
    QByteArray out(cipher.size(), '\0');
    unsigned long long length = 0;
    unsigned char tag = 0;
    auto status = crypto_secretstream_xchacha20poly1305_pull(
        &state, reinterpret_cast<unsigned char *>(out.data()), &length, &tag,
        reinterpret_cast<const unsigned char *>(cipher.constData()), size_t(cipher.size()), nullptr, 0);
    sodium_memzero(&state, sizeof state);
    if (status != 0 || (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL &&
                        tag != crypto_secretstream_xchacha20poly1305_TAG_MESSAGE)) {
        wipe(out);
        throw Error("Wrong password or damaged Ente export.");
    }
    out.resize(qsizetype(length));
    return out;
}
} // namespace crypto
} // namespace aeris
