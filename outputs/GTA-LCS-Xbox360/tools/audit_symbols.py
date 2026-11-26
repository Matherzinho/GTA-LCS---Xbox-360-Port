#!/usr/bin/env python3
"""Whole-archive inventory for platform integration; not an executable link test."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = (ROOT / '../../work/lcs/build-xsdk').resolve()
SDK = Path(os.environ.get('XSDK_ROOT', str(Path.home() / 'xsdk')))
PREFIX = Path.home() / 'xsdk-gcc/bin/powerpc-xsdk-elf-'

def run(tool, *args):
    return subprocess.check_output([str(PREFIX)+tool, *map(str,args)], text=True)

project_archives = [BUILD/'liblcs_gameplay.a', BUILD/'liblcs_fileio.a', BUILD/'liblcs_text.a', BUILD/'librw/src/librw.a', BUILD/'liblcs_gpu.a']
archives = project_archives + [SDK/'build/libxsdk.a']
archives += [Path(run('g++', '-print-file-name='+name).strip()) for name in ('libstdc++.a','libc.a','libm.a','libgcc.a')]
defined, required = set(), set()
for i, archive in enumerate(archives):
    for line in run('nm', '-g', '--format=posix', archive).splitlines():
        row = line.split()
        if len(row)<2 or len(row[1])!=1:
            continue
        symbol, kind = row[:2]
        if kind == 'U' and i<len(project_archives):
            required.add(symbol)
        elif kind not in ('U','w','v'):
            defined.add(symbol)
missing = sorted(required-defined)
names = run('c++filt', *missing).splitlines() if missing else []
result = {'scope':'Whole archive symbol inventory, including unreachable modules; not proof of executable linkage',
          'unresolved_count':len(missing), 'unresolved':names}
(ROOT/'logs/platform-symbols.json').write_text(json.dumps(result,indent=2)+'\n', encoding='utf-8')
print(json.dumps(result,indent=2))
