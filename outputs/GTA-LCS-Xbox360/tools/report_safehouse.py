import hashlib, json, re, shutil
from pathlib import Path
from validate_native_save import inspect
root=Path(__file__).resolve().parent.parent
ws=root.parent.parent; logs=root/'logs'; runtime=ws/'work/lcs/runtime'
sha=hashlib.sha256((ws/'work/lcs/build-xsdk/lcs_boot.xex').read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
cycles={}
for kind,folder in [('first','safehouse-lcx4-first'),('repeat','safehouse-lcx4-repeat'),('cold','safehouse-lcx4-cold')]:
    prefix='cold' if kind=='cold' else 'warm'
    d=logs/folder
    result=read(d/f'safehouse-{prefix}-result.json')
    run=read(d/f'safehouse-{prefix}-boot-run.json')
    assert result['status']=='PASS' and result['next_mission_started'] and result['mission_save_preserved_slot']
    assert run['sha256']==sha
    text=(d/f'safehouse-{prefix}-lcs_boot.log').read_text(errors='replace')
    assert 'next mission started: vic2' in text
    errors=[float(x) for x in re.findall(r'restored state=1.*?error=([0-9.]+)',text)]
    assert errors and max(errors)<2
    result['maximum_observed_position_error_metres']=max(errors)
    result['next_mission']='vic2 (Slacker)'
    cycles[kind]=result
profile=(logs/'safehouse-lcx4-cold/safehouse-cold-safehouse-profile.txt').read_text().splitlines()
source=runtime/profile[0].split('game:\\',1)[1].replace('\\','/')
record=inspect(source)
assert record['missions_passed']==1
negative=read(logs/'corrupt-native-result.json')
assert negative['status']=='PASS' and negative['xex_sha256']==sha and negative['valid_save_sha256']==record['sha256']
report={'date':'2026-11-26','status':'PASS for first safehouse, repeated overwrite/reload and cold title-menu load',
    'sdk':'open XSDK','xex_sha256':sha,'format':'LCX4','records':record,'cycles':cycles,'corruption_rejection':negative,
    'verified_fields':['health 100','money 100','missions passed 1','progress counter 1 (not percent)','last mission VIC1','outfit/model plr','safehouse visits','3D position','walking','Y vehicle entry/exit','accelerating/steering/braking','next story mission vic2','mission save guard preserves prior slot'],
    'setup':'First mission runs real scripts; test fixture stages at real triggers without changing story flags. Safehouse and load UI use production controller input. Sparse rendering during preparation only; post-load gameplay renders every frame.',
    'limits':['Only the observed first-safehouse scenario; full campaign not certified','No physical Xbox 360 or stable 30 FPS measurement','Partial LCX1/LCX2/LCX3 saves rejected; earlier LCX3 test result superseded because repeated load exposed truncated records','Xenia host GPU memory grew during earlier prolonged rendering; these tests do not certify long emulation sessions']}
(logs/'safehouse-validation.json').write_text(json.dumps(report,indent=2)+'\n')
(logs/'save-records-validation.json').write_text(json.dumps(record,indent=2)+'\n')
(logs/'gameplay-validation.json').write_text(json.dumps({'date':report['date'],'status':report['status'],'xex_sha256':sha,'cycles':cycles,'scope':report['limits'],'save_report':'safehouse-validation.json'},indent=2)+'\n')
state=read(logs/'bootstrap-state.json');state['date']=report['date'];state['boot']['sha256']=sha
state['save']={'status':report['status'],'format':'LCX4','report':'safehouse-validation.json','repeated_load':'PASS','cold_main_menu':'PASS','corrupted_copy':'REJECTED','legacy_formats':'LCX1/LCX2/LCX3 partial records rejected'}
state['gameplay']={'status':report['status'],'xex_sha256':sha,'save_report':'safehouse-validation.json','next_mission':'vic2 (Slacker)','scope':report['limits']}
(logs/'bootstrap-state.json').write_text(json.dumps(state,indent=2)+'\n')
art=read(logs/'build-artifacts.json');xex=ws/'work/lcs/build-xsdk/lcs_boot.xex';assert xex.read_bytes()[:4]==b'XEX2'
art['lcs_boot']={'bytes':xex.stat().st_size,'sha256':sha,'magic':'XEX2'}
(logs/'build-artifacts.json').write_text(json.dumps(art,indent=2)+'\n')
shutil.copy2(source,root/'test-save/GTAVCsf8.b')
shutil.copy2(logs/'safehouse-lcx4-cold/safehouse-cold-safehouse-profile.txt',root/'test-save/profile.txt')
summary='SALVAMENTO - SAFEHOUSE VALIDADA NO XENIA (26/11/2026)\nPASS: gravar no menu real, carregar, sobrescrever e carregar novamente,\nencerrar o Xenia e carregar pelo menu inicial de outro processo.\nRecuperados vida 100, $100, 1 missao concluida, progresso 1, VIC1, roupa plr,\nvisitas a safehouse e posicao. Em todos os ciclos Toni caminhou, entrou,\ndirigiu, freou e saiu do carro; a missao seguinte Slacker (vic2) iniciou.\nTentar salvar durante a missao manteve o arquivo anterior inalterado.\nUma copia com checksum adulterado foi rejeitada antes de carregar a cidade.\n\nCorrigidos os tamanhos de pickups, telefones, guindastes e particulas:\nconstantes herdadas truncavam os registros do LCS e travavam cargas repetidas.\nFormato LCX4, payload 512 KiB. Saves parciais LCX1/2/3 sao rejeitados.\nO resultado anterior LCX3 foi substituido: o teste ampliado revelou a falha.\nOs testes preparam os gatilhos reais da primeira missao por reposicionamento,\nsem fabricar flags da campanha. Render esparso apenas na preparacao;\ngameplay apos carregar renderiza todos os frames.\nVer logs/safehouse-validation.json e logs/save-records-validation.json.\nTestado o primeiro cenario de safehouse; campanha inteira e console pendentes.\n'
status=(root/'STATUS.txt').read_text();a=status.index('SALVAMENTO -');b=status.index('RENDER E LIMITES',a)
status=status[:a]+summary+'\n'+status[b:];status=status.replace('25/11/2026','26/11/2026');(root/'STATUS.txt').write_text(status)
(logs/'SAFEHOUSE-VALIDACAO.txt').write_text(summary+'\nXEX SHA256: '+sha+'\n')
print(json.dumps({'status':'PASS','xex_sha256':sha,'save':str(source),'cycles':list(cycles)},indent=2))
