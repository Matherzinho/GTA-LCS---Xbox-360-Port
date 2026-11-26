#!/usr/bin/env python3
"""Reject the Xenia-skipped opcodes that broke the HAnim parent stack."""
from pathlib import Path
import re,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
build=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT.parents[1]/'work/lcs/build-xsdk'
configs=list((build/'CMakeFiles').glob('*/CMakeCXXCompiler.cmake'))
assert configs, 'Missing compiler configuration'
match=re.search(r'set\(CMAKE_CXX_COMPILER "([^"\n]+)"',configs[0].read_text())
assert match, 'Missing compiler path'
objdump=Path(match.group(1)).with_name(Path(match.group(1)).name.replace('g++','objdump'))
symbol='_ZN2rw14HAnimHierarchy14updateMatricesEv'
text=subprocess.check_output([str(objdump),'-d','--disassemble='+symbol,str(build/'lcs_boot')],text=True)
assert '<'+symbol+'>:' in text, 'Hierarchy function missing from ELF'
assert not re.search(r'\b(?:mcrf|rldic)\b',text), 'Hierarchy contains opcode skipped by the Xenia runtime'
(ROOT/'logs/hanim-disassembly.txt').write_text(text)
(ROOT/'logs/animation-codegen.json').write_text(json.dumps({'status':'PASS','symbol':symbol,'check':'No Xenia-skipped mcrf/rldic in HAnim updateMatrices; other game paths not certified'},indent=2)+'\n')
print('PASS: HAnim parent-stack codegen has no mcrf/rldic')
