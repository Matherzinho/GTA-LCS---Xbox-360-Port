# Proveniência

| Componente | Origem | Base usada |
| --- | --- | --- |
| reLCS/reStories | https://github.com/knackers4/res | `83ae2fee63403a9d4f0cd9a686d282abccdaea25` |
| librw | https://github.com/aap/librw | `895556fd81f2b78fce092f1f0bc4c992b2bbb5b7` |
| Referência PowerPC/PS3 consultada | Referência local utilizada na análise de endian/startup | `2a0111291f0fc35330c82940174c9b7b5c0e3233` |
| SDK | https://github.com/xenosworks/xsdk | Instalação local aberta, toolchain GCC 15.3.0 |

Os snapshots reLCS e librw contêm as alterações do port. A dependência librw está incluída como arquivos, não como submódulo; os submódulos de áudio não utilizados pelo backend XSDK não são necessários para este build. Executáveis de premake, bibliotecas binárias, headers Miles não usados no XSDK e dados binários de exemplo foram omitidos.

Os patches em `outputs/GTA-LCS-Xbox360/patches/` documentam as diferenças em relação às bases. Não reaplicá-los sobre o snapshot já modificado. O manifesto `source-snapshot.json` identifica por SHA-256 os arquivos copiados do workspace testado.

Os créditos e avisos da base estão em `work/lcs/relcs/README.md`; a licença da librw está em `work/lcs/relcs/vendor/librw/LICENSE`. Os nomes GTA/Liberty City Stories pertencem aos seus titulares. Este repositório não contém os dados comerciais do jogo.
