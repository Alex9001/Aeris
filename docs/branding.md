# Aeris artwork

The master icon is `assets/aeris-master.png` (1254 × 1254, RGBA), a byte-for-byte copy of the user-supplied **Satin A-Shield Authenticator Icon.png**, selected on 2026-10-06.

Master SHA-256: `82e440d8ae1a0a109be8cc82269fa8190e2a2ed61b9cb92351e7b36f20910089`.

Run `python3 scripts/icons.py` to generate:

- Linux desktop icons: `assets/icons/` (16–1024 pixels)
- Windows icon: `assets/aeris.ico`
- macOS icon: `assets/aeris.icns`
- Size preview: `assets/icon-readability.png` (16–128 pixels)

The Qt window icon and About logo use the bundled 256-pixel PNG. The AppImage embeds the Linux desktop icon. Generated icons preserve the master's colors and transparency.
