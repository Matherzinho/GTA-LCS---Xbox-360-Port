"""Apply narrowly reviewed startup-reader endian changes to the work checkout."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[3]/'work/lcs'
source=root/'relcs/src'
reference=root/'reference-ps3/src'

def read(path): return path.read_text(encoding='utf-8')
def write(path,text): path.write_text(text,encoding='utf-8')
def include(text): return text.replace('#include "common.h"','#include "common.h"\n#include "lcs_binary.h"',1)

p=source/'core/FileLoader.cpp'; text=read(p)
if '#include "lcs_binary.h"' not in text:
 text=include(text)
 ref=read(reference/'core/FileLoader.cpp')
 start=ref.index('// .col files are little-endian')
 end=ref.index('void\nCFileLoader::SplitColTrianglesIntoSections',start)
 replacement=ref[start:end]
 assert '__PS3__' not in replacement and 'PS3_' not in replacement
 begin=text.index('void\nCFileLoader::LoadCollisionModel(')
 finish=text.index('void\nCFileLoader::SplitColTrianglesIntoSections',begin)
 text=text[:begin]+replacement+text[finish:]
 text=text.replace('while(CFileMgr::Read(fd, (char*)&header, sizeof(header))){',
  'while(CFileMgr::Read(fd, (char*)&header, sizeof(header))==sizeof(header)){\n\t\theader.ident=LittleU32(header.ident); header.size=LittleU32(header.size);\n\t\tif(header.size<24 || header.size>sizeof(work_buff)) break;')
 text=text.replace('CFileMgr::Read(fd, (char*)work_buff, header.size);','if(CFileMgr::Read(fd, (char*)work_buff, header.size)!=header.size) break;',1)
 text=text.replace('modelsize = header->size;', 'modelsize = LittleU32(header->size);\n\t\tif(modelsize<24 || modelsize>size-8 || modelsize-24>sizeof(work_buff)) return false;')
 text=text.replace("if(header->ident != 'LLOC')", "if(LittleU32(header->ident) != 'LLOC')")
 write(p,text)

p=source/'animation/AnimManager.cpp'; text=read(p)
if '#include "lcs_binary.h"' not in text:
 text=include(text)
 begin=text.index('CAnimManager::LoadAnimFile(RwStream *stream')
 end=text.index('void\nCAnimManager::RemoveLastAnimFile',begin)
 body=text[begin:end]
 # The reference reads the same headers, counters and float keyframe records.
 body=re.sub(r'(RwStreamRead\(stream, (?:\(char\*\))?&(\w+), sizeof\((?:IfpHeader|info)\)\);)',r'\1\n\t\2.size=LittleU32(\2.size);',body)
 body=body.replace('= *(int*)buf;', '= LittleI32(*(int*)buf);')
 body=body.replace('= *(int*)(buf+28);','= LittleI32(*(int*)(buf+28));')
 body=body.replace('seq->SetBoneTag(*(int*)(buf+40));','seq->SetBoneTag(LittleI32(*(int*)(buf+40)));')
 for n in ('0x2C','0x20','0x14'):
  body=body.replace(f'RwStreamRead(stream, buf, {n});',f'RwStreamRead(stream, buf, {n});\n\t\t\t\t\tLittleSwap32(buf,{n}/4);')
 # Reject unexpected info/name/node sizes instead of overflowing the stack buffer.
 body=re.sub(r'(\s*)(RwStreamRead\(stream, buf, (info|name|anim)\.size\);)',r'\1if(\3.size>sizeof(buf)) { printf("LCS IFP: oversized header\\n"); return; }\1\2',body)
 text=text[:begin]+body+text[end:]; write(p,text)

p=source/'renderer/WaterLevel.cpp'; text=read(p)
if '#include "lcs_binary.h"' not in text:
 text=include(text)
 needle='CFileMgr::Read(hFile, (char *)aWaterFineBlockList, sizeof(aWaterFineBlockList));'
 assert needle in text
 text=text.replace(needle,needle+'\n\t\tms_nNoOfWaterLevels=LittleI32(ms_nNoOfWaterLevels);\n\t\tLittleSwap32(ms_aWaterZs,sizeof(ms_aWaterZs)/4);\n\t\tLittleSwap32(ms_aWaterRects,sizeof(ms_aWaterRects)/4);',1)
 write(p,text)

p=source/'control/Script.cpp'; text=read(p)
if '#include "lcs_binary.h"' not in text:
 text=include(text)
 needle='CFileMgr::Read(mainf, (char*)&nLargestMissionSize, sizeof(nLargestMissionSize));'
 assert needle in text
 text=text.replace(needle,needle+'\n\tMainScriptSize=LittleU32(MainScriptSize);\n\tnLargestMissionSize=LittleI32(nLargestMissionSize);',1)
 needle='CFileMgr::Read(mainf, (char*)ScriptSpace, MainScriptSize);'
 text=text.replace(needle,needle+'\n\t{ int32 size=GetSizeOfVariableSpace();\n\t  for(int32 i=8;i+4<=size && uint32(i+4)<=MainScriptSize;i+=4) LittleSwap32(ScriptSpace+i,1); }',1)
 write(p,text)
print('Applied reviewed COL, IFP, water and SCM startup endian changes.')
