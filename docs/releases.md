# Desktop releases

GitHub Actions are disabled to conserve quota and must only be enabled or run on explicit request. The retained workflows have no automatic push, pull-request, or tag triggers. Manual workflow dispatch builds a nonpublishing preflight; a manually requested run against a `v*` tag matching `CMakeLists.txt` can publish after its quality and packaging gates pass.

The first public release is built locally. Its verified binary target is **Linux x86-64**, with the corresponding source archive, SPDX SBOM, checksums, source recipe and local build provenance.

| Platform | Outputs | Verification |
| --- | --- | --- |
| Linux amd64 / arm64 | AppImage per architecture | Native tests; packaged X11 under Xvfb and Wayland under headless Weston |
| Windows amd64 / arm64 | Per-user NSIS installer and portable ZIP | Native tests; extracted ZIP, silent install, installed app and uninstall smoke tests |
| macOS Intel + Apple silicon | One universal DMG | Native tests per slice; merge/audit every Mach-O; launch both slices and app from mounted DMG |
| Arch Linux | Source PKGBUILD | Version and SHA-256 rendered from the exact release source archive |

Linux release baseline is Ubuntu 24.04; both Qt XCB and Wayland plugins are explicitly bundled. AppImages do not bundle the platform keyring daemon. Windows and macOS use Qt 6.10.2 and pinned vcpkg dependencies, plus independently built QtKeychain 0.17.0. On macOS, each native bundle is deployed before merging, with all non-system dylibs required to resolve inside the bundle. Ad-hoc signatures are applied for Apple silicon execution; packages have no publisher signing certificate or notarization.

Actions are pinned by commit. vcpkg's baseline is pinned in `vcpkg.json`. linuxdeploy's continuous artifacts are accepted only with the pinned SHA-256; if upstream replaces them, the workflow intentionally fails until hashes are reviewed. Apt dependencies follow Ubuntu security updates. Full binary reproducibility is not claimed.

## Local packaging

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

The public `Alex9001/Aeris` repository publishes v0.1.0 with a locally verified Linux x86-64 AppImage. Windows, macOS and Linux ARM outputs remain pending native builds and package acceptance. The local results and remaining checks are recorded in `docs/verification.md`.
