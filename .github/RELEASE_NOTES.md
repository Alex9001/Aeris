Aeris is a native, offline viewer for phone authenticator exports. Your phone remains the source of truth.

## Available packages

**Artix x86-64 native pacman package**, built locally with standard Artix repository libraries. Download `aeris-0.1.0-2-x86_64.pkg.tar.zst` and install it:

```sh
sudo pacman -U ./aeris-0.1.0-2-x86_64.pkg.tar.zst
```

Pacman fetches any missing runtime dependencies from your standard Artix repositories. No compilation, compiler, CMake, Ninja, AUR helper, or AppImage is needed. Existing KWallet users can keep using KWallet. Use an up-to-date Artix x86-64 system; other distributions have not been verified for this native package. Package checks and dependency versions are in `build-provenance.artix.json`; its SBOM is `aeris_artix_x86_64.sbom.json`.

**Linux x86-64 AppImage**, built locally on Ubuntu 24.04 with bundled Qt X11 and Wayland integrations. Windows, macOS and ARM binaries are not included in this release; their native builds and package acceptance remain pending.

Download `aeris_linux_amd64.AppImage`, make it executable, then launch it:

```sh
chmod +x aeris_linux_amd64.AppImage
./aeris_linux_amd64.AppImage
```

Without FUSE, use `APPIMAGE_EXTRACT_AND_RUN=1 ./aeris_linux_amd64.AppImage`. A working system keyring is required; the keyring daemon is supplied by your desktop.

## Included

- Imports from Aegis, Google Authenticator, Raivo, 2FAS, Ente, Bitwarden Authenticator, Proton Authenticator, andOTP and OTP-link text.
- Offline service logos, nine themes, List/Compact/Cards layouts, persistent drag-and-drop account order, and ownership-aware clipboard clearing.
- Encrypted local account storage protected by the system keyring.
- Source archive, SPDX SBOM, dependency/license notices, source manifest, SHA-256 checksums, local build provenance and verification report.

Every import replaces the whole Aeris collection after review. Source exports remain untouched. Use **Delete all…** before uninstalling if you want to remove saved accounts and credentials.

This is an unsigned local build with no GitHub build attestation. Local tests, complexity checks and packaged smoke results are recorded in `VERIFICATION.md` and `build-provenance.local.json`. GitHub Actions remained disabled throughout the build and publication.

Application code is MIT licensed. Bundled service artwork and the demo soundtrack retain their separate licenses; see the source archive's third-party notices.
