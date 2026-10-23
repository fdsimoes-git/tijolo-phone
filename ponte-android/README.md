# Ponte do Tijolo (app Android)

O WhatsApp não roda no Tijolo. Então um Android antigo fica em casa, ligado na tomada e no Wi-Fi, com o **WhatsApp oficial**. Esse Android também roda o app **Ponte do Tijolo**, que faz duas coisas:

```
WhatsApp ── notificação ──▶ Ponte do Tijolo ── SMS "WA#7 Maria / Chego às 8" ──▶ Tijolo
WhatsApp ◀── "Responder" ── Ponte do Tijolo ◀── SMS "WA#7 Ok!" ──────────────── Tijolo
```

- Chegou mensagem no WhatsApp: a ponte lê a notificação e manda uma SMS para o Tijolo. No Tijolo ela aparece como **WhatsApp · Maria**.
- Você responde no Tijolo como responde um SMS. A ponte recebe a SMS e responde no WhatsApp pelo botão "Responder" da notificação, como um relógio inteligente faz.

A conta continua no app oficial: a ponte só lê as notificações e usa o botão de responder.

## Do que precisa

- Um Android 5.0 ou mais novo **que ainda rode o WhatsApp oficial**. Ele fica em casa, na tomada e no Wi-Fi.
- **Um chip só para esse Android**, com SMS. O seu número continua no chip do Tijolo. Um pré-pago com SMS ilimitado é o ideal: cada mensagem do WhatsApp vira uma SMS (veja *Custos*).
- O arquivo `PonteDoTijolo.apk`.

## 1. Passar o WhatsApp para o Android da ponte

1. No celular onde o WhatsApp está hoje, faça o backup das conversas (Configurações → Conversas → Backup), se quiser levar o histórico.
2. No Android da ponte, instale o WhatsApp e entre com **o seu número** (o do Tijolo). O código de confirmação chega por SMS **no Tijolo**: abra em Mensagens. Se o WhatsApp oferecer ligar com o código, atenda no Tijolo.
3. A conta fica neste Android. O WhatsApp do celular antigo sai da conta.
4. No WhatsApp da ponte, deixe as **notificações ligadas** (Configurações → Notificações), para conversas e para grupos. A ponte só vê o que vira notificação, então uma conversa ou grupo **silenciado no WhatsApp não chega ao Tijolo**.
5. Evite abrir as conversas no Android da ponte. Abrir uma conversa marca como lida e apaga a notificação. O que já foi para o Tijolo não muda, mas responder a essa conversa só volta a funcionar quando ela mandar outra mensagem.

## 2. Instalar o app

1. Passe o `PonteDoTijolo.apk` para o Android: cabo USB, Google Drive ou e-mail para você mesmo. Toque no arquivo.
2. O Android pergunta se aquele app (o Arquivos, o Drive...) pode instalar apps: permita.
3. O Play Protect pode avisar que não conhece o app: toque em **Instalar mesmo assim**. O código está neste repositório e você pode montar o seu (veja *Montar o app*).

Se tiver um computador com `adb`, `adb install PonteDoTijolo.apk` também instala. Esse caminho evita a "configuração restrita" do passo 3.

## 3. Configurar (a tela do app)

Abra o **Ponte do Tijolo** e siga a tela de cima para baixo. Cada linha com ✗ diz o que falta.

1. **O número do Tijolo**, com DDD (o do chip que está no Tijolo) → *Guardar o número*.
2. **Permitir SMS** → Permitir.
3. **Acesso às notificações** → ligue o *Ponte do Tijolo* → Permitir.
   - **Android 13 ou mais novo**: se aparecer *Configuração restrita*, volte ao app e toque em **Informações do app**. No menu ⋮ (canto de cima), escolha **Permitir configurações restritas** e repita este passo. Se o SMS também for bloqueado assim, vale o mesmo.
4. **Não economizar bateria** → Permitir. Sem isso, o Android fecha a ponte de madrugada.
   - **Xiaomi / Redmi / POCO**: em Informações do app, ligue *Início automático* e ponha *Economia de bateria* em *Sem restrições*.
   - **Samsung**: Configurações → Bateria → Limites de uso em segundo plano → *Apps que nunca entram em suspensão* → adicione a Ponte do Tijolo.
5. Com tudo ✓, toque em **Mandar uma SMS de teste ao Tijolo**. No Tijolo chega uma mensagem da **Ponte do WhatsApp**.

As SMS que o Tijolo manda para a ponte também aparecem no app de mensagens do Android. Se incomodar, silencie as notificações desse app.

## 4. No Tijolo

No Monitor Serial, diga ao Tijolo qual é o número do chip da ponte:

```
ponte 11977776666
```

Ele fica guardado. Para testar o caminho de ida e volta, use `ponte lista`: a ponte responde com as conversas que já conhece. Se preferir, ponha o número em `PONTE_WHATSAPP` no `config.h`.

## Usando

- **Chegou**: no Tijolo aparece **WhatsApp** em verde e o nome da pessoa ou do grupo. Num grupo, cada linha começa com quem escreveu ("João: bom dia").
- **Responder**: igual a um SMS. Lendo a mensagem, aperte o verde (Responder; sem os botões, segure o 1), escreva e aperte o verde duas vezes (a tela "Enviar para" mostra o nome e "pelo WhatsApp").
- **Opções** (o 5, lendo uma mensagem do WhatsApp): *Silenciar a conversa* (para um grupo animado demais) e *Reativar a conversa*. Lendo um aviso da própria ponte: *Pausar o WhatsApp* (nada mais chega ao Tijolo) e *Retomar o WhatsApp*.
- **Grupos começam desligados**: ligue em *Repassar também os grupos*, no app.
- Mensagens seguidas da mesma conversa vão juntas, numa SMS só. A ponte espera 8 s sem mensagem nova (num grupo, 20 s) e no máximo 30 s (num grupo, 1 minuto).
- **Fotos, áudios, figurinhas**: chegam como `[foto]`, `[áudio]`... com a legenda, se houver. Para ver a foto, só no Android.
- **Emoji**: os comuns viram sinais (`:)` `:D` `<3` `(ok)`). Os outros somem; se a mensagem era só emoji, chega `[emoji]`.
- **Chamada do WhatsApp**: não dá para atender, mas a chamada perdida chega como mensagem (`[Chamada de voz perdida]`).

### O que o Tijolo manda para a ponte

As opções acima mandam estas SMS ao número da ponte. A ponte ignora SMS de qualquer outro número.

| SMS | O que faz |
|---|---|
| `WA#7 texto` | responde na conversa 7 (o número vem na mensagem que chegou) |
| `WA?` | a ponte responde com as conversas recentes e o número de cada uma |
| `WA-` / `WA+` | pausa / retoma tudo |
| `WA-7` / `WA+7` | silencia / reativa só a conversa 7 |

## Limitações (é bom saber)

- **Só dá para responder, não para começar conversa.** A ponte precisa do botão "Responder" de uma notificação daquela conversa. Ela guarda o botão mesmo depois que a notificação some, mas o perde se o Android reiniciar ou fechar o app. Aí a resposta volta a funcionar quando a conversa mandar outra mensagem; até lá, o Tijolo recebe um aviso *"Não consigo responder a ... agora"*.
- **Privacidade**: do WhatsApp até o Android, a criptografia de ponta a ponta continua. **Entre o Android e o Tijolo, as mensagens vão por SMS comum**, que a operadora consegue ler.
- **Custos**: cada SMS longa vai em partes. Sem acento cabem 153 letras por parte; com qualquer acento, 67. A ponte corta cada mensagem em 330 letras (até 6 partes). Num plano sem SMS ilimitado, um grupo movimentado custa caro: silencie pelo Tijolo.
- **A ponte depende do Android**: ligado, na tomada, com Wi-Fi para o WhatsApp e sinal para as SMS.

## Problemas

- **Nada chega no Tijolo**: abra o app no Android. Está tudo ✓? Em *Últimos acontecimentos*, `→ Tijolo: ...` é SMS que saiu. `FALHOU a SMS (motivo)` é crédito, sinal ou modo avião no Android. Se não aparece nada, confira se o WhatsApp está mostrando a notificação no Android: sem notificação, a ponte não vê.
- **"Ainda não está ouvindo"**: desligue e ligue de novo o acesso às notificações (passo 3). Se continuar, reinicie o Android.
- **A resposta não chega no WhatsApp**: o Tijolo recebe um aviso da ponte com o motivo. O mais comum é *"Não consigo responder a ... agora"*: veja *Limitações*.
- **Para de funcionar depois de um tempo**: é a economia de bateria do Android (passo 4).
- **O Tijolo mostra a SMS crua ("WA#7 ...") como SMS comum**: o Tijolo não sabe o número da ponte, ou sabe um número errado (`ponte` no Monitor Serial mostra qual é).

## Montar o app (para quem quiser)

Não precisa de Android Studio nem de Gradle, só das ferramentas de linha de comando. No Ubuntu/Debian:

```
sudo apt install openjdk-17-jdk-headless android-sdk android-sdk-platform-23 \
                 apksigner zipalign aapt dalvik-exchange
sh construir.sh          # roda os testes e monta saida/PonteDoTijolo.apk
sh testar.sh             # só os testes
```

- `src/br/tijolo/ponte/`: o app.
  - `Ouvinte`: lê as notificações e responde.
  - `SmsRecebido`: os pedidos do Tijolo.
  - `Principal`: a tela.
  - `Protocolo`, `Texto`, `Conversas` e `Novidades`: a lógica em Java puro. É ela que decide o formato das SMS, como limpar o texto, o número de cada conversa e o que é mensagem nova.
- `teste/`: a lógica testada no computador. `TestesAndroid` roda o caminho inteiro, de notificação a SMS e de SMS à resposta, com um Android de mentira (`teste/falso/`).
- **A chave que assina o app** fica em `chave/ponte.jks`. Ela é criada na primeira montagem e fica **fora do git**: quem tiver a chave pode fazer uma "atualização" que lê o seu WhatsApp. Guarde uma cópia. Para instalar uma versão nova por cima da antiga, é preciso a **mesma** chave. Com outra chave, o Android só aceita depois de desinstalar a antiga, e aí o app esquece o número do Tijolo e as conversas.
