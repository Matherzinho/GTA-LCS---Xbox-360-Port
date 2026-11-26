# Estado validado — 26/11/2026

Build interna XEX2: `547fa0321cf8b5f60fefa5249a9da65e8ee0637e1b4134e7083c5fe9141c1f56`.

## Evidências

- Inicialização real, menu, cidade, áudio e gameplay exercitados no Xenia.
- Slacker, Dealing Revenge e Snuff concluídas pelos scripts nas rotas testadas; fixtures reposicionam jogador/veículos e fornecem entradas de controle, sem fabricar flags de campanha ou recompensas.
- Save LCX4 com checagem de tamanho, blocos e checksum; carga fria pelo menu, sobrescrita/carga repetida e rejeição de cópia corrompida exercitadas.
- Na regressão após Snuff, carregou 4 missões, progresso 4, dinheiro 1220 e inventário esperado. Foi possível andar, entrar/dirigir/sair do carro e iniciar a missão seguinte. Isso não certifica as missões seguintes.
- Diagnósticos de arquivos/streaming, áudio, modelo, renderização e cena. Testes de streaming incluem dois canais, limites, leituras curtas, reinicialização, janelas de cache, expulsão/revisita e buffers de destino diferentes.
- Stress com 8 carros destruídos e pico de 5 explosões ativas; renderização completa. Houve quadros acima de 33,3 ms: PASS significa conclusão funcional, não 30 FPS estáveis.
- Menu gráfico real alternou 30/até 60 FPS e FXAA ON/OFF; gameplay continuou renderizado nos três modos testados.
- FXAA filtrou 726 pixels de borda na cena sintética, preservando cores planas e overlay desenhado depois. As 15 capturas anteriores do diagnóstico permaneceram idênticas.
- Ícones de mão/armas usam largura e altura iguais em pixels; captura de gameplay revisada visualmente.
- Nenhum símbolo externo indefinido no ELF final. O teste de HAnim não encontrou `mcrf`/`rldic` no caminho corrigido.

## Limites

O Xenia é usado em modo offscreen/readback. Seus tempos incluem emulação e esperas que não representam diretamente o Xbox 360. A carga de outros processos variou entre rodadas; as medidas não são uma comparação controlada do custo de FXAA.

Ainda existem 62 instruções em outros caminhos que o Xenia ignora. Campanha inteira, atividades, física/animações/scripts em 60 FPS, apresentação física e estabilidade prolongada no console não foram certificados.

**30 FPS estáveis em hardware é requisito pendente de medição. Até 60 FPS é um modo experimental, não uma promessa de desempenho.**

Os testes automatizados usam fixtures; o pacote normal deve iniciar sem `video-test.txt`, `stress-test.txt`, `campaign-test.txt`, `safehouse-test.txt`, `gameplay-test.txt`, `reload-test.txt` ou flags de autostart/captura.

## Contadores úteis

`LCS PERF` registra trabalho médio/máximo por quadro, quadros acima de 33,3 ms, limite e FXAA ativos. `PRESENT` registra flips/vblanks/missed em hardware; não existe no modo offscreen. `POSTFX` contabiliza o alvo adicional do filtro. `LCS STREAM` registra leituras/acertos do cache.

Os contadores de heap, texturas e anel são parciais e não equivalem à RAM total/livre do console. Não exponha dados pessoais, saves ou assets nos relatórios públicos.
