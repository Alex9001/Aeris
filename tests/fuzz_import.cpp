#include "import/importer.h"
#include <QCoreApplication>
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
    static QCoreApplication app(*argc, *argv);
    return 0;
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536)
        return 0;
    try {
        aeris::Importer::parse({{QByteArray(reinterpret_cast<const char *>(data), qsizetype(size)), false}});
    } catch (const aeris::Error &) {
    }
    return 0;
}
