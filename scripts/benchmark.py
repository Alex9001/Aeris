#!/usr/bin/env python3
"""Linux process measurements, with synthetic session secrets and an isolated data directory."""
import argparse, json, os, platform, re, subprocess, tempfile, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',default='build-release');a=p.parse_args()
build=Path(a.build).resolve(); result={'platform':platform.platform(),'cpu':next((l.split(':',1)[1].strip() for l in Path('/proc/cpuinfo').read_text().splitlines() if l.startswith('model name')), 'unknown'),'platformPlugin':'offscreen'}
with tempfile.TemporaryDirectory(prefix='aeris-benchmark-') as d:
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',XDG_DATA_HOME=d,XDG_CONFIG_HOME=d)
    starts=[]
    for _ in range(6):
        r=subprocess.run([str(build/'aeris'),'--startup-report','--smoke-test'],env=env,text=True,capture_output=True,check=True)
        starts.append(int(re.search(r'Startup ready: (\d+) ms',r.stderr)[1]))
    result['emptyWarmStartupMs']=starts[1:]
    child=subprocess.Popen([str(build/'bench_session')],env=env,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    ready=child.stdout.readline().strip(); result['thousandAccountLoadAndShowMs']=int(ready.split()[1])
    def sample():
        fields=Path(f'/proc/{child.pid}/stat').read_text().split(); ticks=int(fields[13])+int(fields[14])
        status=Path(f'/proc/{child.pid}/status').read_text(); rss=int(re.search(r'VmRSS:\s+(\d+)',status)[1]);return ticks,rss
    time.sleep(1);before,_=sample();start=time.monotonic();rss=[]
    for _ in range(5): time.sleep(1);ticks,mem=sample();rss.append(mem)
    elapsed=time.monotonic()-start
    result['thousandAccountIdleCpuPercent']=round((ticks-before)/os.sysconf('SC_CLK_TCK')/elapsed*100,3)
    result['thousandAccountRssKiB']=max(rss)
    child.wait(timeout=10)
    filtering=subprocess.run([str(build/'test_ui'),'filtering'],env=env,text=True,capture_output=True,check=True)
    result['filter1000Ms']=float(re.search(r'Filtering 1000 entries: ([0-9.]+) ms',filtering.stdout)[1])
result['credentialBackend']='in-memory test credential backend for 1000-account session; production keyring latency excluded'
result['executableBytes']=(build/'aeris').stat().st_size
print(json.dumps(result,indent=2))
