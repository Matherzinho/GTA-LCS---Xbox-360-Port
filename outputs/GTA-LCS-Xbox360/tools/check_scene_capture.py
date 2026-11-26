#!/usr/bin/env python3
"""Pixel-level checks of the native XSDK scene diagnostic, not gameplay/FPS."""
from pathlib import Path
import json,collections
ROOT=Path(__file__).resolve().parents[1]
RUNTIME=ROOT.parents[1]/'work/lcs/runtime'
def ppm(name):
    data=(RUNTIME/('scene-'+name+'.ppm')).read_bytes()
    header,body=data.split(b'\n255\n',1)
    assert header==b'P6\n1280 720' and len(body)==1280*720*3
    return body
BG=(16,24,32)
def summary(data):
    colors=collections.Counter(zip(data[::3],data[1::3],data[2::3]))
    points=[(i%1280,i//1280) for i,p in enumerate(zip(data[::3],data[1::3],data[2::3])) if p!=BG]
    return colors, (tuple(sum(p[j] for p in points)/len(points) for j in (0,1)) if points else None)
def main():
    assert 'hierarchy regression PASS:' in (ROOT/'logs/lcs_scene_probe.log').read_text(errors='replace'), 'branched HAnim regression missing'
    bind,pose,world,ambient,flip=map(ppm,('bind','pose','world-pose','ambient','normal-flip'))
    a,ca=summary(bind); b,cb=summary(pose)
    assert abs((ca[0]-cb[0])-67.5)<=1 and abs(ca[1]-cb[1])<=1, 'weighted skin displacement'
    assert pose==world, 'local/world hierarchy mismatch'
    assert a[(95,191,191)]==8100 and b[(95,191,191)]==8100, 'ambient + directional + material modulation'
    assert summary(ambient)[0][(32,64,64)]==8100, 'ambient only'
    assert summary(flip)[0][(32,64,64)]==8100, 'skinned normal rotation'
    assert ppm('cull-back')==pose and set(zip(ppm('cull-front')[::3],ppm('cull-front')[1::3],ppm('cull-front')[2::3]))=={BG}, 'front/back culling'
    assert ppm('depth')==ppm('depth-reverse'), 'depth depends on draw order'
    colors,_=summary(ppm('depth')); assert colors[(255,0,0)]>1000 and colors[(0,0,255)]==0, 'closer quad must occlude far quad'
    assert summary(ppm('behind'))[1] is None, 'behind-camera geometry leaked'
    clipped,_=summary(ppm('near-clip')); assert 1000<clipped[(255,0,0)]<1280*720, 'near clipping produced empty/unbounded output'
    assert summary(ppm('world-light'))[0][(48,64,192)]==8100, 'world colour scale applied before texture'
    assert summary(ppm('world-deferred'))[1] is None, 'alpha material leaked into opaque pass'
    assert summary(ppm('world-alpha'))[0][(32,44,112)]==8100, 'deferred world alpha blend'
    assert summary(ppm('world-proxy'))[1] is None, 'untextured world proxy rendered'
    assert ppm('world-dual')==ppm('world-alpha'), 'shared upload alpha passes changed image'
    assert ppm('range-zero')==ppm('range-offset'), 'nonzero mesh base changed projected quad'
    assert summary(ppm('range-offset'))[1] is not None, 'mesh range regression drew nothing'
    aa_off,aa_on=ppm('fxaa-off'),ppm('fxaa-on')
    changed=sum(a!=b for a,b in zip(aa_off,aa_on))
    assert 100<changed<30000, 'FXAA did not filter edges or changed the entire image'
    for x,y in [(80,80),(640,360),(25,25)]:
        start=(y*1280+x)*3
        assert aa_off[start:start+3]==aa_on[start:start+3], 'FXAA changed flat region or post-filter HUD'
    result={'status':'PASS' ,'checks':['branched HAnim hierarchy with rotated parents','weighted skin displacement','local/world hierarchy equivalence','ambient/directional light and material colour','skinned normal rotation','front/back culling','depth order independence','behind camera rejection','near plane crossing','world prelight/emission and post-texture PS2 scale','opaque/deferred alpha separation','untextured proxy rejection','nonzero material index span rebasing','FXAA edge filtering with preserved flat colours and HUD'], 'scope':'Synthetic geometry through actual native pipeline in Xenia; gameplay and physical FPS not certified'}
    (ROOT/'logs/scene-validation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
