# Desktop releases

GitHub Actions are disabled to conserve quota and must only be enabled or run on explicit request. The retained workflows have no automatic push, pull-request, or tag triggers. Manual workflow dispatch builds a nonpublishing preflight; a manually requested run against a `v*` tag matching `CMakeLists.txt` can publish after its quality and packaging gates pass.

The first public release is built locally. Its verified binary target is **Linux x86-64**, with the corresponding source archive, SPDX SBOM, checksums, source recipe and local build provenance.

| Platform | Outputs | Verification |
| --- | --- | --- |
| Linux amd64 / arm64 | AppImage per architecture | Native tests; packaged X11 under Xvfb and Wayland under headless Weston |
| Windows amd64 / arm64 | Per-user NSIS installer and portable ZIP | Native tests; extracted ZIP, silent install, installed app and uninstall smoke tests |
| macOS Intel + Apple silicon | One universal DMG | Native tests per slice; merge/audit every Mach-O; launch both slices and app from mounted DMG |
| Artix x86-64 | Native `.pkg.tar.zst` and source PKGBUILD | Clean standard-repository build, installation, upgrade/removal, X11/Wayland and KWallet checks |

Linux release baseline is Ubuntu 24.04; both Qt XCB and Wayland plugins are explicitly bundled. AppImages do not bundle the platform keyring daemon. Windows and macOS use Qt 6.10.2 and pinned vcpkg dependencies, plus independently built QtKeychain 0.17.0. On macOS, each native bundle is deployed before merging, with all non-system dylibs required to resolve inside the bundle. Ad-hoc signatures are applied for Apple silicon execution; packages have no publisher signing certificate or notarization.

Actions are pinned by commit. vcpkg's baseline is pinned in `vcpkg.json`. linuxdeploy's continuous artifacts are accepted only with the pinned SHA-256; if upstream replaces them, the workflow intentionally fails until hashes are reviewed. Apt dependencies follow Ubuntu security updates. Full binary reproducibility is not claimed.

## Local packaging

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

The public `Alex9001/Aeris` repository publishes v0.1.0 with a locally verified Artix x86-64 native pacman package and Linux x86-64 AppImage. Windows, macOS and Linux ARM outputs remain pending native builds and package acceptance. The local results and remaining checks are recorded in `docs/verification.md`.
