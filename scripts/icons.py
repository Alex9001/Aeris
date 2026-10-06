#!/usr/bin/env python3
"""Derive platform containers and sizes from the supplied master PNG; preserve alpha."""
from pathlib import Path
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parents[1]
master = Image.open(root / 'assets/aeris-master.png').convert('RGBA')
assert master.getextrema()[3][0] == 0, 'Master must have real transparency'
for size in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
    master.resize((size, size), Image.Resampling.LANCZOS).save(root / f'assets/icons/aeris-{size}.png')
master.save(root / 'assets/aeris.ico', sizes=[(s,s) for s in (16,24,32,48,64,128,256)])
master.resize((1024,1024), Image.Resampling.LANCZOS).save(root / 'assets/aeris.icns')
canvas = Image.new('RGB', (650, 190), '#eeeeee')
draw = ImageDraw.Draw(canvas)
for n, size in enumerate((16,24,32,48,64,128)):
    x = 20 + n*95
    icon = master.resize((size,size), Image.Resampling.LANCZOS)
    canvas.paste(icon, (x,20), icon)
    draw.text((x,160), str(size), fill='black')
canvas.save(root / 'assets/icon-readability.png')
