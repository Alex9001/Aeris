#include "core/otp.h"
#include "import/importer.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
using namespace aeris;
class ImportTest final : public QObject {
    Q_OBJECT
  private slots:
    void fixtures_data();
    void fixtures();
    void limits();
    void numericBounds();
    void passwordAndFormat();
    void sourceUntouched();
};
static QByteArray readFixture(const QString &name) {
    QFile f(QString(FIXTURE_DIR) + "/" + name);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Missing synthetic fixture");
    return f.readAll();
}
void ImportTest::fixtures_data() {
    QTest::addColumn<QJsonObject>("spec");
    for (const auto &v : QJsonDocument::fromJson(readFixture("manifest.json")).array()) {
        auto o = v.toObject();
        QTest::newRow(qPrintable(o["name"].toString())) << o;
    }
}
static ImportOptions optionsFor(const QJsonObject &spec) {
    ImportOptions options;
    if (spec["password"].toBool())
        options.password = secret(QString::fromUtf8("synthetic päss🔐").toUtf8());
    if (spec["password"].isString())
        options.password = secret("wrong password");
    if (spec["hint"] == "current")
        options.hint = FormatHint::AndOtpCurrent;
    if (spec["hint"] == "legacy")
        options.hint = FormatHint::AndOtpLegacy;
    return options;
}
void ImportTest::fixtures() {
    QFETCH(QJsonObject, spec);
    QVector<ImportFile> files;
    for (const auto &name : spec["files"].toArray())
        files.append({readFixture(name.toString()), false});
    auto options = optionsFor(spec);
    if (spec["fail"].toBool()) {
        QVERIFY_THROWS_EXCEPTION(Error, Importer::parse(files, options));
        return;
    }
    auto result = Importer::parse(files, options);
    QCOMPARE(result.entries.size(), spec["count"].toInt(1));
    QCOMPARE(result.unsupported.size(), spec["unsupported"].toInt(0));
    QVERIFY(!result.source.isEmpty());
    QVERIFY(!OtpEngine::at(*result.entries.first(), 59).text.isEmpty());
    QCOMPARE(result.entries.first()->key->bytes(), QByteArray("12345678901234567890"));
}
void ImportTest::limits() {
    QVERIFY_THROWS_EXCEPTION(Error, Importer::parse({{QByteArray(Importer::MaxFile + 1, 'x'), false}}));
    QVERIFY_THROWS_EXCEPTION(Error, Importer::parse(QVector<ImportFile>(101)));
    auto o = QJsonDocument::fromJson(readFixture("ente-encrypted.json")).object();
    auto k = o["kdfParams"].toObject();
    k["memLimit"] = 2147483648.0;
    o["kdfParams"] = k;
    ImportOptions options;
    options.password = secret("pw");
    QVERIFY_THROWS_EXCEPTION(Error, Importer::parse({{QJsonDocument(o).toJson(), false}}, options));
}
void ImportTest::passwordAndFormat() {
    try {
        Importer::parse({{readFixture("aegis-encrypted.json"), false}});
        QFAIL("Expected password prompt");
    } catch (const Error &e) {
        QCOMPARE(e.kind(), ErrorKind::PasswordRequired);
    }
    try {
        Importer::parse({{readFixture("andotp-current.bin"), false}});
        QFAIL("Expected format prompt");
    } catch (const Error &e) {
        QCOMPARE(e.kind(), ErrorKind::Ambiguous);
    }
}
void ImportTest::numericBounds() {
    const QByteArray prefix = "otpauth://totp/test?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&";
    for (const auto &parameter : {"digits=-4294967290", "period=-4294967266", "period=1099511627777"})
        QVERIFY_THROWS_EXCEPTION(Error, Importer::parse({{prefix + parameter, false}}));
}
void ImportTest::sourceUntouched() {
    QTemporaryDir directory;
    auto path = directory.filePath("export.txt");
    auto bytes = readFixture("links.txt");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(bytes);
    f.close();
    QCOMPARE(Importer::read({path}).entries.size(), 1);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), bytes);
}
QTEST_GUILESS_MAIN(ImportTest)
#include "test_import.moc"
