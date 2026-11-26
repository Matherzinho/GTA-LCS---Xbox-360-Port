"""Validate the opt-in capture produced by lcs_audio_probe, not arbitrary gameplay."""
from pathlib import Path
import array,json,math,sys,wave
root=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1]) if len(sys.argv)>1 else root/'../../work/lcs/runtime/audio-output.pcm'
raw=source.read_bytes();pcm=array.array('h',raw)
if sys.byteorder!='little': pcm.byteswap()
def rms(start,end,ch):
    values=pcm[int(start*48000)*2+ch:int(end*48000)*2:2]
    assert values,'Capture too short'
    return math.sqrt(sum(v*v for v in values)/len(values))
left=[rms(.3,.8,ch) for ch in range(2)]
right=[rms(1.3,1.8,ch) for ch in range(2)]
speech=[rms(2.6,3.6,ch) for ch in range(2)]
assert left[0]>100 and left[1]<left[0]*.001,('left pan',left)
assert right[1]>100 and right[0]<right[1]*.001,('right pan',right)
assert min(speech)>10,('VAG speech',speech)
with wave.open(str(root/'logs/audio-probe.wav'),'wb') as w:
    w.setparams((2,2,48000,0,'NONE','not compressed'));w.writeframes(raw)
report={'status':'PASS','rate_hz':48000,'channels':2,'seconds':len(pcm)/96000,
        'left_pan_rms':left,'right_pan_rms':right,'VAG_speech_rms':speech,
        'scope':'Actual production mixer output queued to XSDK; not physical-console listening or whole-campaign validation'}
(root/'logs/audio-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
