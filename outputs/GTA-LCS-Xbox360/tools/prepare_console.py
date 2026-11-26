from pathlib import Path
import hashlib,json,shutil
project=Path(__file__).resolve().parents[1]
workspace=project.parents[1]
source=workspace/'work/lcs/data'
if not (source/'data/main.scm').is_file() or not (source/'models/gta3.img').is_file():
 raise SystemExit('Converted user game data missing: expected work/lcs/data/data/main.scm and models/gta3.img')
target=project/'console-test'
target.mkdir(exist_ok=True)
shutil.copy2(workspace/'work/lcs/build-xsdk/lcs_boot.xex',target/'default.xex')
files=total=0
for item in source.rglob('*'):
 if not item.is_file():continue
 dest=target/'data'/item.relative_to(source)
 dest.parent.mkdir(parents=True,exist_ok=True)
 if not dest.exists() or dest.stat().st_size!=item.stat().st_size or dest.stat().st_mtime_ns!=item.stat().st_mtime_ns:shutil.copy2(item,dest)
 files+=1;total+=item.stat().st_size
(target/'userfiles').mkdir(exist_ok=True)
(target/'performance-log.txt').write_text('Enabled: remove this file to disable performance logging.\n')
checkpoint=project/'test-save/after-snuff/GTAVCsf8.b'
if checkpoint.is_file():
 optional=target/'optional-save-after-snuff';optional.mkdir(exist_ok=True)
 shutil.copy2(checkpoint,optional/'GTAVCsf8.b')
(target/'LEIA-ME.txt').write_text("""GTA LCS - teste de desempenho Xbox 360 / XSDK
Copie esta pasta INTEIRA para o dispositivo do console e execute default.xex.
A pasta data contem arquivos reais, sem links para outras pastas do PC.
Build interna: campanha e 30 FPS no hardware ainda nao certificados.
720p nativo; limite padrao de 30 FPS. Diagnosticos e automacao desligados.

OPCOES DE VIDEO
Pause > Options > Graphics Setup:
- Frame rate cap: 30 FPS ou Up to 60 FPS. Use esquerda/direita no direcional.
- FXAA: ON/OFF. Suaviza bordas do mundo; HUD e menus ficam fora do filtro.
Padroes: 30 FPS, FXAA OFF. Opcoes sao salvas pelas configuracoes do jogo.
O modo ate 60 e experimental: nao certifica desempenho ou fisica a 60 FPS.
Compare 30/OFF, 30/ON, 60/OFF e 60/ON nas mesmas areas e condicoes.
FXAA tem custo e um render target extra de aproximadamente 4 MiB.
A primeira ativacao pode ter custo de alocacao/preparacao.
Icones de mao e armas usam um quadro quadrado com ancoragem a direita.

AJUSTES DE STREAMING
Cache IMG de 1 MiB, leitura antecipada adaptativa ate 64 KiB em sequencia.
Acessos aleatorios usam o tamanho pedido; pedidos grandes sao lidos em blocos
ate 256 KiB. Cache descartado ao fechar/reabrir as imagens.
Efeitos comuns sao aquecidos na carga inicial ate 16 MiB, dentro do LRU de 32 MiB.
Isto transfere trabalho para o carregamento; nao elimina toda pausa de USB.
Texturas mantidas na resolucao original; nao ha troca progressiva baixa/alta.
Passagens de transparencia compartilham o upload, preservando ambas as passagens.

MEDIR
performance-log.txt ativa registros LCS PERF a cada 300 quadros no arquivo
.log criado ao lado do XEX. Remova esse txt para desativar a medicao.
Apos carregar, espere pelo menos 30 segundos e teste andando, dirigindo,
combate, chuva/noite e transicoes de area. Teste varios minutos em cada caso.
Guarde o .log e relate o local de quedas. O primeiro bloco inclui carga.
cap e fxaa identificam o modo ativo em LCS PERF.
PRESENT no console registra flips/vblanks/missed; Xenia offscreen nao os mede.
POSTFX registra a memoria do alvo de FXAA separada das texturas do jogo.
work_avg_ms/max_ms: trabalho por quadro antes do descanso do limitador;
nao e FPS apresentado. over_33ms conta quadros acima do orcamento de 30 FPS.
fence_wait_ms: espera total em fences, nao tempo exclusivo de execucao da GPU.
mesh_original/submitted: vertices por material antes/depois da reducao.
vb_kib/ib_kib: trafego de vertices/indices durante a janela de medicao.
vb_4g_wraps/ib_4g_wraps: cada unidade acrescenta 4194304 KiB ao contador.
ring_wraps: esperas por esgotamento do segmento; ring_peak_kib mostra o pico.
texture_live/peak_kib incluem texturas pendentes de liberacao por fence,
excluem render targets e memoria interna do SDK. ring_reserved_kib e separado.
LCS STREAM mostra acertos/falhas, leituras fisicas e KiB lidos/servidos.
A antecipacao pode ler bytes extras; compare pausas reais em varias areas.
heap_used/arena/free_kib descrevem o heap C; nao sao RAM total/livre do console.

SAVE OPCIONAL
Se optional-save-after-snuff estiver presente, contem um checkpoint de teste apos 4 missoes.
Para usa-lo, copie GTAVCsf8.b para userfiles SOMENTE se seu slot 8 estiver livre
ou se voce ja guardou uma copia do seu save. Depois carregue pelo menu normal.
Nao ha carregamento automatico, teleporte ou scripts de teste nesta pasta.
Dados do jogo vieram da copia fornecida pelo usuario; pasta para teste local.
""",encoding='utf-8')
manifest={'xex_sha256':hashlib.sha256((target/'default.xex').read_bytes()).hexdigest(),'data_files':files,'data_bytes':total,'data_is_real_directory':not (target/'data').is_symlink(),'profiling_enabled':True,'automatic_fixtures':False,'console_fps':'not measured'}
(target/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest))
