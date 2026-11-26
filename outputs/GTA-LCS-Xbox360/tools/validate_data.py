#!/usr/bin/env python3
"""Validate converted user data before the Xbox renderer consumes it."""
from pathlib import Path
import hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'../../work/lcs/data'
DATA=DATA.resolve()
files={p.relative_to(DATA).as_posix().lower():p for p in DATA.rglob('*') if p.is_file()}
errors=[]
warnings=[]
required=['data/gta_lcs.dat','data/main.scm','models/gta3.img','models/gta3.dir','models/fonts.txd','models/hud.txd','models/x360btns.txd','models/frontend_x360.txd','text/american.gxt','audio/sfx.raw','audio/sfx.sdt']
missing=[p for p in required if p not in files]
errors.extend('missing '+p for p in missing)
references=[]
if 'data/gta_lcs.dat' in files:
 for line in files['data/gta_lcs.dat'].read_text(errors='replace').splitlines():
  parts=line.split('#',1)[0].strip().split()
  if parts and parts[0].upper() in ('IDE','IPL','COLFILE','MODELFILE','TEXDICTION'):
   rel=parts[-1].replace('\\','/').lower();references.append(rel)
   if rel not in files:
    if parts[0].upper()=='TEXDICTION' and rel=='models/generic/wheels.txd':
     warnings.append('Missing wheels.txd: upstream LoadTexDictionary substitutes an empty dictionary. Wheel appearance remains to be checked in game.')
    else:errors.append('DAT reference missing '+rel)
img_entries=[]
if not any(p not in files for p in ('models/gta3.img','models/gta3.dir')):
 index=files['models/gta3.dir'].read_bytes(); size=files['models/gta3.img'].stat().st_size
 if len(index)%32:errors.append('IMG directory has partial record')
 for at in range(0,len(index)-31,32):
  offset,sectors,name=struct.unpack_from('<II24s',index,at)
  name=name.split(b'\0',1)[0].decode('ascii',errors='replace')
  begin=offset*2048;end=begin+sectors*2048
  # Last sector may be stored without its trailing padding.
  if begin>=size or end>size+2047:errors.append('IMG range outside archive '+name)
  img_entries.append(name)
 for extension in ('.dff','.txd'):
  if not any(n.lower().endswith(extension) for n in img_entries):errors.append('IMG lacks '+extension)
txd=[]
for name,path in files.items():
 if not name.endswith('.txd'):continue
 with path.open('rb') as f:header=f.read(12)
 if len(header)<12:errors.append('short TXD '+name);continue
 kind,body,version=struct.unpack('<III',header)
 if kind!=0x16 or body+12>path.stat().st_size:errors.append('invalid TXD container '+name)
 txd.append({'file':name,'version':hex(version)})
samples=0
if not any(p not in files for p in ('audio/sfx.raw','audio/sfx.sdt')):
 table=files['audio/sfx.sdt'].read_bytes();size=files['audio/sfx.raw'].stat().st_size
 if len(table)%20:errors.append('audio table has partial sample')
 for at in range(0,len(table)-19,20):
  offset,length,hz,loop_begin,loop_end=struct.unpack_from('<IIIIi',table,at);samples+=1
  if offset+length>size:errors.append('audio sample outside bank '+str(samples-1))
  if length and not 1000<=hz<=192000:errors.append('invalid sample rate '+str(samples-1))
source=ROOT/'../../work/lcs/disc-extracted/DATA/MAIN.SCM'
script_matches=None
if source.exists() and 'data/main.scm' in files:
 script_matches=hashlib.sha256(source.read_bytes()).digest()==hashlib.sha256(files['data/main.scm'].read_bytes()).digest()
 if not script_matches:errors.append('campaign script differs from supplied disc')
result={'root':str(DATA),'files':len(files),'bytes':sum(p.stat().st_size for p in files.values()),'required_files':len(required),'dat_references':len(references),'img_entries':len(img_entries),'texture_dictionaries':len(txd),'audio_samples':samples,'campaign_matches_disc':script_matches,'errors':errors,'warnings':warnings,'status':'PASS' if not errors else 'FAIL','scope':'Container and range validation only; does not establish gameplay or rendering correctness'}
(ROOT/'logs/data-validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
sys.exit(bool(errors))