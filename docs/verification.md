# Verification record

Local verification completed on 2026-10-06 for Aeris 0.1.0. This is a Linux preflight, not a claim that the unrun release matrix has passed.

## Executed checks

- Release builds on the Artix host (Clang 23, Qt 6.11.2) and Ubuntu 24.04 container (Clang 18, Qt 6.4.2). Both pass all five CTest suites: core, imports, storage, UI and unavailable keyring.
- 77 synthetic import fixture cases cover every advertised plain/encrypted adapter, all four 2FAS schemas, wrong passwords, Unicode, corruption, unsupported tokens, unknown versions, empty inputs and Google multipart/PNG/JPEG exports. Additional tests exercise workload bounds, negative numeric fields, password/format prompts and source-file preservation.
- RFC 6238 vectors for all three hashes; independently calculated Steam reference codes; rollover, custom periods, forward/backward clock changes and copying across an expiry boundary.
- Replacement, journal/write/keyring failures, staged-key recovery, failed old-key retirement, interrupted deletion, deletion during pending cleanup, corrupt ciphertext and removal of abandoned encrypted staging files. Tests scan persisted files for synthetic secrets and account names, and verify complete removal after deletion. Source review found no secret/account/password logging in application code.
- A real desktop QtKeychain write/read/delete round trip succeeded using a new random test credential. The unavailable-keyring test uses an invalid session-bus address and confirms rejection with insecure fallback disabled.
- UI tests exercise search, Enter and Ctrl/Cmd+C, clipboard ownership including identical replacement text, 30-second expiry, malformed drag-and-drop, replacement cancellation, explicit HOTP exclusions and complete deletion. Light/dark palettes, long names, empty state and About were rendered and visually reviewed at 1× and 2× scale.
- Lizard **1.17.31** passes cyclomatic complexity ≤10 for maintained C++ functions. clang-tidy passes cognitive complexity ≤15. Generated protobuf and dependencies are excluded; Qt assertion macro expansions are ignored, with no blanket function suppressions.
- Ubuntu Clang 18 **ASan + UBSan** passes all five suites. The final 60-second libFuzzer parser campaign completed 59,748 inputs without a crash (61 seconds including shutdown). Fuzzing uses a 64 KiB input limit and a 2 GiB RSS ceiling; unit tests cover encrypted/KDF paths separately.
- The amd64 AppImage passes packaged startup on X11 (Xvfb) and Wayland (headless Weston), with bundled platform and Wayland integration plugins. Headless Mesa reports unavailable GPU acceleration and uses software rendering. The package has a validated SPDX SBOM and bundled dependency notices.
- actionlint 1.7.7 validates both workflows; Python scripts compile and shell packaging scripts pass Bash syntax checks.

The host's bleeding-edge system Qt/protobuf libraries were unsuitable for mixed instrumented/noninstrumented sanitizer builds (protobuf layout mismatch and an allocation mismatch inside system Qt). Sanitizer acceptance therefore uses the clean Ubuntu 24.04/Clang 18 dependency baseline used by CI. No sanitizer suppressions were added to hide those failures.

## Linux reference measurements

AMD Ryzen 7 5800H, x86_64 Artix Linux, kernel 7.2.7-zen1-1-zen, glibc 2.44, Clang 23 Release build, Qt 6.11.2. `scripts/benchmark.py --build build-release` uses Qt's offscreen platform and an isolated application-data directory. The first startup is discarded. CPU is process user+system time over a five-second idle interval, relative to one CPU core.

| Measurement | Observed |
| --- | --- |
| Warm empty startup, five runs | 147, 165, 141, 150, 155 ms |
| Load/show encrypted 1,000-account session | 135 ms |
| Filter 1,000 accounts | 0.616 ms |
| Idle CPU, 1,000 accounts | 0.4% |
| Normal session RSS, 1,000 accounts | 70,444 KiB (68.8 MiB) |
| Host executable, excluding shared dependencies | 533,744 bytes |
| Ubuntu amd64 AppImage, bundled runtime | 30,468,600 bytes (29.1 MiB) |
| Encrypted Ente fixture, Argon2id 64 MiB / 2 passes | 136 ms process wall time; 89,268 KiB peak process RSS |

The 1,000-account benchmark uses an in-memory credential backend with a real encrypted snapshot; system-keyring latency is excluded. The Ente peak was measured separately using child-process resource usage. A supported 1 GiB Ente KDF can consume much more memory. These are reference observations, not guarantees for every compositor, keyring or machine. Raw local results are in `build-release/benchmark.json` and `build-release/encrypted-import-measurement.json`.

## Reproduce

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-release --parallel 3
ctest --test-dir build-release --output-on-failure
.venv/bin/python scripts/lint.py --build build-release
python scripts/benchmark.py --build build-release
./build-release/keychain_probe
```

For sanitizer/fuzzer reproduction, use Ubuntu 24.04 and the exact commands in `.github/workflows/ci.yml`. The fixture generator dependencies are pinned in `scripts/requirements.txt`. `AERIS_QA_DIR` saves UI snapshots when running `test_ui`; use `QT_SCALE_FACTOR=2` for the high-DPI pass.

## File chooser freeze regression

A desktop trial exposed a freeze that startup-only package smoke tests missed. With the Ubuntu Qt 6.4.2 runtime on the newer KDE/Wayland host, opening the fallback file chooser spun in `QMimeDatabase::mimeTypeForFile` while obtaining file icons. A standalone chooser reproduced the same stack. Using generic file/folder icons avoided that lookup and the diagnostic chooser opened and closed normally.

The import chooser now supplies generic icons for the Qt fallback, while leaving native platform dialogs enabled. `UiTest::filePicker` opens the actual import chooser, waits for directory entries and multiple timer heartbeats, cancels it, and checks that the collection is unchanged. The test passes on the KDE/Wayland host with the bundled Qt 6.4.2 libraries (730 ms including test setup/cleanup). Release and sanitizer suites and complexity gates also pass. The earlier startup smoke tests alone did not cover this interaction.

## Interface refinement verification

The follow-up interface pass adds complete themes, thin controls, List/Compact/Cards layouts, offline account badges and transient in-place copy feedback. Tests cover all **27 theme/layout combinations**, readable search foreground/background contrast, malformed system palette recovery, saved appearance, repeatable/distinct account identities, card columns and minimum-width resizing. Copy feedback is checked for mouse and keyboard activation, including the native Wayland focus timing case. Screenshots were reviewed at 1× and 2× scale with synthetic accounts; see `docs/interface.md`.

All five release and ASan/UBSan suites pass after these changes; Lizard 1.17.31 and clang-tidy complexity gates pass. The Qt 6.4.2 packaged runtime also passes the System-theme presentation, Import chooser and copy-animation checks on the KDE/Wayland host. X11 and Wayland packaged startup smoke tests pass. Import/cryptographic behavior is unchanged; the earlier parser fuzz results remain the parser verification record.

A repeat of the same reference benchmark observed warm startup **122, 123, 117, 129, 116 ms**, 1,000-account load/show **138 ms**, filtering **0.401 ms**, idle CPU **0.2%**, and normal session RSS **72,240 KiB**. Raw results are in `build-release/benchmark-ui.json`; the original baseline table above remains as the initial measurement record.

## Remaining release acceptance

The Linux arm64 AppImage, both Windows installers/ZIPs, and universal macOS DMG have **not been built or executed locally**. Their native build, test, deployment, architecture and packaged smoke checks are configured in `release.yml` and must pass in a nonpublishing workflow dispatch before a release tag is published. Windows/macOS keychain prompts and native clipboard behavior still need interactive checks on those systems. No remote repository, release, AUR submission or website was created.

Synthetic compatibility tests are comprehensive for the implemented layouts, but do not replace user trials with current phone applications. Parser fuzzing is time-bounded and is not a security audit. OS swap, crash dumps and external clipboard managers are outside Aeris's memory/clipboard ownership guarantees; see the architecture document.

## About text layout

The About dialog reflows the bundled legal documents as Markdown paragraphs instead of double-wrapping their source line breaks. Separate MIT license and Dependencies tabs use a wider, resizable reading area. The regression test checks paragraph continuity, both documents' final text, horizontal overflow and scrolling to the end of the license. Tests passed on Qt 6.4.2 and 6.11.2, including 2× scaling; the rendered dialog was visually reviewed.

## Offline service logos

The service-logo pass bundles and validates all **2,949 standard PNGs** from
selfh.st/icons revision `2053b70b283ffed5f2cc1424d1e17d9c554a846d`. Every resource
is decoded with Qt and checked against its catalog SHA-256, alpha channel and
128×128 size bound. The catalog also records original PNG and upstream archive
checksums. The regeneration script rejects an archive with the wrong checksum.
The collection's CC-BY-4.0 attribution/license accompanies the MIT application
and is represented in the packaged SBOM.

Host Qt 6.11.2 and Ubuntu Qt 6.4.2 Release builds pass all five CTest suites;
the Ubuntu ASan/UBSan build also passes all five. The UI suite now includes
normalization, explicit aliases, permanent ambiguity rejection, misleading
partial names, unknown services, email non-inference, Steam override, duplicate
accounts, image decoding, cache size/scale keys and eviction at the 8 MiB
pixel budget. Pixel comparisons verify that missing and unreadable resources
paint exactly the existing badge. Lizard and clang-tidy complexity gates pass.

All 27 theme/layout combinations were rendered and visually reviewed at 1× and
2× scaling, including selected rows and copy feedback. Logos, original colors,
neutral backing and account accents remain legible. Updated screenshots are in
`docs/screenshots`; the full QA renders are in `build-release/qa-brands-1x` and
`build-release/qa-brands-2x`. The final high-DPI presentation/image/cache/copy
subset passes 32 checks.

The final benchmark measured **0.531 ms** to filter 1,000 accounts (under the
50 ms gate), **179 ms** for encrypted 1,000-account load/show, **0.2%** idle CPU,
and **73,760 KiB** RSS. Warm empty startup measured 151, 133, 141, 146 and 150 ms.
The full results are in `release-assets/linux-brand-icons-benchmark.json`.
These measurements ran on the same host as the earlier reference observations.

The rebuilt Linux amd64 AppImage is **60,336,632 bytes** and passes X11/Xvfb and
Wayland/Weston smoke tests. Its validated SBOM, bundled notices, source archive,
source manifest and release checksums have been refreshed. The workspace has no
Git metadata, so the source archive and manifest capture the exact local source;
no Git commit or remote publication was possible. Other-platform acceptance
remains as listed above. Token entries, imports and encrypted storage are unchanged.

## Additional issuer coverage

The follow-up adds **15 supplemental logos**, bringing the bundle to **2,964**,
and exact aliases for all 20 reported issuer strings: wordpress.org, monogodb,
wordpress.com, godaddy, patelco, fidelity, gog, chase, dreamhost,
rocket-mortgage, rocket-money, spaceship.com, zoho, intuit, rockstar, ubisoft,
tastyworks, mysonicwall, id.me and dwservice.net. The misspelling monogodb is an
explicit MongoDB alias; tastyworks maps to tastytrade artwork. GOG/Gogs and
Zoho/Zoho Mail remain separate. No substring, arbitrary-domain or account-email
matching was introduced.

Forty data-driven cases cover those strings and common service/domain aliases,
including cached model roles, case normalization, misleading suffixes, email
non-inference and image loading at 1×/2×. All 2,964 bundled PNGs pass catalog
checksum, decoding, alpha and dimension validation. The host and Ubuntu Release
builds pass all five CTest suites, including **88 UI checks**. The high-DPI
issuer/asset/filtering subset passes **44 checks**. Filtering 1,000 accounts
measured **0.545 ms** in the full host suite and **0.620 ms** in the 2× subset.
Lizard and clang-tidy pass. The artwork contact sheet was visually reviewed.
The application matching/rendering implementation was unchanged; this pass adds
artwork, aliases, regression coverage and offline preparation support.

Original supplemental files, source URLs/revisions and SHA-256 values are retained
in `assets/brand-sources`. Their ownership/attribution is recorded separately
from the MIT application and CC-BY-4.0 base collection. Offline regeneration is
byte-for-byte idempotent. The Linux amd64 AppImage, notices, SBOM, source archive,
source manifest and release checksums were refreshed for this coverage update;
packaged X11/Xvfb and Wayland/Weston startup smoke tests pass.

## Drag-and-drop account order

Accounts can be moved in List, Compact and Cards by dragging to an insertion
marker. Search remains active; destinations map to visible neighbors while
hidden accounts retain their relative order. Escape, release outside the view,
and no-op moves do not save or copy a code. Autoscroll runs only during a drag.
A successful save preserves the moved account's selection and keyboard focus.

A new `order` suite adds **18 checks** covering model move notifications,
persistent indexes and cached identities, duplicate labels with distinct
secrets, upward/downward moves, all layouts, filtered moves and hidden tails,
reopening the encrypted collection, clipboard preservation, cancellation,
no-op write suppression, edge scrolling, failed snapshot/journal/keyring writes,
subsequent successful retries, and committed saves with credential-cleanup
warnings. Failed saves leave the displayed order intact. The existing vault
array and transactional replacement path are used without a schema change.

All **six CTest suites** pass on the host Release build (Qt 6.11.2), Ubuntu
Release build (Qt 6.4.2), and Ubuntu ASan/UBSan build. The complete order suite
also passes at 2× scaling and on X11/Xvfb with Qt 6.4.2. The native test harness
explicitly activates its window before checking keyboard focus. Insertion
markers were visually reviewed in all three layouts at 1× and 2×. Lizard and
clang-tidy complexity gates pass. Existing import, clipboard and brand-image
regression tests remain passing.

The Linux amd64 AppImage was rebuilt and passed X11/Xvfb and Wayland/Weston
startup smoke tests. Because the prior AppImage was in use, the replacement was
built and tested in a staging directory, then installed by atomic rename without
stopping the running application. Restarting loads the new version. The SBOM,
source archive, manifest, checksums, interface documentation and local provenance
were refreshed for this change. Other-platform native acceptance remains pending.

## Order-save freeze correction

The worker-based keyring probe reproduced a stalled follow-up request with the
AppImage's Qt 6.4.2 / QtKeychain 0.14.2 libraries. The older probe ran every job on
the main thread and did not cover the application's changing worker threads.
QtKeychain jobs now live on the application event loop, with queued completion
back to their caller. Real-keyring tests now include write/read/delete on fresh
workers and repeated encrypted synthetic-vault import/load/reorder/delete.
These pass with both the host and bundled libraries; no real account collection
was used or modified by the tests.

The order suite now has **19 checks**. A deliberately blocked keyring write
verifies continued search and copying, prevention of overlapping reorders,
correct persisted order after completion, and deferred close without blocking
the event loop. All six suites pass in host Release, Ubuntu Release and Ubuntu
ASan/UBSan builds. The order suite also passes at 2× scaling and X11/Xvfb.
The rebuilt AppImage passes X11/Xvfb and Wayland/Weston startup smoke tests.
The 25-second keyring deadline retains unfinished jobs for late callbacks and
rejects additional keyring work until completion, preserving recovery journals.
Live desktop debugger attachment was denied by host ptrace policy; the original
window's exact stack could not be inspected.

Lizard and clang-tidy complexity gates pass. Filtering 1,000 accounts measured
0.376 ms (50 ms limit). The release SBOM, source archive, manifest, provenance
and checksums were refreshed, and the AppImage was replaced by atomic rename.
The previously running process is left untouched and must be restarted.

## Supplied A-Shield application icon

Replaced the master artwork with the user-supplied Satin A-Shield Authenticator
Icon.png, preserving it byte-for-byte. Regenerated nine transparent PNG sizes,
Windows ICO frames and the macOS ICNS container. All images decode; PNG sizes,
alpha and ICO frame sizes were checked. The small-size contact sheet and the
rebuilt About dialog were visually reviewed. The AppImage desktop icon matches
the new artwork. The focused About test and packaged X11/Wayland startup smoke
tests pass. Platform artwork is prepared for Windows/macOS; native binaries for
those platforms were not rebuilt. Release metadata and checksums were refreshed.

## High-resolution interface gallery

Replaced the four older previews with ten lossless PNG screenshots across all
nine themes and all three layouts. Light/Compact is the README's primary image;
the drag insertion and older copy-feedback screenshots were removed. Every
sample account is `user@example.com`, backed by synthetic secrets only.

The capture harness uses isolated Xvfb/Openbox sessions with native titlebars
and Qt's 2× display scale. Images are 2008 × 1360 or 2008 × 1680 pixels, captured
directly from the desktop without upscaling. All ten images were checked for
valid decoding, dimensions and complete titlebars; themes, text and layouts
were visually reviewed. Reproduction is documented in `docs/interface.md` and
`scripts/screenshots-linux.sh`. No application behavior or production vault data
was changed.

All six local CTest suites pass after the capture-harness changes. Lizard and
clang-tidy complexity checks, PNG validation and README link/render checks pass.
