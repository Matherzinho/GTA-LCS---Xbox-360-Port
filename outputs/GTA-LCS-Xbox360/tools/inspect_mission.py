"""Read-only disassembly of user-converted LCS mission data for test planning."""
from pathlib import Path
import re, struct, argparse
ROOT=Path(__file__).resolve().parents[3]
SRC=ROOT/'work/lcs/relcs/src/control'
def preprocess(s):
    return re.sub(r'#ifdef SUPPORT_GINPUT_SCRIPT.*?#else(.*?)#endif',r'\1',s,flags=re.S)
s=preprocess((SRC/'ScriptCommands.h').read_text());s=re.sub(r'//[^\n]*|/\*.*?\*/','',s,flags=re.S)
entries={};n=0
for name,val in re.findall(r'\b(COMMAND_\w+)\s*(?:=\s*(\d+))?\s*[,}]',s):
    if val:n=int(val)
    entries[name]=n;n+=1
meta={}
for name,a,b in re.findall(r'REGISTER_COMMAND\((COMMAND_\w+), INPUT_ARGUMENTS\((.*?)\), OUTPUT_ARGUMENTS\((.*?)\)',preprocess((SRC/'ScriptDebug.cpp').read_text())):
    if name in entries:meta[entries[name]]=(name,[x.strip() for x in (a+','+b).split(',') if x.strip()])
data=(ROOT/'work/lcs/data/DATA/main.scm').read_bytes()
main,largest=struct.unpack_from('<II',data);x=data[8:];v=struct.unpack_from('<I',x,3)[0];o=struct.unpack_from('<I',x,v+3)[0];num=struct.unpack_from('<H',x,o+16)[0]
offsets=list(struct.unpack_from('<'+'I'*num,x,o+20))
def argument(d,p,kind):
    if kind=='ARGTYPE_STRING':return repr(d[p:p+8].split(b'\0')[0].decode('ascii',errors='replace')),p+8
    t=d[p];p+=1
    if t==0:return 'END',p
    if t in (1,2):return 0,p
    sizes={3:1,4:2,5:3,6:4,7:1,8:2,9:4}
    if t in sizes:
        n=sizes[t];raw=d[p:p+n];p+=n
        if t in (3,4,5):
            bits=(int.from_bytes(raw,'little')<<(32-8*n));value=struct.unpack('<f',bits.to_bytes(4,'little'))[0]
        elif t==9:value=struct.unpack('<f',raw)[0]
        else:value=int.from_bytes(raw,'little',signed=True)
        return value,p
    if t<12:return f'T{t-10}',p
    if t<108:return f'L{t-12}',p
    if t<204:return f'LARR{t-108}[L{d[p]}]/{d[p+1]}',p+2
    if t<230:return f'G{4*((t-204)*256+d[p])}',p+1
    return f'GARR{t-230}/{d[p:p+3].hex()}',p+3

def decode(d,p,end):
    while p<end:
        start=p;op=struct.unpack_from('<H',d,p)[0];p+=2;name,kinds=meta[op&0x7fff];args=[]
        if 'ARGTYPE_FUNCTION' in kinds:
            ins,outs,loc=d[p:p+3];p+=3
            for _ in range(1+ins+outs):a,p=argument(d,p,'ARGTYPE_INT');args.append(a)
        else:
            for k in kinds:a,p=argument(d,p,k);args.append(a)
        if name=='COMMAND_GOSUB_FILE':
            for _ in range(2):a,p=argument(d,p,'ARGTYPE_INT');args.append(a)
        if name=='COMMAND_START_NEW_SCRIPT':
            while d[p]!=0:a,p=argument(d,p,'ARGTYPE_INT');args.append(a)
            p+=1
        yield start,('NOT ' if op&0x8000 else '')+name.removeprefix('COMMAND_'),args
        if name=='COMMAND_TERMINATE_THIS_SCRIPT':pass

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('mission');ns=a.parse_args()
    if ns.mission.upper()=='MAIN':
        d=data[8:8+main];p=0
        while struct.unpack_from('<H',d,p)[0]==2:
            _,p=argument(d,p+2,'ARGTYPE_LABEL') if False else (0,p)
            jump,_=argument(d,p+2,'ARGTYPE_LABEL');p=jump
        out=[]
        try:
            for off,name,args in decode(d,p,len(d)):out.append(f'{off:06d} {name} '+', '.join(map(str,args)))
        except Exception as ex:out.append(f'DECODE STOP {ex}: '+out[-1])
        dest=ROOT/'outputs/GTA-LCS-Xbox360/logs/mission-main.txt';dest.write_text('\n'.join(out));print('MAIN',p,'lines',len(out),out[-1]);raise SystemExit(0)
    opcode=struct.pack('<H',entries['COMMAND_SCRIPT_NAME']);needle=opcode+ns.mission.upper().encode()+b'\0'
    starts=[off for off in offsets if off<len(data) and needle in data[off:off+100]]
    if not starts:
        print('Mission starts:',[(i,off,data[off:off+28].hex()) for i,off in enumerate(offsets[:10])]);raise SystemExit('mission not located')
    start=starts[0]+8;end=min([off+8 for off in offsets if off+8>start]+[len(data)])
    out=[]
    try:
        for p,n,args in decode(data,start,end):out.append(f'{p-start:05d} {n} '+', '.join(map(str,args)))
    except Exception as ex:out.append(f'DECODE STOP: {ex} at last '+(out[-1] if out else str(start)))
    text='\n'.join(out);dest=ROOT/'outputs/GTA-LCS-Xbox360/logs'/('mission-'+ns.mission.lower()+'.txt');dest.write_text(text)
    print(f'{start=} {end=} lines={len(out)} output={dest}');print('\n'.join(out[:18]));print(out[-1])
