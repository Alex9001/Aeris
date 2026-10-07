Aeris is MIT licensed. Export adapters are independently written; upstream
format references are documented in docs/formats.md. No authenticator source
implementation is incorporated.

Bundled runtime dependencies retain their own licenses:

Qt 6 (Core, GUI, Widgets, Concurrent, DBus and platform plugins): LGPL-3.0-only
or GPL-3.0-only or commercial. Aeris uses the LGPL option and dynamic linking.
QtKeychain: BSD-3-Clause. OpenSSL 3: Apache-2.0.
libsodium: ISC. libzip: BSD-3-Clause. ZXing-C++: Apache-2.0.
Protocol Buffers (protobuf-lite): BSD-3-Clause.

Full license texts, dependency versions and source locations accompany the
packages under licenses/ and in the SPDX SBOM. Qt libraries may be replaced
with compatible modified builds; do not restrict reverse engineering for
that purpose. Qt source: https://download.qt.io/archive/qt/
QtKeychain: https://github.com/frankosterfeld/qtkeychain
OpenSSL: https://github.com/openssl/openssl
libsodium: https://github.com/jedisct1/libsodium
libzip: https://github.com/nih-at/libzip
ZXing-C++: https://github.com/zxing-cpp/zxing-cpp
protobuf: https://github.com/protocolbuffers/protobuf

Transitive dependencies are enumerated in each package's SBOM and license
bundle. Build tools and synthetic fixture generators are not app runtime
components.

## Service logo artwork

The bundled standard PNG collection is by [selfh.st/icons](https://selfh.st/icons),
from [selfhst/icons revision 2053b70b283ffed5f2cc1424d1e17d9c554a846d](https://github.com/selfhst/icons/tree/2053b70b283ffed5f2cc1424d1e17d9c554a846d),
under Creative Commons Attribution 4.0 International (CC-BY-4.0).
The complete license is in `licenses/selfhst-icons-CC-BY-4.0.txt`.
Aeris resized the standard artwork to at most 128×128 pixels and optimized PNG
encoding, preserving aspect ratio, alpha and colors. Source and derived asset
SHA-256 checksums are in `assets/brands/catalog.json`. Logos and trademarks
belong to their respective owners; inclusion does not imply endorsement.
Aeris's implementation remains MIT licensed.

## Supplemental service logos

Additional service logos come from the [2FA Directory](https://2fa.directory)
contributors at revision `92d901f308ed6adc07b2415377b011c7dd544b13` and from the
Rocket Money, tastytrade, MySonicWall and DWService websites. The images belong
to their respective copyright and trademark owners and are used for service
identification. They are not covered by Aeris's MIT license or the selfh.st
collection's CC-BY-4.0 license. The 2FA Directory explicitly excludes its images
from its repository's MIT license. See `licenses/brand-supplement-NOTICE.md`
and the retained upstream license/image policy for attribution and details.
Exact source URLs, original files and SHA-256 checksums are recorded in
`assets/brand-sources`; output checksums are in `assets/brands/catalog.json`.
The artwork is rasterized/resized to at most 128×128 pixels, preserving its
colors and proportions, and bundled for offline use.

## Demo video soundtrack

`docs/media/aeris-demo-14s.mp4` includes an excerpt of **Classical 7** by
**Jonny S.**, Mixkit recording 714. The soundtrack retains its third-party
copyright and is not covered by Aeris's MIT license. The finished synchronized
video and its documentation accompany the source archive; the music is not
bundled in the application. See [video provenance and music license](docs/media/aeris-demo-14s.md)
for source links, the Mixkit Stock Music Free License, and reuse restrictions.
