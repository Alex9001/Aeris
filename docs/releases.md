# Desktop releases

GitHub Actions are disabled to conserve quota and must only be enabled or run on explicit request. The retained workflows have no automatic push, pull-request, or tag triggers. Manual workflow dispatch builds a nonpublishing preflight; a manually requested run against a `v*` tag matching `CMakeLists.txt` can publish after its quality and packaging gates pass.

The first public release is built locally. Its verified binary targets are **Linux x86-64**, **Artix x86-64**, **Windows x64** and **Intel macOS**, with source archives, SPDX SBOMs, checksums and local build provenance. The platform matrix below also describes additional architectures that remain pending acceptance.

| Platform | Outputs | Verification |
| --- | --- | --- |
| Linux amd64 / arm64 | AppImage per architecture | Native tests; packaged X11 under Xvfb and Wayland under headless Weston |
| Windows amd64 / arm64 | Per-user NSIS installer and portable ZIP | Native tests; extracted ZIP, silent install, installed app and uninstall smoke tests |
| macOS Intel + Apple silicon | One universal DMG | Native tests per slice; merge/audit every Mach-O; launch both slices and app from mounted DMG |
| Artix x86-64 | Native `.pkg.tar.zst` and source PKGBUILD | Clean standard-repository build, installation, upgrade/removal, X11/Wayland and KWallet checks |

Linux release baseline is Ubuntu 24.04; both Qt XCB and Wayland plugins are explicitly bundled. AppImages do not bundle the platform keyring daemon. Windows and macOS use Qt 6.10.2 and pinned vcpkg dependencies, plus independently built QtKeychain 0.17.0. On macOS, each native bundle is deployed before merging, with all non-system dylibs required to resolve inside the bundle. Ad-hoc signatures are applied for Apple silicon execution; packages have no publisher signing certificate or notarization.

Actions are pinned by commit. vcpkg's baseline is pinned in `vcpkg.json`. linuxdeploy's continuous artifacts are accepted only with the pinned SHA-256; if upstream replaces them, the workflow intentionally fails until hashes are reviewed. Apt dependencies follow Ubuntu security updates. Full binary reproducibility is not claimed.

## Local packaging

### BuildLab Windows and macOS

The local BuildLab at `/run/media/user/edat/VMs` provides verified native x64 builders.
Run either platform independently, or both in sequence:

```sh
python3 scripts/build-local.py --target all
python3 scripts/build-local.py --target windows-x64
python3 scripts/build-local.py --target macos-x64
```

The driver checks BuildLab readiness and snapshots the current checkout, including
uncommitted changes. `--buildlab PATH` (or `AERIS_BUILDLAB`) selects another lab;
`--output PATH` selects an output directory. The default is `release-assets/`.
Jobs allow six hours each and bounded ten-minute waits for the shared lock and RAM.
Use `--timeout`, `--wait-lock`, and `--wait-memory` to change those limits.
For debugging one VM target, `--keep` preserves the guest after completion;
inspect it with BuildLab, then run `buildlab reset TARGET` before another build.

Disposable guests install Qt 6.10.2 with aqtinstall 3.3.0, use the repository's pinned
vcpkg baseline, and build QtKeychain at its pinned revision. SDKs, downloads and
vcpkg binary packages persist under `BUILDLAB_CACHE/aeris`; the workstation and
verified builder bases are not used for dependency installation. Syft 1.51.0 and
portable NSIS 3.13 downloads are checked against pinned SHA-256 hashes.
macOS also bootstraps pinned GNU m4/autoconf/automake/libtool and macro archives
into the cache when the base image lacks vcpkg's autotools prerequisites.
Native host tools use the target triplet, avoiding a duplicate protobuf build
for macOS. The Mac triplet builds release dependencies only.
The native driver defaults to two compiler jobs on Windows and one on the
single-vCPU Mac builder.

Windows produces `aeris_windows_amd64.zip`, `aeris_windows_amd64-setup.exe` and an
SPDX SBOM. macOS produces `aeris_macos_x64.dmg`, a deployed app archive and an SPDX
SBOM. Each target runs native CTest suites, the synthetic system-keychain/vault
probe and package acceptance before its files are promoted to the output directory.
macOS DMG verification includes Intel binary slices, ad-hoc signatures and launching
the application from the mounted image with developer library/plugin paths removed.
Its offscreen smoke plugin is explicitly deployed and relocated. Windows acceptance covers portable launch,
installation, installed launch and removal while retaining unrelated files.
Windows bundles the application-local MSVC redistributable DLLs and tests both
offscreen and desktop launches with the developer SDK removed from PATH.
It deploys the Qt Widgets SDK module directly so windeployqt does not confuse
the independently built QtKeychain DLL with an SDK module.

Per-platform local provenance identifies dependency pins, acceptance results and
the BuildLab source snapshot/commit, resources and log. `local-builds.json` records
the latest attempt per target; `SHA256SUMS` covers collected files. Failed attempts
are retained under `failed/<target>/<run-id>/` and do not replace verified packages.
A failed target does not prevent the other target from running. These commands build
locally without running GitHub Actions or publishing a GitHub release. ARM runtime
acceptance and publisher signing/notarization remain separate work.

### Existing platform helpers

Artix: use rootless Podman and the published source archive with its verified checksum:

```sh
scripts/package-artix.sh release-assets/aeris-v0.1.0-source.tar.gz \
  eea276924499f2b81166d650a645cc79906f3ac9fd3868d7dddc90586c83e4c7 release-assets
```

The helper builds with `makepkg` in an isolated Artix base-devel container using only `system`, `world`, and `galaxy`; it does not install packages on the host. Logs, container identity, and package versions are retained under `build-artix/`. Compiler, CMake, Ninja and pkgconf are build requirements, not runtime requirements. The native executable uses Artix's shared libraries and the desktop's existing Secret Service/KWallet. The binary metadata constrains protobuf, Abseil and ZXing to their build-time upstream versions because their shared-library ABIs change between releases. Publish it only after a separate clean Artix installation test. Build records and SBOMs are separate from the Ubuntu AppImage's records. Rolling repository library ABI changes require a new package build; do not advertise untested Arch or ARM compatibility.

For native publication, extract the package into `build-artix/package-root`, generate an SPDX SBOM with Syft, and supplement it using `scripts/complete-sbom.py --build build-artix/0.1.0/src/build`. Record the installed packages owning every library resolved by `ldd` in `build-artix/audit/runtime-library-packages.txt`. Capture each successful acceptance check as a boolean in `build-artix/audit/checks.json`, then run `python3 scripts/artix-release.py --output release-assets` after committing the packaging changes. This records external runtime libraries, the exact source/tag and packaging revisions, recipe hash, container identity and checks. Refresh release checksums and verify downloaded artifacts after upload. Preserve existing source/AppImage assets and their build records.

Linux: build a Release tree, install linuxdeploy and its Qt plugin beside one another, put Syft on PATH, then run:

```sh
LINUXDEPLOY=/absolute/path/linuxdeploy scripts/package-linux.sh build-release release-assets
scripts/smoke-linux.sh release-assets/aeris_linux_amd64.AppImage
```

Windows: use the matching MSVC/Qt SDK, the pinned vcpkg toolchain and `scripts/build-keychain.py`. `scripts/package-windows.ps1` deploys Qt and DLLs, collects notices, makes the ZIP and NSIS installer, and exercises both. Portable launches use `bin/aeris.exe`; all distributions use the platform app-data directory rather than storing secrets next to the executable.

macOS: run `scripts/package-macos.sh <triplet>` after each native build. `scripts/merge-macos.py` creates the universal bundle; `scripts/audit-macos.py` rejects unresolved build-machine dylib paths. See the workflow for exact commands.

`licenses/` includes direct dependency license texts. Packaging also collects the actual dependency-manager notices, including transitive libraries. Syft's filesystem inventory is supplemented with CMake/vcpkg dependency metadata because compiled Qt libraries are not consistently identified by generic file scanners. The SBOM labels this source of evidence. Artifacts published by the explicitly requested GitHub release workflow can receive GitHub build-provenance attestations. Locally published artifacts instead include a local build record; it is not a signed GitHub attestation.

Use **Delete all…** before uninstalling if you want to remove imported data and keychain credentials. Uninstallers remove program files and retain user data; they do not silently erase authenticator state.

## Release status

Release notes begin with direct application download links. Installation help,
features and technical details use closed `<details>` sections. Supporting
attachments (SBOMs, build records, test results, verification reports, manifests,
benchmarks and the Artix recipe) are bundled in `release-support.zip` to keep
the Assets list focused on applications. Source archives and `SHA256SUMS`
retain separate downloads; the support archive includes checksums for its
contents. Follow this layout for future releases.

The public `Alex9001/Aeris` repository publishes v0.1.0 with locally verified Artix x86-64 native pacman, Linux x86-64 AppImage, Windows x64 ZIP/installer and Intel macOS DMG/app-archive packages. The Windows and Mac additions pass native tests, credential/vault checks and package acceptance. Their build records identify a dirty checkout based on `fd5263eb23c54528a27da083a3a7fafc267c565e`, rather than the original release tag. `aeris-v0.1.0-native-source.tar.gz` captures that checkout after acceptance, including later release documentation; `native-source-manifest.json` distinguishes this publication snapshot from the transient build snapshots recorded in provenance. Existing Linux/source assets and the release tag are preserved. ARM outputs and a universal macOS package remain pending native builds and package acceptance. The results and remaining checks are recorded in `docs/verification.md`.
