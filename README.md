# Tijolo — dumbphone com ESP32-S3 e 4G

Celular "tijolinho" montado na **LILYGO T-A7670SA-S3-Standard**: ESP32-S3 + modem SIMCom A7670SA (4G Cat-1 + 2G) na mesma placa, com suporte para 18650, amplificador de áudio e conectores para alto-falante e microfone.

O firmware é o **TijoloOS**, feito por fases:

| Fase | O que entra | Situação |
|---|---|---|
| 1 | Modem: ligar, atender, SMS com acentos, USSD (saldo), toque — pelo Monitor Serial | pronta, testada na placa |
| 2 | Tela ILI9341 + teclado 3×4 (o 1 e o 3, segurados, fazem as vezes dos botões verde e vermelho): tela inicial, discador, chamadas, SMS recebidos, saldo, liga/desliga | pronta, testada na placa |
| 3a | Menu, escrever SMS pelo teclado (multitoque), caixa de mensagens na memória do Tijolo (ler, responder, encaminhar, apagar, reenviar) | **este firmware** |
| 3b | Agenda de contatos (na memória do Tijolo), histórico de chamadas, despertador | próxima |
| 4 | Opcionais: modo roteador (compartilhar o 4G por Wi-Fi) e ponte do WhatsApp | |

O Monitor Serial continua aceitando todos os comandos da Fase 1.

## Peças

- LILYGO T-A7670SA-S3-Standard, versão **Without GPS** (modem A7670SA-LASE), com antena LTE, chip nano-SIM e uma 18650
- Alto-falante 3525 de 8 Ω, microfone de eletreto 9×7 mm e dois cabos JST SH 1.0 de 2 pinos (Fase 1)
- Tela TFT de 2,4" SPI com ILI9341 (240×320, sem touch)
- Teclado matricial 3×4 (12 teclas, 7 fios)
- Protoboard e jumpers (os 2 botões de pressão do kit Eletrogate são opcionais: sem eles, o 1 e o 3 seguradas fazem o papel deles)

## Montagem da Fase 1 (modem e áudio)

1. **Antena primeiro.** Nunca ligue o modem sem antena: transmitindo "no vazio", o amplificador de rádio dele pode queimar.
2. **Chip com a placa desligada.** Se o chip entrar com a placa ligada, o modem não o enxerga até reiniciar.
3. **Alto-falante → conector `SPEK`.** O alto-falante vem com um conector JST de 1,25 mm, que não serve: corte, emende os dois fios num cabo SH 1.0 (solda + termo-retrátil) e encaixe. A polaridade não importa para funcionar. Nunca ligue um fio do alto-falante ao GND: o amplificador (NS4150B) tem saída em ponte.
4. **Microfone → conector `MIC`.** A perninha ligada à carcaça metálica é o **negativo**. Encaixe o cabo SH 1.0 na placa, veja qual fio ficou do lado do **"+"** da serigrafia e solde nele a outra perninha.
5. **Bateria 18650** no suporte (polo + do lado marcado) e chave em **ON**. Na primeira vez a placa só liga pela bateria depois de ser conectada uma vez na USB **ESP-USB**: é o que destrava a proteção da bateria.

## Ligação da Fase 2 (tela e teclado)

Com a chave em OFF e sem USB. Se as barras de pinos vieram soltas, solde-as primeiro. O guia ilustrado (página "Fiação do Tijolo") mostra o mapa das duas barras; os nomes abaixo são os da serigrafia da placa.

**Não use IO1 nem IO41**: eles têm resistores de 5,1 kΩ até a alimentação da câmera, que fica desligada, e um botão ali leria "apertado" o tempo todo.

| Peça | Pino da peça | Pino da placa |
|---|---|---|
| Tela ILI9341 | VCC / GND | 3V3 / GND (os dois de baixo da barra do lado da chave) |
| | CS / RESET / DC | IO47 / IO9 / IO14 |
| | SDI (MOSI) / SCK | MOSI / SCK |
| | LED (luz de fundo) | IO36 |
| | SDO (MISO) | J1-15, MISO (IO13): opcional, recomendado (o Tijolo confere a tela por ele) |
| Teclado 3×4 | os 7 fios, **em qualquer ordem** | IO40, IO39, IO38, IO15, IO16, IO37, IO21 (seguidos, na barra do lado das USB) |
| Botão verde (opcional) | uma perninha no pino, a outra no GND | IO35, e `BOTAO_VERDE = 35` no `config.h` |
| Botão vermelho (opcional) | uma perninha no pino, a outra no GND | IO17, e `BOTAO_VERMELHO = 17` no `config.h` |

**Sem botões** (o padrão): segurar o **1** vale o verde e segurar o **3** vale o vermelho. Com os botões, os dois jeitos funcionam. Nos botões de 4 perninhas, use duas em diagonal. O GND dos botões pode vir do GND de cima da barra do lado das USB.

A tela divide o SPI com o cartão TF da placa (IO11/IO12); o firmware deixa o CS do cartão (IO10) em nível alto.

## Gravar e abrir o monitor

1. Abra a pasta `tijolo` no VS Code com o PlatformIO. Na primeira compilação ele baixa a LovyanGFX (a mesma do robô de mesa).
2. Placa na **ESP-USB**, com a bateria colocada (o modem puxa picos de ~2 A).
3. ✓ (Build), → (Upload) e o ícone da tomada (Monitor).
4. Se a gravação não começar: segure **BOOT**, aperte e solte **RST**, solte **BOOT** e mande gravar de novo.

## Primeira vez com a Fase 2

1. A tela mostra um tijolinho e "Ligando o modem…" (até 1 minuto).
2. **Configurar o teclado**: aperte as teclas que a tela pedir (1, 2, 3… 9, *, 0, #). O Tijolo descobre qual fio é linha e qual é coluna e guarda o mapa. No fim: "Teclado pronto!". Depois que o 1 e o 3 já foram aprendidos, segurar o 1 recomeça e segurar o 3 pula essa etapa.
3. Tela inicial: relógio (a hora vem da operadora), operadora, sinal e bateria.
4. Digite um número e segure o 1 (ligar). Segurar o 3 encerra.
5. Para desligar, segure o 3 na tela inicial por uns 2 segundos e confirme segurando o 1. Para ligar, segure o 3 por 1 segundo.

As bolinhas verde e vermelha no pé da tela mostram o 1 e o 3: a tecla 1 fica embaixo do canto esquerdo da tela, a 3 do direito.

Se o teclado for trocado ou religado em outra ordem: segure o 1 enquanto aparece "Ligando o modem…", ou digite `teclado aprender` no monitor.

## Mensagens (Fase 3)

**Menu**: na tela inicial sem novidades, segure o 1. Lá estão *Escrever mensagem*, *Continuar rascunho* (se você saiu no meio de uma), *Mensagens* (recebidas e enviadas) e *Rediscar*. O 2 e o 8 andam, o 5 (ou o 1 segurado) abre, o 3 segurado volta.

**Escrever** é pelo multitoque, como nos celulares antigos: toque a tecla até aparecer a letra; outra tecla, ou 1 segundo parado, confirma (a letra que ainda pode mudar fica sublinhada).

| Tecla | Letras |
|---|---|
| 1 | . , ? ! : ; ' " - ( ) @ / + = & % * # $ _ < > 1 |
| 2 | a b c á à â ã ç |
| 3 | d e f é ê |
| 4 | g h i í |
| 5 | j k l |
| 6 | m n o ó ô õ |
| 7 | p q r s |
| 8 | t u v ú |
| 9 | w x y z |
| 0 | espaço |
| * | troca o modo: **Abc** (maiúscula no começo de cada frase) → abc → ABC → 123 (o modo aparece no canto de cima) |
| # | apaga a última letra; segurado, a última palavra |

- Segurar uma tecla de letra dá o número dela. O 1 e o 3 não: segurados, eles são o verde e o vermelho (para eles, use o modo 123).
- Embaixo do texto aparece quanto já foi e quanto cabe: 160 letras por SMS sem acento; com qualquer acento ou ç, a mensagem inteira vai em UCS2 e cabem 70. Passando disso, ela vai em partes (até 6, `SMS_MAXIMO_DE_PARTES` no `config.h`), e cada parte é cobrada como um SMS.
- Segure o 1 para escolher o destinatário (digite o número; o # apaga) e segure o 1 de novo para enviar. A tela mostra "Enviando…" e depois "Mensagem enviada", ou o motivo da falha (sem rede, sem crédito...): segure o 1 para tentar de novo. A que não foi fica guardada, com um "!" na lista.
- Segurar o 3 sai do texto sem perder nada: ele vira o rascunho do menu. Escrevendo, a tela apaga mas não sai do lugar.

**Recebidas**: com o Tijolo na tela inicial, a mensagem nova abre sozinha; ela só conta como lida quando alguém toca numa tecla (se ninguém mexer, a tela apaga e ela continua nova). Na tela inicial fica o cartão das novidades: segure o 1 para ver (uma mensagem nova abre direto; várias, a lista) ou segure o 3 um pouco, menos de 1,6 s, para limpar a tela (as mensagens continuam marcadas como novas na lista).

**Lendo**: o 2 e o 8 rolam o texto, o 1 segurado responde, o 5 abre as opções (responder, ligar, encaminhar, apagar; numa que não foi enviada: tentar de novo, editar e enviar) e o 3 segurado volta.

**Onde elas ficam**: na memória flash do Tijolo (a partição de dados de 3,4 MB, com o sistema de arquivos LittleFS), até 500 mensagens (`CAIXA_MAXIMO`); cheia, sai a mais antiga já lida. Depois de guardada, a mensagem sai do chip, que guarda poucas e, cheio, para de receber (`APAGAR_SMS_DO_CHIP` no `config.h`). Gravar um firmware novo não apaga as mensagens (só o "Erase Flash" do PlatformIO apagaria).

**Vindo da Fase 2**: no primeiro liga com a Fase 3, as mensagens que estavam no chip vão para a memória do Tijolo: as que você ainda não tinha visto entram como novas, as outras como lidas. O monitor conta quantas foram.

### Roteiro de teste da Fase 3 (uns 15 minutos)

1. **Gravar e ligar** com o monitor aberto. Na primeira vez aparece "Preparando a memória das mensagens" e depois a contagem: `SMS do chip para a memória do Tijolo: ...` e `Mensagens na memória do Tijolo: N`. `status` mostra a memória usada e `Chip: 0 de ... posições` (o chip esvaziou).
2. **Receber**: mande um SMS de outro celular com o Tijolo na tela inicial. Ele abre sozinho; toque o 8 (conta como lida) e segure o 3. No monitor, `lista` mostra a mensagem e `chip` diz que o chip está vazio.
3. **Nova sem ninguém ver**: mande outra e não toque em nada. A tela apaga em 30 s e volta para o início com o cartão "1 mensagem nova". Segure o 1: ela abre.
4. **Escrever**: tela inicial → segure o 1 (Menu) → 5 (Escrever mensagem). "Oi! Tudo bem?" é 666 444 1111 0 8, *espere 1 s* (o u está na mesma tecla do T), 88 3 666 0 22 33 6 111. Teste um acento (2222 = á), o * (modo) e o # (apaga; segurado, a palavra).
5. **Enviar**: segure o 1, digite o número do outro celular, segure o 1. "Enviando…" e depois "Mensagem enviada". Confira se chegou certo, com os acentos.
6. **Responder**: abra a mensagem recebida (Menu → Mensagens), segure o 1 (Responder), escreva e envie.
7. **Falha**: tire a antena (ou `radio desligar` no monitor), envie uma mensagem: aparece "Não foi possível enviar" e o motivo. Volte a antena (`radio ligar`), segure o 1 (Tentar de novo).
8. **Rascunho**: comece uma mensagem, segure o 3 (sai) e veja "Continuar rascunho" no Menu.
9. **Desligar e ligar** (segure o 3 na tela inicial): as mensagens continuam lá, nas mesmas condições (novas e lidas).

Se algo der errado, mande o que o monitor mostrou (e `lista` e `chip`).

## O que cada tecla faz

| Tecla | Função |
|---|---|
| 1 segurado (0,8 s) = verde | liga, atende; na tela inicial, "Ver" as novidades ou abre o menu; nas listas, abre; escrevendo, envia |
| 3 segurado (0,8 s) = vermelho | encerra ou recusa, apaga o último dígito do discador, volta; na tela inicial com novidades, limpa a tela |
| 3 segurado mais (1,6 s) | apaga o número inteiro e, na tela inicial, pergunta se quer desligar o Tijolo |
| 1 e 3 tocados rápido | os números 1 e 3 (eles aparecem quando a tecla sobe) |
| 0 segurado | "+" no começo do número |
| * ou # segurado | durante a ligação, baixa ou sobe o volume (apertados rápido, são tons) |
| 2 e 8 | andam nas listas e rolam o texto das mensagens |
| 5 | abre o que está selecionado; lendo uma mensagem, as opções |
| `*222#` + verde | códigos que terminam em # vão para a operadora (saldo, recarga, menus) |
| `*#06#` | mostra o IMEI |

A tela escurece depois de 15 s sem uso e apaga em 30 s; a primeira tecla depois disso só acende a tela. Durante a ligação ela apaga em 10 s, mas segurar o 3 encerra mesmo com a tela apagada.

**Desligado pela tecla 3**, o Tijolo fica em sono profundo (ESP32 e tela em repouso, modem desligado) e gasta muito pouco. Para acordar, um dos dois fios da tecla 3 precisa estar num pino que acorda a ESP32 (IO15, IO16 ou IO21; o outro fio fica preso em nível baixo durante o sono). Se não estiver, o Tijolo dorme um sono mais leve, que gasta perto de 1 mA (uma 18650 dura meses assim); trocar dois fios do teclado de lugar e configurar o teclado de novo resolve. Para guardar por muitos dias, use a chave. Com a bateria em 2% por um minuto, ele se desliga sozinho.

**SMS**: veja [Mensagens (Fase 3)](#mensagens-fase-3). Escrever é com as teclas de letra; as mensagens ficam na memória do Tijolo e saem do chip.

**Saldo e menus da operadora** (USSD): a resposta leva alguns segundos; enquanto isso o telefone continua funcionando e segurar o 3 cancela. Sem resposta em 25 s, aparece "sem resposta da operadora".

## Comandos do Monitor Serial

| Comando | O que faz |
|---|---|
| `status` | chip, sinal, rede, operadora, mensagens (na memória e no chip), bateria, hora, tela e teclado |
| `ligar <número>` | liga; aceita `11 99999-8888`, `+55...`, `190`, `*144` |
| `atender` / `desligar` | `desligar` também recusa uma chamada tocando, e sem chamada solta alguma que tenha ficado presa |
| `tons <dígitos>` | teclas durante a ligação, para menus de atendimento (`tons 1`) |
| `sms <número> <texto>` | número sem espaços; com acentos e emoji; texto longo vai em partes (até 15); fica na caixa, como as enviadas pela tela |
| `lista` / `lista tudo` | as mensagens da memória do Tijolo, recebidas e enviadas (as 15 últimas, ou todas), com o número de cada uma (`[12]`) |
| `ler <n>` / `apagar <n>` / `apagar tudo` | mostra inteira (e marca como lida) ou apaga da memória do Tijolo |
| `chip` | os SMS que estão no chip (normalmente nenhum); os longos aparecem inteiros, com as posições de cada parte (`[4,5]`) |
| `chip importar` / `chip ler <posição>` / `chip apagar <posição>` / `chip apagar tudo` | leva os SMS do chip para a memória do Tijolo; lê ou apaga direto no chip |
| `caixa formatar` | apaga a memória das mensagens e começa do zero (pede confirmação); só para quando ela não monta |
| `ussd <código>` | saldo, recarga, menus da operadora; responda a um menu com `ussd 1` |
| `volume <0-7>` / `mic <0-7>` | alto-falante e microfone (o volume fica guardado) |
| `vivavoz sim/nao` | perfil de áudio do modem: viva-voz (bancada) ou fone no ouvido |
| `toque instalar` / `toque testar` | grava e toca o trrrim |
| `pin <senha>` | se o chip pedir PIN (3 erros bloqueiam o chip) |
| `teclado aprender` | a tela pede as 12 teclas de novo |
| `teclado teste` | mostra no monitor cada tecla, botão e par de fios (de novo: para) |
| `tela teste` | faixas coloridas e acentos para conferir a tela |
| `tela reiniciar` | refaz a tela na hora (pulso no RESET), se ela ficar toda branca; o Tijolo também faz isso sozinho a cada troca de rede do modem |
| `tela diag` | diagnóstico da tela branca: estado da tela, falhas anotadas, tensões, rádio (veja abaixo) |
| `tela vigia <ms>` | sem o fio MISO: reenvio às cegas a cada `<ms>` com a luz acesa; `0` desliga (para ver as falhas a olho) |
| `tela spi <MHz>` | velocidade da tela, de 8 a 80, guardada (a placa reinicia); `tela spi padrao` volta à do `config.h`. O SPI divide 80 MHz por um inteiro: 40, 26,7, 20, 16, 13,3, 11,4, 10... |
| `tela faixa` | pinta três faixas (vermelha, verde, azul) direto da RAM interna, sem a imagem do Tijolo, e lê de volta pela MISO |
| `tela rede desligar` / `ligar` | modo de teste da causa: para de refazer a tela nas trocas de rede 4G ↔ 2G e a deixa sempre acesa (para o branco aparecer); `ligar` volta ao normal |
| `memoria` | onde está a imagem da tela, RAM livre e se a PSRAM perdeu os dados |
| `memoria interna` / `memoria psram` | muda onde fica a imagem da tela (a placa reinicia); o padrão é a RAM interna |
| `tela detector desligar` / `ligar` | detector de tensão baixa da própria tela (registrador B7h), para teste; fica guardado |
| `tensao [s]` | 3V3 e bateria a cada segundo por `s` segundos (precisa do resistor no IO2); `tensao parar` |
| `radio desligar` / `radio ligar` | rádio do modem (AT+CFUN=4/1), para teste: desligado, não há rede nem chamadas |
| `amplificador desligar` / `amplificador ligar` | amplificador do alto-falante, para teste |
| `brilho <0-255>` | luz máxima da tela até reiniciar (o fixo fica no `config.h`) |
| `dormir` | desliga o Tijolo (para ligar: segure o 3) |
| `at <comando>` | comando AT direto, para explorar: `at AT+CPSI?` |
| `log sim/nao` | mostra todo o tráfego entre a ESP32 e o modem |
| `reiniciar` | reinicia só o modem |

## Problemas comuns

**Modem e áudio**

- **"O modem não respondeu"**: placa na ESP-USB? Chave em ON? Bateria colocada? Aperte RST.
- **Chip não encontrado**: desligue a chave, tire a USB, encaixe o chip e ligue de novo.
- **Sem serviço**: antena no conector LTE (não no GNSS); teste perto da janela.
- **A ligação não completa, mas SMS funciona**: o A7670SA faz voz pelo 4G (VoLTE) ou caindo para o 2G. `at AT+CNMP?` deve responder `2` (automático); se não, `at AT+CNMP=2`.
- **Ligação muda**: confira o cabo no `SPEK`, `volume 7` e `vivavoz sim`.
- **Microfonia em viva-voz**: baixe `volume` e `mic`.
- **SMS deixou de chegar**: o chip pode estar cheio; veja em `status`. Com `APAGAR_SMS_DO_CHIP = true` (o padrão) ele se esvazia sozinho; `chip importar` leva o que estiver lá para a memória do Tijolo e `chip apagar tudo` esvazia.
- **"A memória das mensagens não montou"**: a partição de dados tem algo que o LittleFS não entende (de outro firmware, ou uma leitura ruim numa queda de tensão). O Tijolo **não apaga nada sozinho**: as mensagens funcionam, mas ficam também no chip e as novas somem da memória ao desligar. Se acontecer toda vez que liga, `caixa formatar` no monitor apaga a memória das mensagens e começa do zero (as que estão no chip voltam). Confira também `board_build.partitions = default_16MB.csv` no `platformio.ini`.
- **"Não foi possível enviar"**: o motivo aparece embaixo. "Falta crédito?" é o que as operadoras respondem no pré-pago sem saldo; com "o chip não tem o número da central de mensagens", confira com `at AT+CSCA?`, que deve mostrar o número da central da operadora.
- **USSD responde ERRO**: alguns firmwares do modem recusam USSD com o VoLTE (IMS) ligado. Mande o que aparece com `log sim` antes de mudar qualquer configuração.

**Tela, teclado e botões**

- **A tela fica branca quando chega ou sai uma ligação**: é a troca de rede do modem (4G ↔ 2G) travando o painel; o Tijolo refaz a tela sozinho logo depois (ela pisca escura e volta). Veja [A causa: a troca de rede do modem](#a-causa-a-troca-de-rede-do-modem).
- **A tela fica toda branca depois do "Ligando", ou de repente** (ela já mostrou imagem): o controlador da tela se perdeu. O resto do telefone continua funcionando e o Tijolo tenta consertar sozinho; no monitor, `tela reiniciar` refaz na hora. Para achar a causa, veja [Diagnóstico da tela branca](#diagnóstico-da-tela-branca). Se acontecer sempre na mesma tecla, confira os fios do teclado: os 7 têm de estar só em IO40, IO39, IO38, IO15, IO16, IO37 e IO21.
- **Tela branca desde que liga, só com a luz acesa**: algum fio de dados (SCK, MOSI, CS, DC, RESET) fora do lugar. O IO47 é vizinho do IO1: fácil de trocar. `tela teste` desenha faixas coloridas.
- **Chuvisco ou riscos**: fios compridos para 40 MHz. No `config.h`, `TELA_SPI_HZ = 27000000`.
- **Vermelho e azul trocados / fundo branco / de cabeça para baixo**: `TELA_TROCAR_RGB`, `TELA_INVERTER_CORES` e `TELA_ROTACAO = 2` no `config.h`.
- **Tela sem luz nenhuma**: confira VCC, GND e o LED no IO36. Ligando o LED direto no 3V3 ela deve acender; se for assim que você quiser deixar, ponha `TELA_LUZ = -1`.
- **"Algo não bateu" ao configurar o teclado, ou teclas mudas**: fio solto ou dois fios no mesmo pino. `teclado teste` mostra o par de fios de cada tecla.
- **Segurar o 1 ou o 3 não faz nada**: `teclado teste` mostra "verde (tecla 1 segurada)" depois de 0,8 s. Se mostrar outra tecla, o mapa do teclado está trocado: `teclado aprender`.
- **Botão (opcional) não responde**: confira `BOTAO_VERDE`/`BOTAO_VERMELHO` no `config.h`; `teclado teste` mostra "verde"/"vermelho"; se nada aparecer, use as outras duas perninhas em diagonal.
- **Não liga segurando o 3**: chave em ON? Segure 1 segundo inteiro. Bateria acabada: conecte o USB. Se nunca ligar pelo 3, aperte RST e veja no monitor, ao desligar, se ele fala do sono leve (fios da tecla 3 fora de IO15, IO16 e IO21).
- **Bateria com porcentagem estranha**: a leitura vem do divisor da placa (IO8). Com o USB conectado, ela mostra a tensão de carga (perto de 100%). Compare com `status`.

## Diagnóstico da tela branca

**O sintoma**: a tela mostra o "Ligando" e fica branca quando o modem liga ou procura rede; o resto do telefone funciona. Branco quer dizer que o controlador da tela (ILI9341) voltou ao estado de fábrica ou recebeu um comando que desliga a imagem: com a luz acesa e sem imagem, este tipo de tela fica branco.

**Achado (22/10)**: no Tijolo do Felipe, o gatilho é a troca de rede do modem (4G ↔ 2G), que acontece em toda ligação. O firmware apaga a luz na troca e refaz a tela; veja [A causa: a troca de rede do modem](#a-causa-a-troca-de-rede-do-modem) e, para resolver na montagem, [Atacar a causa](#atacar-a-causa). As seções abaixo contam o caminho até lá e continuam valendo para outras falhas.

### O que o esquemático da placa mostra

- O 3V3 da tela sai de um regulador de 2 A (U19, SY8089) alimentado pela **mesma fonte do modem** (U3, TPS63020, ajustada em 3,56 V pelos resistores R12/R15, com cerca de 1 mF de capacitores). O manual do A7670 diz que o modem puxa picos de até 2 A ao transmitir. Entre 3,56 V e 3,3 V sobram só 0,23 V: uma queda forte na fonte do modem passa para o 3V3.
- Mas a ESP32-S3 está no mesmo 3V3 e não reinicia: se a tensão cai na placa, não chega a derrubar a ESP32. A tela pode ser mais sensível, porque fica na ponta de fios compridos e finos e atrás do regulador do próprio módulo, que perde mais um pouco. A medição abaixo responde isso.
- **Com a USB ligada, a bateria fica fora do caminho**: um transistor (U12) desconecta a bateria e tudo vem da USB por um diodo (D7). A bateria só entra se a USB fraquejar muito, abaixo da tensão da própria bateria (pelo diodo interno do transistor). A dica "use com a bateria" só vale com a USB desligada.
- Os pinos da tela (IO47, IO14, IO9, IO36) também vão para o conector da câmera, mas só com resistores de 22 a 100 Ω em série. Sem câmera, não atrapalham.
- O README da LilyGO pede uma USB que dê picos de 2 A sem cair abaixo de 5 V. Porta USB de computador costuma dar 0,5 a 0,9 A.

### O que o módulo da tela mostra

Pelo esquemático do módulo de 2,4" (LCDWiki MSP2402, o vermelho com cartão SD) e pelo datasheet do ILI9341:

- **Com VCC de 3,3 V, o jumper J1 tem de estar fechado.** A nota no esquemático do módulo diz: VCC de 5 V, J1 aberto; VCC de 3,3 V, **J1 fechado**. Aberto, a tela inteira passa pelo regulador do módulo (XC6206, "662K"), que com 3,3 V na entrada fica sem folga: perde 0,1 a 0,25 V (até 0,68 V no pior caso do datasheet) e qualquer queda do 3V3 passa direto. E a luz de fundo também puxa a corrente dela por esse regulador.
- O controlador aceita de 2,5 a 3,3 V. Com o J1 aberto, ele trabalha perto de 3,0 a 3,2 V, sem muita margem para quedas.
- O RESET vai direto do pino ao controlador, sem resistor nem capacitor. Um pulso de mais de 10 µs nele reinicia a tela.
- No módulo só há um capacitor de 100 nF na alimentação. Não há nenhum na entrada VCC.
- O datasheet garante o SPI até 10 MHz para escrever e 6,7 MHz para ler. Os 40 MHz do Tijolo funcionam na maioria das telas, mas sem folga, e com fios compridos a folga diminui.
- Um caso parecido, sem solução confirmada, num fórum da Newhaven: uma tela TFT perto de um rádio LoRa de 868 MHz "fica toda branca e trava" quando o rádio transmite, e só volta reconfigurando. A suspeita era o cabo da tela funcionando como antena.

### Suspeitos

1. **Interferência do rádio do modem nos fios da tela.** Ao transmitir, o modem solta até 2 W (2G) ou 0,2 W (4G) pela antena. Jumpers de 15 a 30 cm têm o tamanho certo para captar essas frequências (um quarto de onda a 850 MHz dá 9 cm), e o ruído entra pelo RESET, pelos fios de dados ou pela alimentação da tela. Combina com o que se vê: começa quando o modem começa a transmitir, e a ESP32 não sente nada.
2. **Queda de tensão na tela** nos picos do modem (com o J1 aberto, a tela fica sem margem nenhuma).
3. **Amplificador do alto-falante** (classe D), que liga logo depois do modem.
4. **SPI rápido demais para fios longos**: depois que o modem liga, a tela recebe muito mais desenho.
5. **Mau contato** num fio da tela.

### Duas ligações opcionais (recomendadas)

| O quê | De | Para | Para que serve |
|---|---|---|---|
| Fio | SDO (MISO) da tela | J1-15, MISO (IO13) | o Tijolo lê o estado da tela a cada 0,1 s: percebe quando ela se perde, conserta e anota o tipo de falha |
| Resistor de 10 kΩ | J2-4 (GND) | J2-5, SCL (IO2) | mede o 3V3 da placa milhares de vezes por segundo. A placa já tem 10 kΩ do IO2 ao 3V3; com o seu até o GND, o IO2 fica com metade do 3V3 |

Depois de ligar, reinicie a placa e digite `tela diag`: deve aparecer "Fio MISO: funcionando", a referência `0x84530400` (acordada, imagem ligada, conversor ligado, 16 bits, MADCTL 0x08) e as tensões. O fio MISO pode ficar para sempre. Com ele, uma tela que se perde volta em uns 0,2 s, sem reenviar a imagem inteira a cada 2 s.

Cada falha é confirmada por três leituras iguais, a 1,54 e 3,08 ms da primeira (um terço e dois terços do quadro do 2G, de 4,615 ms). O 2G transmite em janelas curtas dentro de cada quadro, e as três leituras nunca caem todas dentro delas. Leituras que não se confirmam contam como "leituras com ruído". Se uma falha não se resolve, o Tijolo avisa uma vez e tenta de novo cada vez mais espaçado, para não travar o telefone. Se o próprio fio MISO parar de responder, ele avisa, passa para a vigia às cegas e confere o fio de novo a cada 15 s. Se o fio "voltar sozinho", quem falhou foi a alimentação da tela (mau contato no VCC ou no GND). De minuto em minuto, e toda vez que a tela acende, ele também reenvia tudo, para o que o boletim não mostra.

Quando terminar o diagnóstico, **tire o resistor de 10 kΩ**. Ele gasta um pouquinho o tempo todo, até com o Tijolo desligado (0,17 mA), e a medição ocupa um núcleo da ESP32.

Sem o fio MISO, o Tijolo não sabe quando a tela se perde e a vigia às cegas esconde as falhas (a tela volta em até 2 s). Para contá-las a olho, use `tela vigia 0`: cada falha deixa a tela branca até um `tela reiniciar`.

### Roteiro (uns 30 minutos)

Para o modem ligar junto, como no problema, comece com tudo desligado (chave em OFF e sem USB) e ligue. Deixe a bateria colocada e a chave em ON. Se der, use um carregador ou hub com fonte em vez da porta do computador. Se houver um cartão microSD na placa, tire: ele divide os fios SCK, MOSI e MISO com a tela e, sem ser configurado, pode responder sozinho.

**Parte A, sem mexer em nada:**

1. **Base**: ligue e espere 3 minutos. `tela diag` e copie o resultado.
2. **Ligação**: ligue para o seu celular e fique 1 minuto na linha. O modem transmite o tempo todo, e é o pior caso. `tela diag`.
3. **Sem rádio**: `radio desligar`, 2 minutos parado, `tela diag`. A contagem parou de subir? Depois `radio ligar`: as falhas voltam enquanto ele procura a rede?
4. **Antena longe**: afaste a antena da tela e dos fios o quanto o cabo deixar, ou vire a placa para a antena ficar do lado oposto. Repita a ligação. `tela diag`.
5. **Sem amplificador**: `amplificador desligar`, repita a ligação, `tela diag`, depois `amplificador ligar`.
6. **SPI devagar**: `tela spi 10` (a placa reinicia), repita a ligação, `tela diag`, depois `tela spi padrao`.
7. **Tensão** (com o resistor): `tensao 120` e, enquanto ele mostra, `radio desligar` e `radio ligar`, ou faça uma ligação. Veja se o mínimo do 3V3 cai quando as falhas aparecem. Se a bateria também cair com a USB ligada, a USB não aguentou o pico e a bateria entrou para ajudar.

**Parte B, um conserto de cada vez**, repetindo o passo 2 (ligação de 1 minuto) e `tela diag` depois de cada um:

8. **Fechar o J1** da tela com uma gota de solda (veja o aviso abaixo).
9. **Cerâmicos de 100 nF** nos pinos da tela: um entre VCC e GND (junto do eletrolítico de 100 µF) e outro entre RESET e GND.
10. **Fios da tela curtos** e longe da antena.

Cada falha também aparece sozinha no monitor, na hora, com o tipo, a pista, as tensões do último segundo e o estado do rádio.

### A causa: a troca de rede do modem

Com o fio MISO ligado (22/10), a tela ficou branca enquanto o controlador dizia estar acordado, com a imagem ligada e a configuração intacta, e a vigia não viu nenhuma falha. Os testes seguintes acharam o gatilho:

| Teste | Resultado |
|---|---|
| Ligação para o Tijolo | branca assim que começa a tocar, também no mudo e com o amplificador desligado |
| `at AT+CPSI?` parado e tocando | parado: LTE (4G); tocando: GSM (2G) |
| `at AT+CNMP=13` (só 2G), sem ligação nenhuma | branca assim que o modem respondeu OK; `tela reiniciar` trouxe a imagem; branca de novo ao voltar para o automático (4G) |
| `at AT+CNMP=38` (só 4G) e ligação | a tela não ficou branca, mas a ligação não chegou ("fora de área") |
| `tela faixa` com a tela branca | a memória da tela tem as três faixas; o painel é que não mostra |
| Imagem na RAM interna, com a sentinela da PSRAM | branca do mesmo jeito; a PSRAM não perdeu nada |
| Antena longe, brilho 40, SPI a 10 e 20 MHz, detector de tensão da tela desligado, J1 fechado | branca do mesmo jeito |
| Noite de 22/10, sem os botões, firmware novo, `tela rede desligar`: `AT+CNMP=13`, SMS e chamada recebida no 2G, `AT+CNMP=2` | **não ficou branca** nenhuma vez; boletim certo e varredura andando |
| Logo depois, tela sempre acesa: 6 idas ao 2G (4 em 850 MHz, 2 em 1800 MHz) e 6 voltas ao 4G | branca **uma vez**, numa das primeiras idas ao 2G (850 MHz). Durante o branco a vigia não viu nada: boletim certo e a varredura do painel andando |
| Em seguida, uma ligação para o Tijolo (4G → 2G para tocar) | **branca**. O modem não avisou a troca (`+CNSMOD`) nem na ida nem na volta |
| Mais três ligações, com `AT+CREG=2`/`AT+CEREG=2` e `log sim` na 1ª e na 3ª | branca **só na 2ª**. O log mostrou os avisos de registro chegando antes do `RING` (`+CEREG: 0`, `+CREG: 0`, `+CREG: 1`) e o `+CEREG: 1` na volta ao 4G |
| Firmware que apaga a luz no `+CEREG: 0`, uso normal: 3 ligações recebidas (uma atendida) e 3 feitas | **nenhuma branca**: a tela escurece e volta já com a chamada. Do aviso ao `RING` passam ~1,4 s |

**Toda troca de tecnologia do modem (4G ↔ 2G) trava o painel da tela em branco.** O controlador continua com a configuração e com a imagem na memória; o que deixa de funcionar é o painel. Só um RESET da tela seguido da configuração (`tela reiniciar`) destrava, e só depois que a troca passou. Nesta operadora o modem não faz voz pelo 4G (VoLTE), então **toda ligação, recebida ou feita, cai para o 2G** e volta para o 4G quando termina. Deixar o modem só no 4G evita a troca, mas aí as ligações não chegam: mantenha `AT+CNMP=2` (automático).

O que o firmware faz enquanto a causa não é resolvida na montagem:
- pede ao modem que avise cada troca de tecnologia e de registro (`AT+CNSMOD=1`, `AT+CREG=1`, `AT+CEREG=1`). No instante do aviso (até no meio de outro comando), apaga a luz da tela; 1,5 s depois do último aviso refaz a tela e acende de novo (no máximo 5 s no escuro). Em vez de branca, a tela fica escura por um ou dois segundos;
- quando uma chamada chega com a tela apagada pela troca, refaz na hora e ela acende já mostrando quem liga. Os recomeços "de reserva" (2,5 s depois do toque, 1 s depois de atender, no fim da chamada e ao ligar) saíram em 22/10: com os avisos de registro, as trocas das chamadas já são vistas na hora;
- toda vez que a luz apaga, refaz a tela no escuro, também com o fio MISO;
- com o fio MISO, confere a cada 0,1 s se o painel continua varrendo as linhas (comando Get_Scanline, 45h). Se a varredura parar, é "painel parou": o Tijolo dá RESET na hora e anota no monitor. **Este travamento não para a varredura** (teste de 22/10): o controlador da tela continua funcionando e quem trava é a parte analógica que gera as tensões do vidro. Nem o boletim nem a varredura mostram; só o olho vê.

Na chamada, o modem cai do 4G para o 2G sem mandar `+CNSMOD` (ele só avisa trocas pedidas com `AT+CNMP`), mas os avisos de registro chegam antes do `RING` (log de 22/10): `+CEREG: 0` (saiu do 4G), `+CREG: 0`, `+CREG: 1` (registrou no 2G) e só então `RING`. Por isso o Tijolo também escuta o `+CEREG` e o `+CREG`: a luz apaga no `+CEREG: 0` e a tela da chamada aparece já refeita. Quando a chamada acaba, o `+CEREG: 1` (de volta ao 4G) faz o mesmo.

O monitor mostra cada troca e cada conserto com a hora (`[rede: 2G às 3:12.4]`, `[tela refeita às 3:13.6: rede mudou para 2G]`, `TELA [3:12.5]: painel parou…`), então dá para ver o que veio primeiro. `tela diag` mostra as contagens e se a varredura está andando. Para parar os consertos por troca: `tela rede desligar` (até reiniciar a placa) ou `TELA_REFAZER_NA_TROCA_DE_REDE = false` no `config.h`.

A troca só sumiria de vez com VoLTE (voz pelo 4G). No Brasil, as operadoras só liberam VoLTE para aparelhos homologados, e o teste com `AT+CNMP=38` ("fora de área") mostra que ele não está ativo para este modem.

### Atacar a causa

Logo depois de passar para o 2G, o modem transmite com a potência máxima (até 2 W, em rajadas de 0,58 ms repetidas 217 vezes por segundo, com picos de ~2 A) até a rede mandar baixar, alguns segundos depois; no 4G o máximo é 0,2 W. É o momento mais barulhento do rádio. Numa ligação recebida ele transmite assim por mais tempo (atende a rede, monta a chamada, toca) do que numa troca pedida com `AT+CNMP` (só se registra), e é na ligação que a tela trava quase sempre. Depois que os botões saíram, as trocas com `AT+CNMP` passaram a travar bem menos: os fios deles corriam junto dos da tela, mais uma pista de que os fios captam o rádio. A ESP32, na mesma alimentação, não sente nada; o painel da tela sente. A perturbação pode chegar a ele por três caminhos:

1. **alimentação**: o 3V3 da tela sai da mesma fonte do modem, com só 0,23 V de folga;
2. **terra**: os picos de 2 A do modem voltam pelo mesmo GND que a tela usa de referência, e um fio de GND fino e comprido "sobe e desce" junto;
3. **rádio pelos fios**: jumpers de 10 a 20 cm são antenas para 850 a 1900 MHz. Afastar a antena não resolveu, mas a placa e o cabo da antena também irradiam.

**Gatilho de teste** (repete o problema em segundos, sem ligação), com o monitor aberto:
1. `tela rede desligar`: o Tijolo para de consertar sozinho na troca e deixa a tela sempre acesa (apagada, o branco não aparece);
2. `at AT+CNMP=13` (só 2G): a tela fica branca? Veja se o monitor mostrou "painel parou" (a vigia percebeu e consertou) e rode `tela diag`;
3. se ela continuar branca, `tela reiniciar`. `at AT+CNMP=2` volta ao automático, e é outra troca (outro teste);
4. no fim, `tela rede ligar`.

Depois de cada troca, o monitor mostra em que faixa o modem ficou (`[rede às …: 2G, canal 52 (900 MHz) …]`). No 2G a potência máxima depende da faixa: 2 W em 850 e 900 MHz, 1 W em 1800 MHz. Se a tela só travar numa faixa, a causa é a potência do rádio.

**Primeiro, a troca ou a transmissão?** Ainda só no 2G (`at AT+CNMP=13` e `tela reiniciar`), mande um SMS (`sms <seu número> teste`) e ligue para o Tijolo de outro telefone. Os dois transmitem no 2G sem trocar de rede. Se a tela travar, quem trava é a transmissão em potência máxima; se não travar, é a troca em si.

**Depois, um conserto de cada vez**, repetindo o gatilho 3 vezes (13 e 2, ida e volta) depois de cada um:

| Mudança | Se resolver, o caminho era |
|---|---|
| Nos pinos da própria tela: um cerâmico de 100 nF entre VCC e GND com um de 100 pF (ou 1 nF) em paralelo, e outro de 100 nF entre RESET e GND | alimentação ou rádio chegando pelos pinos (o de 100 nF segura os picos; o de 100 pF é o que funciona nas frequências do rádio) |
| Um segundo fio de GND da tela até a placa (no GND da outra barra) e os fios da tela curtos (menos de 10 cm), trançados ou correndo junto de um GND | terra, ou os fios captando rádio |
| Só para descobrir: tire o VCC da tela do 3V3 da placa e ligue-o no + de 2 pilhas AA (3 V); o – das pilhas no GND, e o GND da tela continua ligado à placa. **Nunca as pilhas e o 3V3 ao mesmo tempo** | alimentação. A solução definitiva aí é um filtro no VCC da tela (ferrite + capacitor) ou um regulador só para ela |
| Tela e fios do lado oposto da placa em relação ao modem e ao cabo da antena | rádio irradiado pela placa |

Quando o gatilho não travar mais nenhuma das 3 vezes, a causa está resolvida e os consertos por troca podem até ser desligados (`TELA_REFAZER_NA_TROCA_DE_REDE = false`).

### Como ler o resultado

| O que aparece | Causa provável | O que fazer |
|---|---|---|
| Falhas só com o rádio ligado, e bem menos com a antena longe | interferência do rádio nos fios | antena longe da tela e dos fios; fios curtos; cerâmico de 100 nF entre VCC e GND e entre RESET e GND, nos pinos da própria tela |
| Falhas junto com o 3V3 caindo (mínimo bem abaixo de 3,2 V) | queda de tensão | fechar o J1 da tela (abaixo); fios de VCC e GND curtos; capacitores na tela |
| "voltou ao estado de fábrica" | a tela reiniciou: queda na alimentação dela ou pulso no fio RESET | fechar o J1; 100 nF entre RESET e GND na tela; as duas linhas acima |
| "entrou em repouso", "desligou a imagem" ou "configuração mudou" | comando falso, por ruído nos fios de dados | fios curtos, longe da antena; `tela spi` mais baixo |
| "conversor de tensão interno parou" | alimentação da tela fraca ou com ruído (o controlador tem um detector de tensão baixa, que vem ligado) | fechar o J1; capacitores na tela; fios de VCC e GND curtos. Teste: `tela detector desligar`; se esse tipo de falha sumir, era o detector desligando a tela nas quedas |
| Falhas também com o rádio desligado | amplificador, SPI ou mau contato | passos 5 e 6; trocar os jumpers da tela |
| Muitas "leituras com ruído" | ruído no fio MISO ou no SCK | fios curtos |
| "painel parou de varrer as linhas" | a troca de rede (ou a transmissão do 2G) travou o painel | o Tijolo dá RESET na hora; para acabar com isso, [Atacar a causa](#atacar-a-causa) |
| Tela branca, `tela diag` diz "tudo certo" e a varredura "andando" | o painel travou de um jeito que nem a varredura mostra | o firmware refaz a tela a cada troca; `tela reiniciar` na hora; [Atacar a causa](#atacar-a-causa) |

**Jumper J1 da tela**: fica atrás da tela, perto do regulador (o componente de 3 pernas marcado 662K). Fechado com uma gota de solda, a tela e a luz de fundo usam os 3,3 V da placa direto, como o esquemático do módulo manda para VCC de 3,3 V. **Depois de fechado, a tela só pode receber 3,3 V. Com 5 V ela queima, e isso vale também se ela voltar para o robô de mesa.** Para desfazer, tire a solda com malha dessoldadora.

## Como funciona

O modem é um computadorzinho à parte. A ESP32 conversa com ele por uma serial (GPIO4 e GPIO5) usando **comandos AT**: `ATD+5511...;` liga, `ATA` atende, `AT+CMGS` manda SMS. Ele responde `OK` ou `ERROR`, e também **fala sem ser perguntado** (as URCs): `RING` quando chega chamada, `+CMTI` quando chega SMS, `+CLCC` a cada mudança na ligação. Use `log sim` para ver a conversa.

Se o modem reiniciar sozinho (bateria fraca, pico de corrente), ele esquece a configuração; o firmware percebe pelo aviso de boot (`*ATREADY`), refaz tudo, inclusive o áudio, e avisa.

- `lib/Modem/`: liga o modem, manda os comandos, separa resposta de URC, transforma URCs em eventos e acompanha a chamada pelo número dela no `+CLCC`. A cada 2 s de chamada, confere com `AT+CLCC`. Também avisa quando o modem troca de tecnologia (`+CNSMOD`: 4G, 2G...), que é quando a tela precisa ser refeita.
- `lib/Pdu/`: os SMS vão e vêm no **modo PDU**, os bytes crus da rede. O SMS comum usa o alfabeto GSM de 7 bits (160 letras, sem ã, õ, â, ê); com esses caracteres o texto vai em **UCS2** (70 por SMS). SMS longos levam um cabeçalho com o número de cada parte, e o firmware junta as partes quando todas chegam.
- `lib/Texto/`: UTF-8 ↔ UCS2 (`ã` = `00E3`), usado no SMS e no USSD.
- `lib/Toque/`: o trrrim é calculado na hora: duas notas de sino (1150 e 1430 Hz) batidas 25 vezes por segundo.
- `lib/Teclado/`: puxa um fio do teclado por vez para o GND e vê qual outro fio desce junto; cada tecla é um **par de fios**. O mapa par → tecla é aprendido uma vez e guardado na memória da ESP32 (Preferences). Tira o "tremido" dos contatos (20 ms) e avisa quando uma tecla é segurada (0,8 s). O 1 e o 3 viram os botões verde e vermelho quando seguradas; por isso o número delas só sai quando a tecla sobe.
- `lib/Tela/`: tudo é desenhado num **quadro** na memória (240×320, 16 bits por ponto, 150 KB na RAM interna) e só as linhas que mudaram vão para a tela, pela LovyanGFX. As letras são da fonte **Inter**, já desenhadas com 16 tons de cinza (bordas suaves) e com os acentos do português; quem gera é `ferramentas/gerar_fontes.py`.
- `lib/Tela/EstadoTela.h`, `lib/Tela/VigiaTela.h` e `lib/Medidor/`: o diagnóstico da tela branca. O ILI9341 responde ao comando RDDST com 32 bits sobre si mesmo (acordada? imagem ligada? conversor interno ligado? formato, orientação); comparando com o que foi configurado, o Tijolo sabe se ela se perdeu e de que jeito. O Medidor lê o 3V3 e a bateria numa tarefa no outro núcleo da ESP32.
- `lib/Interface/`: as telas e o que cada tecla faz em cada uma. Não sabe nada de fios nem de modem: pede "ligue para tal número" a uma classe `Telefone`, que o `main.cpp` implementa com o modem de verdade.
- `lib/Escrita/`: o multitoque (cada tecla, várias letras; a mesma tecla de novo em menos de 1 s troca a letra), com os acentos do português, os modos Abc/abc/ABC/123 e o apagar letra a letra (os acentos ocupam 2 bytes em UTF-8).
- `lib/Caixa/`: as mensagens, um arquivo pequeno por mensagem na partição de dados da flash (LittleFS). O estado (nova, lida, enviada, não enviada) está no nome do arquivo (`0000002a.n`), então marcar como lida é só renomear: se a energia cair no meio, fica o nome velho ou o novo, nunca meio arquivo.
- `src/main.cpp`: liga tudo; a cada poucos segundos lê sinal, rede, operadora, bateria e hora (uma consulta por vez, nunca enquanto alguém digita) e cuida do sono profundo: um dos fios da tecla 3 (ou o botão vermelho) acorda a ESP32.

### Testes no computador (sem placa)

- `texto_test`: conversões de texto; gera `toque.wav` para ouvir.
- `pdu_test`: montagem e leitura dos SMS. Também conferido contra uma biblioteca Python independente (`python-gsmmodem-new`, scripts em `test_host/pdu_referencia/`).
- `modem_sim_test`: o `lib/Modem` contra um A7670 de mentira (chamadas, SMS, USSD, PIN, arquivo, modem que reinicia sozinho, IMEI, desligar, troca de 4G para 2G).
- `teclado_test`: o teclado com os 7 fios embaralhados, contatos tremendo, duas teclas juntas, botões e o 1 e o 3 segurados no lugar deles.
- `interface_test`: as telas com um telefone de mentira; cada tela vira uma imagem em `test_host/saida/telas/`. Também liga um Teclado de verdade (com fios de mentira) à interface: ligar, atender, encerrar e desligar só com o 1 e o 3. Na Fase 3: escrever pelo multitoque, enviar, falhar e tentar de novo, a lista, responder, apagar, o rascunho e o tamanho máximo.
- `caixa_test`: a caixa de mensagens com uma pasta de mentira (guardar, marcar, apagar, religar, caixa cheia, falhas da memória).
- `multitoque_test`: as letras, os acentos, as maiúsculas no começo da frase, os modos, segurar para o número, apagar e o limite.
- `diagnostico_test`: a leitura do estado da tela (cada tipo de falha) e as janelas de tensão do Medidor.
- `vigia_test`: a vigia da tela contra uma tela de mentira: ruído de rádio na leitura (aleatório e o pior caso do 2G, de 1 a 4 janelas por quadro, 30 min cada), falhas que o reenvio conserta, que só o RESET conserta, que não se consertam (espera crescente, 6 horas), fio MISO solto e voltando, ruído no meio de um conserto, rede de segurança e relógio dando a volta.

Todos de uma vez (precisa só do g++), da raiz do projeto:

```
sh test_host/rodar_todos.sh
```

Cada linha de compilação está no começo do arquivo de teste e no `test_host/rodar_todos.sh`.

## WhatsApp e Uber

Não rodam no ESP32: os dois só existem para Android/iPhone (o WhatsApp para KaiOS acabou em fevereiro de 2025), não há API oficial para conta pessoal, e clientes não oficiais do WhatsApp levam a banimento. Os caminhos possíveis estão na Fase 4:

- **Modo roteador**: o tijolo compartilha o 4G por Wi-Fi para um tablet ou notebook (WhatsApp Web, Uber pelo navegador). É um exemplo oficial da Espressif (`ap_to_pppos`). A serial entre a ESP32 e o modem limita a velocidade a algumas centenas de kbps (os pinos de controle de fluxo da placa vêm desligados): dá para mensagens, mapa e Uber, não para vídeo.
- **Ponte do WhatsApp**: um Android antigo em casa, com o WhatsApp oficial, encaminha as mensagens para o tijolo por SMS, e as respostas voltam pelo "responder" da notificação. A conta continua no app oficial.

## Créditos

Letras: fonte Inter, (c) 2016 The Inter Project Authors, SIL Open Font License 1.1 (`lib/Tela/LICENCA-Inter.txt`). Tela: LovyanGFX (MIT/BSD).
