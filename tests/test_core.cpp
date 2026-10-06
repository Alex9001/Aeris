#include "core/error.h"
#include "core/otp.h"
#include <QtTest>
using namespace aeris;
class CoreTest final : public QObject {
    Q_OBJECT
  private slots:
    void rfc_data();
    void rfc();
    void rollover();
    void steam();
    void cryptoRoundtrip();
    void strictDecoding();
    void kdfBounds();
};
void CoreTest::rfc_data() {
    QTest::addColumn<qint64>("time");
    QTest::addColumn<QString>("algorithm");
    QTest::addColumn<QByteArray>("key");
    QTest::addColumn<QString>("expected");
    const QList<qint64> times{59, 1111111109, 1111111111, 1234567890, 2000000000, 20000000000LL};
    const QStringList algorithms{"SHA1", "SHA256", "SHA512"};
    const QList<QByteArray> keys{"12345678901234567890", "12345678901234567890123456789012",
                                 "1234567890123456789012345678901234567890123456789012345678901234"};
    const QStringList expected{"94287082", "46119246", "90693936", "07081804", "68084774", "25091201",
                               "14050471", "67062674", "99943326", "89005924", "91819424", "93441116",
                               "69279037", "90698825", "38618901", "65353130", "77737706", "47863826"};
    for (int t = 0; t < times.size(); ++t)
        for (int a = 0; a < 3; ++a)
            QTest::newRow(qPrintable(QString::number(t) + algorithms[a]))
                << times[t] << algorithms[a] << keys[a] << expected[t * 3 + a];
}
void CoreTest::rfc() {
    QFETCH(qint64, time);
    QFETCH(QString, algorithm);
    QFETCH(QByteArray, key);
    QFETCH(QString, expected);
    Entry e("", "", key, algorithm, 8);
    QCOMPARE(OtpEngine::at(e, time).text, expected);
}
void CoreTest::rollover() {
    qint64 now = 59;
    OtpEngine engine([&now] { return now; });
    Entry e("", "", "12345678901234567890");
    auto before = engine.current(e);
    QCOMPARE(before.remaining, 1);
    now = 60;
    auto after = engine.current(e);
    QCOMPARE(after.remaining, 30);
    QVERIFY(before.text != after.text);
    now = 29;
    QCOMPARE(engine.current(e).remaining, 1);
    now = 59;
    QCOMPARE(engine.current(e).text, before.text);
    Entry custom("", "", "12345678901234567890", "SHA1", 8, 45);
    QCOMPARE(OtpEngine::at(custom, 89).remaining, 1);
    QCOMPARE(OtpEngine::at(custom, 90).remaining, 45);
    QVERIFY_THROWS_EXCEPTION(Error, OtpEngine::at(e, -1));
}
void CoreTest::steam() {
    Entry e("Steam", "synthetic", "12345678901234567890", "SHA1", 5, 30, TokenType::Steam);
    QCOMPARE(OtpEngine::at(e, 0).text, QString("GG5F5"));
    QCOMPARE(OtpEngine::at(e, 59).text, QString("PV9M4"));
    QCOMPARE(OtpEngine::at(e, 1111111109).text, QString("PY4YB"));
}
void CoreTest::cryptoRoundtrip() {
    auto k = crypto::random(32), n = crypto::random(12);
    QByteArray plain = "synthetic secret";
    auto cipher = crypto::seal(plain, k, n, "context");
    QCOMPARE(crypto::open(cipher, k, n, "context"), plain);
    QVERIFY_THROWS_EXCEPTION(Error, crypto::open(cipher, k, n, "other context"));
    cipher[0] = char(cipher.at(0) ^ 1);
    QVERIFY_THROWS_EXCEPTION(Error, crypto::open(cipher, k, n, "context"));
}
void CoreTest::strictDecoding() {
    QCOMPARE(crypto::base32("MZXW6==="), QByteArray("foo"));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::base32("MZXW7==="));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::base32("!secret"));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::base64("@abc"));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::hex("abc"));
}
void CoreTest::kdfBounds() {
    QVERIFY_THROWS_EXCEPTION(Error, crypto::argon("pw", QByteArray(16, 's'), 100, 8192));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::argon("pw", QByteArray(16, 's'), 2, 1073741825));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::scrypt("pw", "12345678", 1ULL << 40, 8, 1));
    QVERIFY_THROWS_EXCEPTION(Error, crypto::pbkdf("pw", "salt", 10000001, "SHA1"));
}
QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
