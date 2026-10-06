#!/usr/bin/env python3
"""Build pinned Qt 6 Keychain against the same Qt SDK used by the application."""
import argparse, subprocess
from pathlib import Path
p=argparse.ArgumentParser(); p.add_argument('--prefix', required=True); p.add_argument('--arch'); a=p.parse_args()
root=Path(__file__).resolve().parents[1]
source=root/'build-deps/qtkeychain-source'
commit='875f77d9f61bd97fd84cca47ce3bc71186dfbd09'
def run(*args): subprocess.run(args,check=True)
if not (source/'.git').exists(): run('git','clone','https://github.com/frankosterfeld/qtkeychain.git',str(source))
run('git','-C',str(source),'checkout','--detach',commit)
args=['cmake','-S',str(source),'-B',str(root/'build-deps/qtkeychain'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DBUILD_WITH_QT5=OFF','-DBUILD_TRANSLATIONS=OFF','-DBUILD_TEST_APPLICATION=OFF','-DCMAKE_INSTALL_PREFIX='+str(Path(a.prefix).resolve())]
if a.arch: args.extend(['-DCMAKE_OSX_ARCHITECTURES='+a.arch,'-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0'])
run(*args); run('cmake','--build',str(root/'build-deps/qtkeychain'),'--parallel','3'); run('cmake','--install',str(root/'build-deps/qtkeychain'))
