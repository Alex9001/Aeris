Aeris is a native, offline viewer for phone authenticator exports. Your phone remains the source of truth.

## Available package

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
