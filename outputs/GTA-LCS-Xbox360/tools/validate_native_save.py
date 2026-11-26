"""Independent host checks for an LCX4 save produced by the native XEX."""
import argparse, hashlib, json, struct
from pathlib import Path

def inspect(path):
    data=Path(path).read_bytes()
    u32=lambda b,p=0: struct.unpack_from(">I",b,p)[0]
    blocks=[]; pos=0; payload=0; checksum=0
    while payload<524284:
        n=u32(data,pos); assert 0<n<=102400,(pos,n)
        aligned=(n+3)&~3
        assert pos+4+aligned<=len(data)
        checksum=(checksum+sum(data[pos:pos+4+aligned]))&0xffffffff
        blocks.append(data[pos+4:pos+4+n]); payload+=aligned;pos+=4+aligned
    assert payload==524284 and pos+4==len(data)
    assert u32(data,pos)==checksum,"checksum mismatch"
    assert u32(blocks[0],64)==524288
    assert u32(blocks[0],68)==0x4c435834,"unsupported native save version"
    inner=[]
    for b in blocks[1:20]:
        size=u32(b); assert 4+((size+3)&~3)==len(b),(size,len(b))
        inner.append(b[4:4+size])
    # Counts and packed sizes are independently derived from LCS field layouts.
    assert len(inner[5])==8+11*136,"truncated crane records"
    assert len(inner[6])==336*56+4+20*4,"truncated pickup records"
    assert len(inner[7])==8+60*56,"truncated telephone records"
    assert len(inner[13])==4+136*(u32(inner[13])+1),"particle record size"
    for i in range(336):
        pickup=inner[6][i*56:(i+1)*56]
        assert len(pickup)==56
        for off in (20,24):
            index=struct.unpack_from(">i",pickup,off)[0]
            assert 0<=index<=10000,(i,index)
    missions=struct.unpack_from(">i",inner[16],len(inner[16])-8)[0]
    return {"status":"PASS","format":"LCX4","file_bytes":len(data),
        "sha256":hashlib.sha256(data).hexdigest(),"outer_blocks":len(blocks),
        "checksum":checksum,"title":blocks[0][:48].decode("utf-16-be").split(chr(0))[0],
        "missions_passed":missions,"cranes_bytes":len(inner[5]),
        "pickups_bytes":len(inner[6]),"phones_bytes":len(inner[7]),"particles_bytes":len(inner[13])}

if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("save");p.add_argument("--output");a=p.parse_args()
    result=inspect(a.save);text=json.dumps(result,indent=2)
    if a.output:Path(a.output).write_text(text+"\n",encoding="utf-8")
    print(text)
