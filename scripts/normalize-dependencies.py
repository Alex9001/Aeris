#!/usr/bin/env python3
"""Fill dependency versions from the actual vcpkg status database before bundle assembly."""
import json,sys
from pathlib import Path
path=Path(sys.argv[1]);metadata=json.loads(path.read_text())
status=Path(sys.argv[2]).parent/'vcpkg/status'
for paragraph in status.read_text().split('\n\n'):
    fields=dict(line.split(': ',1) for line in paragraph.splitlines() if ': ' in line)
    name={'nu-book-zxing-cpp':'zxing-cpp'}.get(fields.get('Package'),fields.get('Package'))
    if name in metadata and fields.get('Version'): metadata[name]['version']=fields['Version']
path.write_text(json.dumps(metadata,indent=2)+'\n')
