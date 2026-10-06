#include "helpers.h"
#include <QStringDecoder>
namespace aeris::imports {
struct CsvReader {
    QVector<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;
    bool closed = false;
    void finishField() {
        row.append(field);
        field.clear();
        closed = false;
    }
    void finishRow() {
        if (row.isEmpty() && field.isEmpty() && !closed)
            return;
        finishField();
        rows.append(row);
        row.clear();
        require(rows.size() <= Importer::MaxEntries + 1, "Too many CSV accounts.");
    }
    void character(QChar c) {
        require(!closed || c == ',' || c == '\n' || c == '\r', "Malformed CSV quoting.");
        if (c == ',')
            finishField();
        else if (c == '\n')
            finishRow();
        else if (c == '\r')
            return;
        else if (c == '"') {
            require(field.isEmpty(), "Malformed CSV quote.");
            quoted = true;
        } else
            field += c;
    }
    void parse(const QString &s) {
        for (qsizetype i = 0; i < s.size(); ++i) {
            auto c = s.at(i);
            if (!quoted) {
                character(c);
                continue;
            }
            if (c != '"') {
                field += c;
                continue;
            }
            if (i + 1 < s.size() && s.at(i + 1) == '"') {
                field += '"';
                ++i;
            } else {
                quoted = false;
                closed = true;
            }
        }
        require(!quoted, "Unterminated CSV field.");
        if (!field.isEmpty() || !row.isEmpty() || closed)
            finishRow();
    }
};
static void androidMetadata(const ImportResult &result, const QStringList &row, qsizetype previous) {
    if (row.size() != 9 || result.entries.size() == previous)
        return;
    // Android's exporter writes these three fields beyond its six-column header.
    const auto &entry = result.entries.last();
    require(row[6] == row[3] && row[7] == QString::number(entry->period) &&
                row[8] == QString::number(entry->digits),
            "Inconsistent Android CSV metadata.");
}
static void csvAccount(ImportResult &result, const QStringList &row, bool ios) {
    const int expected = ios ? 11 : 6;
    require(row.size() == expected || (!ios && row.size() == 9), "Malformed CSV row.");
    require(QStringList{"login", "1"}.contains(row[2]), "Unsupported Bitwarden CSV item type.");
    if (ios)
        require(row[5].isEmpty() && row[9].isEmpty(), "Password-manager CSVs are outside this release.");
    const auto &value = row[ios ? 10 : 5];
    auto previous = result.entries.size();
    if (value.contains("://"))
        uri(result, value);
    else
        append(result, "totp", row[3], ios ? row[8] : QString{}, value, "SHA1", 6, 30);
    androidMetadata(result, row, previous);
}
ImportResult csv(const QByteArray &data) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(data);
    require(!decoder.hasError(), "CSV is not valid UTF-8.");
    CsvReader reader;
    reader.parse(text);
    require(reader.rows.size() >= 2, "Empty CSV export.");
    auto header = reader.rows.takeFirst();
    const QStringList android{"folder", "favorite", "type", "name", "login_uri", "login_totp"};
    const QStringList ios{"folder",   "favorite",  "type",           "name",           "notes",     "fields",
                          "reprompt", "login_uri", "login_username", "login_password", "login_totp"};
    require(header == android || header == ios, "Unrecognized Bitwarden Authenticator CSV header.");
    ImportResult result;
    result.source = "Bitwarden Authenticator";
    for (const auto &row : reader.rows)
        csvAccount(result, row, header == ios);
    return result;
}
} // namespace aeris::imports
