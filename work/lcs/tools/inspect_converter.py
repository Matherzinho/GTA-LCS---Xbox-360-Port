from pathlib import Path
import mmap, struct, json
root=Path(__file__).resolve().parents[1]
p=root/'reLCSAssetConverter.exe'
with p.open('rb') as f:
 m=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
 result=[]
 for magic in (b'7z\xbc\xaf\x27\x1c',b'PK\x03\x04',b'Rar!\x1a\x07',b'MSCF'):
  at=0
  for _ in range(12):
   at=m.find(magic,at)
   if at<0:break
   result.append({'magic':magic.hex(),'offset':at,'next':m[at:at+32].hex()})
   at+=len(magic)
 print(json.dumps(result,indent=2))
 print('TAIL',repr(m[-160:]))