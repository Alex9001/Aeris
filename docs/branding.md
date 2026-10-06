# Aeris artwork

The application uses the user-supplied **Satin A-Shield Authenticator Icon.png**, selected on 2026-10-06: a cyan and blue satin A on a shield with a transparent background.

`assets/aeris-master.png` is a byte-for-byte copy of that image (1254 × 1254, RGBA). Its SHA-256 is `82e440d8ae1a0a109be8cc82269fa8190e2a2ed61b9cb92351e7b36f20910089`.

Run `scripts/icons.py` to derive the Linux desktop PNGs in `assets/icons/` (16–1024 pixels), the Windows icon `assets/aeris.ico`, and the macOS icon `assets/aeris.icns`. The Qt window icon and About logo use the bundled 256-pixel PNG. The AppImage embeds the Linux desktop icon. Resizing preserves the supplied artwork's colors and transparency.

`assets/icon-readability.png` shows the 16–128 pixel variants on a neutral background.
