# Supplemental artwork sources

`catalog.json` identifies each source, its SHA-256 and explicit aliases. Original
SVG/PNG/ICO files are retained here. The prepared runtime PNGs and their output
hashes are in `../brands`; only the prepared images and merged catalog enter Qt
resources. No runtime or normal build step accesses the network.

Eleven sources are pinned to 2FA Directory revision
`92d901f308ed6adc07b2415377b011c7dd544b13`; four are checksum-pinned snapshots
of official-site artwork retrieved on 2026-10-06. See
`../../licenses/brand-supplement-NOTICE.md` for ownership and upstream policy.
These images do not inherit the selfh.st CC-BY-4.0 or Aeris MIT license.

To regenerate the supplement and refresh aliases, run:

```sh
.venv/bin/python scripts/brand-icons.py
```

Preparation used Pillow 12.3.0 and rsvg-convert 2.62.4. SVGs are rendered into
at most 128×128 pixels with their proportions and colors retained. Raster
sources are converted to RGBA, reduced when needed, and losslessly PNG-encoded.
Already-small raster sources are not enlarged in the bundle. Every source
checksum is verified before use. Output files are written only when bytes change.
