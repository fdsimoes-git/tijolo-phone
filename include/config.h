// ============================================================
//  config.h — pinos e ajustes do Tijolo
//  Placa: LILYGO T-A7670SA-S3-Standard (ESP32-S3 + modem A7670SA)
//  Tudo o que você pode querer mudar sem mexer na lógica fica aqui.
// ============================================================
#pragma once
#include <stdint.h>

// ---------- Ligações internas da placa (já vêm prontas) ----------
// Fonte: utilities.h e esquemático da LilyGo (T-A7670X-S3-Standard Rev1.0)
constexpr int PINO_MODEM_TX     = 4;    // ESP → modem
constexpr int PINO_MODEM_RX     = 5;    // modem → ESP
constexpr int PINO_MODEM_RING   = 6;    // vai a LOW quando chega chamada ou SMS
constexpr int PINO_MODEM_DTR    = 7;    // LOW = modem acordado
constexpr int PINO_MODEM_PWRKEY = 46;   // um pulso liga o modem
constexpr int PINO_FONTE_FORTE  = 42;   // HIGH = fonte da placa sem modo economia (o modem puxa ~2 A nos picos)
constexpr int PINO_BATERIA      = 8;    // divisor 1:2 da 18650 (ADC)
constexpr uint32_t MODEM_BAUD   = 115200;

// O amplificador do alto-falante é ligado por um GPIO DO MODEM (não da ESP):
// AT+CGDRT=3,1 e AT+CGSETV=3,1. Sem isso, a ligação fica muda.
constexpr int MODEM_GPIO_AMPLIFICADOR = 3;

// ---------- Áudio ----------
constexpr int  VOLUME_PADRAO    = 5;     // 0 a 7 (AT+COUTGAIN)
constexpr int  MICROFONE_PADRAO = 5;     // 0 a 7 (AT+CMICGAIN)
constexpr bool VIVA_VOZ_PADRAO  = true;  // true na bancada; false quando o alto-falante for "fone de ouvido"

// ---------- Toque ----------
// Arquivo gerado pelo firmware e gravado na memória do modem ("toque instalar")
constexpr const char* ARQUIVO_TOQUE = "C:/tijolo.wav";
constexpr float VOLUME_TOQUE = 0.5f;     // 0.0 a 1.0

// ---------- SMS ----------
// As mensagens (recebidas e enviadas) ficam na memória do Tijolo, até
// CAIXA_MAXIMO; cheia, sai a mais antiga já lida.
constexpr int CAIXA_MAXIMO = 500;
// O chip guarda poucos SMS (uns 20 a 50) e, cheio, para de receber.
// true: depois de guardado na memória, o SMS sai do chip.
// false: fica também no chip (aí, apague de vez em quando: chip apagar tudo).
constexpr bool APAGAR_SMS_DO_CHIP = true;
// SMS longo: até quantas partes (cada parte é cobrada como um SMS).
// 6 partes = 918 letras sem acento, ou 402 com acento.
constexpr int SMS_MAXIMO_DE_PARTES = 6;

// ---------- Números ----------
// Números com 10 ou 11 dígitos sem "+" ganham o +55 na frente
// (11999998888 → +5511999998888). Ponha false se não quiser.
constexpr bool COMPLETAR_MAIS55 = true;

// ============================================================
//  Fase 2: tela ILI9341 + teclado 3×4 (+ 2 botões, opcionais)
//  Pinos livres nas barras da placa (sem câmera e sem GPS externo),
//  com os nomes da serigrafia (IO47 = GPIO 47).
//  NÃO use IO1 nem IO41: eles têm resistores de 5,1 kΩ até a
//  alimentação da câmera, que fica desligada, e leriam errado.
// ============================================================
// Tela no mesmo SPI do cartão SD da placa (IO11/IO12), com CS próprio.
constexpr int TELA_SCK  = 12;   // na tela: SCK          (barra J1, pino 14)
constexpr int TELA_MOSI = 11;   // na tela: SDI (MOSI)   (J1-13)
constexpr int TELA_CS   = 47;   // na tela: CS           (J1-4)
constexpr int TELA_DC   = 14;   // na tela: DC/RS        (J1-5)
constexpr int TELA_RST  = 9;    // na tela: RESET        (J1-6)
constexpr int TELA_LUZ  = 36;   // na tela: LED          (J1-7); -1 se o LED for direto no 3V3
constexpr int PINO_SD_CS = 10;  // CS do cartão SD da placa: fica em HIGH para o SD não atrapalhar

constexpr uint32_t TELA_SPI_HZ = 40000000;  // se aparecer chuvisco ou cores erradas: 27000000
constexpr uint8_t  TELA_ROTACAO = 0;        // 0 = em pé; 2 se a imagem ficar de cabeça para baixo
constexpr bool TELA_INVERTER_CORES = false; // fundo branco em vez de preto? mude para true
constexpr bool TELA_TROCAR_RGB = false;     // vermelho e azul trocados? mude para true
constexpr uint8_t  BRILHO_MAXIMO = 255;     // 0 a 255; menos = bateria dura mais
constexpr uint32_t TELA_LUZ_HZ = 12000;     // PWM do brilho
// Uma queda de tensão ou ruído nos fios faz a tela esquecer a configuração
// e ficar toda branca. Para o Tijolo perceber e consertar:
//  - fio MISO (opcional, recomendado): SDO(MISO) da tela → J1-15 (MISO,
//    IO13). Com ele, o Tijolo lê o estado da tela a cada TELA_CONFERIR_MS,
//    conserta na hora e anota cada falha (comando "tela diag"). Sem o fio,
//    ele percebe sozinho e usa o modo às cegas abaixo.
//  - às cegas (sem o fio): com a luz acesa, reconfirma a configuração e
//    reenvia a imagem a cada TELA_VIGIA_MS (~35 ms; a imagem não pisca).
//    0 = desligado.
// Quando o modem troca de tecnologia (4G ↔ 2G: toda chamada cai para o 2G
// nesta operadora), o painel da tela pode travar branco, com o controlador
// dizendo que está tudo certo. O Tijolo apaga a luz no aviso da troca e
// refaz a tela (RESET) no escuro: ela escurece um instante em vez de ficar
// branca. false desliga.
constexpr bool     TELA_REFAZER_NA_TROCA_DE_REDE = true;
constexpr int      TELA_MISO = 13;
constexpr uint32_t TELA_CONFERIR_MS = 100;
constexpr uint32_t TELA_VIGIA_MS = 2000;

// Medição do 3V3 (opcional, para o diagnóstico da tela branca): um resistor
// de 10 kΩ entre J2-4 (GND) e J2-5 (IO2/SCL). A placa já tem 10 kΩ do IO2
// para o 3V3, então o IO2 fica com metade do 3V3. Sem o resistor, nada muda.
constexpr int PINO_MEDIR_3V3 = 2;

// Teclado 3×4: os 7 fios, EM QUALQUER ORDEM (o Tijolo aprende qual é qual).
// Estão em sequência na barra J2, pinos 8 a 14.
constexpr int TECLADO_FIOS[7] = {40, 39, 38, 15, 16, 37, 21};
constexpr const char* NOMES_FIOS[7] = {"IO40", "IO39", "IO38", "IO15", "IO16", "IO37", "IO21"};

// Verde (ligar, atender) e vermelho (encerrar, voltar, apagar; segurado
// desliga o Tijolo). Sem botões: segurar a tecla 1 vale o verde e segurar
// a tecla 3 vale o vermelho; tocadas rápido, continuam sendo 1 e 3.
// 0 = sem tecla no lugar do botão.
constexpr char TECLA_COMO_VERDE    = '1';
constexpr char TECLA_COMO_VERMELHO = '3';
// Botões de verdade (opcionais): um lado no pino, o outro no GND (pull-up
// interno da ESP32). -1 = sem botão. Com eles: verde 35 (J1-8) e vermelho
// 17 (J1-10). Os dois jeitos funcionam juntos.
constexpr int BOTAO_VERDE    = -1;
constexpr int BOTAO_VERMELHO = -1;
// Ligar: segure o 3 (ou o vermelho) por este tempo. Quem acorda a ESP32 do
// sono profundo é um pino RTC (GPIO 0 a 21): o vermelho, ou um dos dois
// fios da tecla 3 (IO15, IO16 ou IO21). Se nenhum fio da tecla 3 cair num
// desses, o Tijolo "desligado" dorme um sono mais leve (gasta ~1 mA).
constexpr uint32_t SEGURAR_PARA_LIGAR_MS = 1000;
