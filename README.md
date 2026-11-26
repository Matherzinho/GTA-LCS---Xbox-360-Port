# GTA Liberty City Stories — Xbox 360 / XSDK

Port nativo em desenvolvimento para Xbox 360, usando o **XSDK aberto**, reLCS/reStories e librw. Trabalho do port: **Matherzin / Matherzinho**.

**Build interna: não é uma versão estável nem uma campanha totalmente certificada.** O alvo é 1280×720 nativo, 16:9 sem distorção e 30 FPS estáveis no console. O modo de até 60 FPS é experimental. As validações descritas foram feitas no Xenia; desempenho, apresentação e comportamento completo em hardware físico ainda precisam ser medidos.

## O que está implementado

- Inicialização real, menu, cidade, introdução e gameplay básico com controle.
- Renderizador Xenos pelo XSDK, geometria, skinning/animações, iluminação, transparência e correções de hierarquia.
- Áudio nativo: efeitos PCM, rádio/falas/cutscenes VB/VAG estéreo e mixer a 48 kHz.
- Salvamento LCX4, carregamento pelo menu, restauração de progresso/inventário e proteção contra saves durante missões.
- Testes de Slacker, Dealing Revenge e Snuff, com ciclos de salvar/carregar.
- Cache de leitura IMG de 1 MiB, antecipação sequencial limitada, prewarm de efeitos comuns no LRU e instrumentação de memória/streaming.
- Redução do intervalo de vértices enviado por material, upload compartilhado nas passagens de alfa e projeção calculada uma vez por objeto.
- FXAA opcional aplicado ao mundo antes do HUD/menus. Ícones da mão e armas com proporção quadrada.
- **Options → Graphics Setup:** limite de 30 FPS ou até 60 FPS, FXAA ON/OFF. Padrões: 30 FPS e FXAA desligado.

A primeira implementação está em inglês; PT-BR fica para uma etapa posterior.

## Conteúdo do repositório

| Caminho | Conteúdo |
| --- | --- |
| `outputs/GTA-LCS-Xbox360/platform/` | Backend nativo, GPU/shaders, áudio, streaming, input e fixtures de teste |
| `outputs/GTA-LCS-Xbox360/CMakeLists.txt` | Build XSDK |
| `outputs/GTA-LCS-Xbox360/tools/` | Compilação, validações, execução no Xenia e preparação da pasta do console |
| `outputs/GTA-LCS-Xbox360/patches/` | Diffs das bases utilizadas |
| `work/lcs/relcs/` | Snapshot completo dos fontes usados, com as alterações aplicadas |
| `work/lcs/relcs/vendor/librw/` | Snapshot da dependência gráfica com as correções locais |
| `artifacts/default.xex` | XEX2 interno correspondente ao estado documentado, sem assets embutidos |
| `docs/` | Proveniência, validação e hashes dos fontes |

A estrutura conserva os caminhos dos scripts existentes. Não é necessário reaplicar os patches sobre os fontes deste repositório: eles já estão modificados.

## Compilar

Requisitos: Linux/WSL, CMake, Ninja, Python 3 e [XSDK](https://github.com/xenosworks/xsdk) com seu compilador PowerPC e ferramentas de shader instalados. O ambiente usado possuía GCC **15.3.0**. Não usa o XDK da Microsoft.

Na raiz do clone:

```sh
export XSDK_ROOT="$HOME/xsdk"
bash outputs/GTA-LCS-Xbox360/tools/build_boot.sh
```

Saída: `work/lcs/build-xsdk/lcs_boot.xex`. O script também verifica o codegen da hierarquia de animação contra as instruções que o Xenia ignora nesse caminho.

## Dados e execução

**Não estão incluídos ISO, ROM, assets comerciais, áudio do GTA, saves pessoais ou o conversor de assets.** O XEX precisa dos arquivos externos do jogo; não contém o jogo comercial completo embutido.

Use seus próprios dados de LCS PS2, convertidos conforme as instruções da [base reLCS/reStories](work/lcs/relcs/README.md), em `work/lcs/data/`. A organização esperada inclui `data/`, `models/`, `text/` e `audio/` dentro dessa pasta. As texturas específicas de botões/frontend também precisam ser fornecidas no conjunto de dados; o validador lista os arquivos exigidos.

```sh
python3 outputs/GTA-LCS-Xbox360/tools/validate_data.py
python3 outputs/GTA-LCS-Xbox360/tools/prepare_console.py
```

O segundo comando prepara `outputs/GTA-LCS-Xbox360/console-test/` com `default.xex`, os dados reais e instruções. O checkpoint de teste só é incluído se houver um fixture local; ele não é necessário para preparar ou executar o jogo.

Para usar o XEX publicado sem compilar, copie `artifacts/default.xex` e sua pasta de dados convertidos para o dispositivo:

```text
GTA-LCS/
  default.xex
  data/
    data/
    models/
    text/
    audio/
  userfiles/
```

Não sobrescreva seus saves ao atualizar o executável. Para registrar desempenho, crie `performance-log.txt` ao lado do XEX; remova-o para desligar o registro. Compare os modos 30/OFF, 30/ON, 60/OFF e 60/ON nas mesmas áreas. FXAA tem custo e um alvo de imagem adicional; não é uma operação gratuita.

## Testes no Xenia

Os runners Windows esperam `work/xenia/app/xenia_canary.exe`, ou `-XeniaPath` quando o script oferece esse parâmetro. Prepare `work/lcs/runtime/data` com os seus dados antes de rodar. Os testes automáticos são opt-in e não devem acompanhar a pasta usada normalmente no console.

- `tools/test_xenia.ps1`: diagnósticos disponíveis no build (precisam dos XEXs correspondentes).
- `tools/start_boot.ps1`: menu normal; `-City` inicia uma partida.
- `tools/watch_safehouse_test.ps1`: acompanha a regressão de safehouse.
- `tools/start_campaign_test.ps1`, `start_stress_test.ps1` e `start_video_test.ps1`: exigem um perfil/checkpoint de teste local, passado por `-Profile`. Os fixtures privados não são publicados.

Os caminhos acima são relativos a `outputs/GTA-LCS-Xbox360/`. Veja [validação e limites](docs/VALIDACAO.md) antes de interpretar um resultado PASS como certificação do jogo inteiro.

## Bases e créditos

Veja [proveniência](docs/UPSTREAM.md). Os avisos e créditos dos projetos de origem foram preservados, incluindo a licença MIT da librw. Nenhuma licença global nova é imposta às bases de terceiros.
