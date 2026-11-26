#!/usr/bin/env python3
"""Check real intro hierarchy dumps from start_boot.ps1 -City -TracePose."""
from pathlib import Path
from datetime import datetime
import json,math,hashlib
ROOT=Path(__file__).resolve().parents[1]
RUNTIME=ROOT.parents[1]/'work/lcs/runtime'
def quat_matrix(q):
 x,y,z,w=q
 return [1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)]
def main():
 run=json.loads((RUNTIME/'boot-run.json').read_text(encoding='utf-8-sig')); start=datetime.fromisoformat(run['started_utc']).timestamp()
 assert hashlib.sha256((RUNTIME/'lcs_boot.xex').read_bytes()).hexdigest()==run['sha256']
 samples=[]; signatures=set()
 for p in sorted(RUNTIME.glob('skin-pose-*.json')):
  assert p.stat().st_mtime>=start, 'Stale pose dump'
  pose=json.loads(p.read_text());bones=pose['bones'];assert len(bones)==53
  stack=[-1];parent=-1;translation_error=rotation_error=0
  for i,b in enumerate(bones):
   assert abs(sum(x*x for x in b['q'])-1)<.002, 'Invalid quaternion'
   if parent>=0:
    m=bones[parent]['pose'];t=b['t'];q=quat_matrix(b['q'])
    expected=[sum(t[k]*m[k*3+j] for k in range(3))+m[9+j] for j in range(3)]
    translation_error=max(translation_error,math.sqrt(sum((expected[j]-b['pose'][9+j])**2 for j in range(3))))
    rotation_error=max(rotation_error,max(abs(sum(q[row*3+k]*m[k*3+col] for k in range(3))-b['pose'][row*3+col]) for row in range(3) for col in range(3)))
   if b['flags']&2:stack.append(parent)
   parent=i
   if b['flags']&1:
    assert stack, 'Parent stack underflow'
    parent=stack.pop()
  assert translation_error<.001 and rotation_error<.001, 'Real hierarchy has a wrong parent/rotation'
  signatures.add(tuple(round(x,3) for x in bones[0]['q']))
  samples.append({'file':p.name,'bones':len(bones),'max_translation_error':translation_error,'max_rotation_error':rotation_error})
 assert len(samples)>=3 and len(signatures)>=2, 'Need multiple animated intro samples'
 guest=(RUNTIME/'lcs_boot.log').read_text(errors='replace');assert 'LCS BOOT stage 6: city initialization returned' in guest
 probe=(ROOT/'logs/lcs_scene_probe.log').read_text(errors='replace');assert 'hierarchy regression PASS:' in probe
 report={'status':'PASS for observed introductory hierarchy/poses','xex_sha256':run['sha256'],'samples':samples,'distinct_root_orientations':len(signatures),'cause':'Xenia skips mcrf emitted by adjacent HAnim PUSH/POP flag tests; sibling limbs inherited the previous branch','fix':'Equivalent switch on combined flag operation, bounded parent stack; no mcrf/rldic in the compiled HAnim routine','scope':'Real intro sampled in Xenia. Full campaign, controls, serialized saves, audio and physical 30 FPS remain unverified.'}
 (ROOT/'logs/skin-animation-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
