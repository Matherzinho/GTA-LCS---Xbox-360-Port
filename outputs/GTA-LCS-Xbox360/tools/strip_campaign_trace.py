"""Remove temporary engine call-site tracing after a diagnosed mission regression."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[3]
files=['core/Game.cpp','core/main.cpp','core/World.cpp','control/Script.cpp','peds/PlayerPed.cpp','peds/Ped.cpp','peds/PedFight.cpp','animation/AnimManager.cpp','animation/AnimBlendAssocGroup.cpp','animation/AnimBlendAssociation.cpp']
for name in files:
 p=root/'work/lcs/relcs/src'/name;s=p.read_text()
 s=re.sub(r'\A#ifdef GTA_LCS_XSDK\nextern "C" void lcs_campaign_debug_.*?#endif\n','',s,flags=re.S)
 s=re.sub(r'#ifdef GTA_LCS_XSDK\n\s*lcs_campaign_debug_command\(m_nIp-2,command\);\n#endif\n','',s)
 s=re.sub(r'#ifdef GTA_LCS_XSDK\n\s*if\(hierarchy->totalLength<=0\)printf\([^\n]+\n#endif','',s)
 while True:
  m=re.search(r'\((LCS_[A-Z]+\([^)]*\)),',s)
  if not m:break
  start=m.start();end=m.end();level=1
  while level:
   if s[end]=='(':level+=1
   elif s[end]==')':level-=1
   end+=1
  s=s[:start]+s[m.end():end-1]+s[end:]
 s=re.sub(r'LCS_[A-Z]+\(\d+\);','',s)
 assert 'LCS_PHASE' not in s and 'LCS_ANIM' not in s and 'lcs_campaign_debug_' not in s,name
 p.write_text(s,newline='\n')
p=root/'outputs/GTA-LCS-Xbox360/platform/xsdk_boot.cpp';s=p.read_text();s=re.sub(r'extern "C" void lcs_campaign_debug_step\(unsigned\);\n','',s);s=re.sub(r'lcs_campaign_debug_step\(\d+\);','',s);p.write_text(s,newline='\n')
print('Removed temporary engine trace call sites')
