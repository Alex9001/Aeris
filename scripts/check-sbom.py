#!/usr/bin/env python3
import json,sys
p=json.load(open(sys.argv[1],encoding='utf-8'))
packages=p.get('packages',[])
if len(packages)<8: raise SystemExit('SBOM must enumerate bundled dependencies')
if not any('qt' in x.get('name','').lower() for x in packages): raise SystemExit('SBOM missing Qt')
