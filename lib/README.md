# lib/ — os módulos do Tijolo

Cada pasta é uma parte do telefone. O PlatformIO encontra e compila tudo
sozinho; no código basta `#include <Modem.h>`.

| Módulo | O que faz | Fase |
|---|---|---|
| `Modem/` | liga o A7670SA, manda comandos AT, transforma os avisos do modem em eventos (chamada chegando, SMS novo), acompanha a chamada e se reconfigura se o modem reiniciar; avisa quando o modem troca de 4G para 2G e de volta (`+CNSMOD`), também na hora, por uma função (`avisarNaHora`), até no meio de um comando; IMEI e desligar o modem | 1 e 2 |
| `Pdu/` | SMS no modo PDU: alfabeto GSM de 7 bits e UCS2, SMS longos (cabeçalho e montagem das partes), remetente com nome; só `std::string`, testável no computador | 1 |
| `Texto/` | UTF-8 ↔ UCS2 (acentos), campos entre aspas, validação de números | 1 |
| `Toque/` | calcula o "trrrim" de telefone antigo como WAV (8 kHz, 16 bits) | 1 |
| `Teclado/` | teclado 3×4 com os 7 fios em qualquer ordem (aprende o mapa) e os dois botões, de verdade ou no lugar deles o 1 e o 3 segurados; tira o tremido e avisa "segurou"; os pinos são lidos por funções passadas no `begin()`, então testa no computador | 2 |
| `Tela/` | `Quadro`: a imagem da tela na memória, formas e texto com acentos e bordas suaves; `Fontes`: as letras (Inter, geradas por `ferramentas/gerar_fontes.py`); `EstadoTela`: lê o "boletim" que a tela dá de si mesma (RDDST) e classifica a falha, inclusive o painel que para de varrer as linhas (Get_Scanline, 45h); `VigiaTela`: confere a tela pelo boletim, confirma com três leituras espalhadas pelo quadro do 2G, conserta (reenvio, depois RESET), espera cada vez mais quando não adianta e reenvia tudo de minuto em minuto; `TelaIli9341`: manda para a tela as linhas que mudaram e, com o fio MISO, confere o estado dela e conserta na hora (sem o fio, reconfirma às cegas de tempos em tempos) (LovyanGFX, só na placa) | 2 |
| `Medidor/` | lê o 3V3 da placa (com um resistor no IO2) e a bateria milhares de vezes por segundo, numa tarefa no outro núcleo; guarda mínimo, máximo e média em janelas de 100 ms, para o diagnóstico da tela | 2 |
| `Memoria/` | `Sentinela`: vigia a PSRAM com um padrão conhecido (lendo direto do chip, por cima do cache) e avisa se ela perder os dados | 2 |
| `Interface/` | as telas e o que cada tecla faz; conversa com o resto pela classe `Telefone`, sem saber de fios nem de modem | 2 |

Os módulos não leem o `config.h`: recebem pinos e ajustes no `begin()`,
como no robô de mesa.

Testes no computador (sem placa), da raiz do projeto:

```
g++ -std=c++17 -I lib/Texto -I lib/Toque test_host/texto_test.cpp -o /tmp/t && /tmp/t
g++ -std=c++17 -I lib/Texto -I lib/Pdu test_host/pdu_test.cpp -o /tmp/pdu && /tmp/pdu
g++ -std=c++17 -I test_host/shim -I lib/Texto -I lib/Pdu -I lib/Modem test_host/modem_sim_test.cpp lib/Modem/Modem.cpp -o /tmp/sim && /tmp/sim
g++ -std=c++17 -I lib/Teclado test_host/teclado_test.cpp lib/Teclado/Teclado.cpp -o /tmp/tec && /tmp/tec
g++ -std=c++17 -I lib/Texto -I lib/Tela -I lib/Teclado -I lib/Interface test_host/interface_test.cpp lib/Tela/Quadro.cpp lib/Tela/Fontes.cpp lib/Teclado/Teclado.cpp lib/Interface/Interface.cpp -o /tmp/ui && /tmp/ui
g++ -std=c++17 -I lib/Tela -I lib/Medidor -I lib/Memoria test_host/diagnostico_test.cpp -o /tmp/diag && /tmp/diag
g++ -std=c++17 -I lib/Tela test_host/vigia_test.cpp -o /tmp/vigia && /tmp/vigia
```

O `interface_test` salva cada tela como imagem em `test_host/saida/telas/`.

Conferência do `Pdu` contra uma biblioteca independente (precisa de Python):

```
python3 -m venv /tmp/pduenv && /tmp/pduenv/bin/pip install python-gsmmodem-new
/tmp/pduenv/bin/python test_host/pdu_referencia/gerar.py /tmp/vetores.tsv
g++ -std=c++17 -I lib/Texto -I lib/Pdu test_host/pdu_test.cpp -o /tmp/pdu && /tmp/pdu /tmp/vetores.tsv /tmp/nossos.tsv
/tmp/pduenv/bin/python test_host/pdu_referencia/conferir.py /tmp/nossos.tsv
```

Para mudar as letras (outra fonte, outros tamanhos), edite e rode
`python3 ferramentas/gerar_fontes.py` (precisa do Pillow); ele refaz
`lib/Tela/Fontes.cpp`.

Próximos: `Agenda/` e `Mensagens/` (Fase 3).
