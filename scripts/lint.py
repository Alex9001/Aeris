#!/usr/bin/env python3
"""Gate every maintained C++ function; generated protobuf and dependencies excluded."""
import argparse, concurrent.futures, json, subprocess, sys
from pathlib import Path
import lizard
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build')
p.add_argument('--tidy', default='clang-tidy')
p.add_argument('--jobs', type=int, default=2)
a = p.parse_args()
files = sorted([*root.glob('src/**/*.cpp'), *root.glob('src/**/*.h'), *root.glob('tests/*.cpp')])
failures=[]
for path in files:
    for function in lizard.analyze_file(str(path)).function_list:
        if function.cyclomatic_complexity > 10:
            failures.append(f'{path.relative_to(root)}:{function.start_line}: {function.name}: CCN {function.cyclomatic_complexity} > 10')
print('\n'.join(failures) or 'Lizard: all maintained C++ functions have CCN <= 10', flush=True)
commands = json.loads((root / a.build / 'compile_commands.json').read_text())
translation_units = sorted({r['file'] for r in commands if Path(r['file']).resolve() in files})
def check(path):
    r = subprocess.run([a.tidy, '-p', str(root/a.build), '--quiet', path], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return path, r.returncode, r.stdout
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
    for path, code, output in pool.map(check, translation_units):
        if code:
            failures.append(path)
            print(output)
if failures: sys.exit(1)
print('clang-tidy: cognitive complexity <= 15')
