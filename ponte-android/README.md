# Ponte do Tijolo (app Android)

O WhatsApp não roda no Tijolo. Então um Android antigo fica em casa, ligado na tomada e no Wi-Fi, com o **WhatsApp oficial**. Esse Android também roda o app **Ponte do Tijolo**, que faz duas coisas:

```
WhatsApp ── notificação ──▶ Ponte do Tijolo ── SMS "WA#7xk Maria / Chego às 8" ──▶ Tijolo
WhatsApp ◀── "Responder" ── Ponte do Tijolo ◀── SMS "WA#7xk Ok!" ──────────────── Tijolo
```

- Chegou mensagem no WhatsApp: a ponte lê a notificação e manda uma SMS para o Tijolo. No Tijolo ela aparece como **WhatsApp · Maria**.
- Você responde no Tijolo como responde um SMS. A ponte recebe a SMS e responde no WhatsApp pelo botão "Responder" da notificação, como um relógio inteligente faz.

A conta continua no app oficial: a ponte só lê as notificações e usa o botão de responder.

## Do que precisa

- Um Android **6.0 ou mais novo** (de preferência 8 ou mais novo) **que ainda rode o WhatsApp oficial**. Desde 8/9/2026 o WhatsApp não funciona no Android 5. Ele fica em casa, na tomada e no Wi-Fi.
  - **Não serve um celular "Android Go"** (os baratos de 1 GB, com "Go" na versão do Android) com Android 10 ou mais velho: neles o Android não deixa nenhum app ler as notificações.
- **Um chip só para esse Android**, com SMS. O seu número continua no chip do Tijolo. Um pré-pago com SMS ilimitado é o ideal: cada mensagem do WhatsApp vira uma SMS (veja *Custos*). Tire o PIN desse chip (veja *Deixar a ponte em casa*).
- **Um computador** (Windows, Mac ou Linux) e o cabo USB do Android, só para instalar o app.
- O arquivo `PonteDoTijolo.apk`.

## 1. Passar o WhatsApp para o Android da ponte

1. No celular onde o WhatsApp está hoje, faça o backup das conversas (Configurações → Conversas → Backup), se quiser levar o histórico.
2. No Android da ponte, instale o WhatsApp e entre com **o seu número** (o do Tijolo). O código de confirmação chega por SMS **no Tijolo**: abra em Mensagens. Se o WhatsApp oferecer ligar com o código, atenda no Tijolo.
3. A conta fica neste Android. O WhatsApp do celular antigo sai da conta.
4. **Deixe o WhatsApp mostrar notificações.** A ponte só vê o que vira notificação.
   - No Android 13 ou mais novo, o WhatsApp pergunta na primeira vez: toque em **Permitir**. Se negou, ligue em Configurações → Apps → WhatsApp → Notificações.
   - Ali mesmo, deixe ligadas as categorias de mensagens e de grupos.
   - No WhatsApp (Configurações → Notificações), deixe as notificações de conversas e de grupos ligadas.
5. **O que nunca chega ao Tijolo**, porque não vira notificação:
   - conversas e grupos **silenciados** no WhatsApp;
   - conversas **arquivadas** (o WhatsApp as deixa arquivadas e silenciadas quando chega mensagem nova);
   - conversas **trancadas** (Bloqueio de conversa) e, com o bloqueio do app ligado, as notificações sem conteúdo: chega só o "Nova mensagem" que o WhatsApp mostra.

## 2. Instalar o app (pelo computador)

**Por que pelo computador.** No Brasil, o Play Protect bloqueia a instalação de apps baixados que leem SMS ou notificações, como este, quando o arquivo é aberto pelo navegador, pelo Gmail, pelo Drive ou pelo gerenciador de arquivos. Aparece *"App bloqueado para proteger seu dispositivo"*, sem botão para instalar mesmo assim. Pelo cabo USB (o `adb`), a instalação não é bloqueada, e o Android 13 ou mais novo também não põe a "configuração restrita" no passo 3.

1. **Ligar as opções do desenvolvedor.** No Android, toque **7 vezes** em *Número da versão* até aparecer "Você agora é um desenvolvedor".
   - Na maioria: Configurações → Sobre o telefone → Número da versão.
   - Samsung: Configurações → Sobre o telefone → Informações do software → Número de compilação.
   - Xiaomi / Redmi / POCO: Configurações → Sobre o telefone → Versão do MIUI (ou do HyperOS).
2. **Ligar a depuração USB.** Configurações → Sistema → **Opções do desenvolvedor** → ligue **Depuração USB**. Na Samsung, as opções do desenvolvedor ficam no fim das Configurações; na Xiaomi, em Configurações adicionais. Na Xiaomi, ligue também **Instalar via USB** (ela pede a conta Mi e um chip no aparelho).
3. **Baixar o `adb` no computador**: o pacote [SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools), da Google, para o seu sistema. Descompacte. Ponha o `PonteDoTijolo.apk` dentro da pasta `platform-tools`.
4. **Abrir um terminal nessa pasta.**
   - Windows: abra a pasta `platform-tools`, clique na barra de endereço, digite `cmd` e Enter.
   - Mac ou Linux: abra o Terminal e digite `cd ` seguido da pasta (dá para arrastar a pasta para a janela). Nos comandos abaixo, escreva `./adb` no lugar de `adb`.
5. **Ligar o Android no computador** com o cabo e digitar:
   ```
   adb devices
   ```
   O Android pergunta *"Permitir depuração USB?"*: marque **Sempre permitir deste computador** e toque em **Permitir**. Rode `adb devices` de novo: o aparelho aparece com a palavra `device`.
   - Se aparecer `unauthorized`, a pergunta não foi aceita: destrave a tela e olhe de novo.
   - Se não aparecer nada, troque o cabo (alguns só carregam). No Windows, a Samsung e outras marcas precisam do driver USB do fabricante.
6. **Instalar:**
   ```
   adb install PonteDoTijolo.apk
   ```
   Termina com `Success`.
7. **Só no Android 15 ou mais novo.** O Android esconde dos apps as notificações que parecem ter um código de confirmação (por exemplo, "o código é 482913"). Sem este comando, essas mensagens chegam ao Tijolo como um aviso do Android no lugar do texto:
   ```
   adb shell appops set br.tijolo.ponte RECEIVE_SENSITIVE_NOTIFICATIONS allow
   ```
   Depois, reinicie o Android. Outra saída é desligar as *Notificações aprimoradas* do Android (Configurações → Notificações), mas aí somem também as respostas sugeridas.
   Sem nenhum dos dois, a mensagem escondida chega ao Tijolo uma vez, como *"Conteúdo de notificação sensível oculto"*, e as seguintes da mesma conversa podem continuar escondidas até a notificação dela ser limpa no Android.
8. **Desligue a depuração USB** quando terminar (Opções do desenvolvedor). Ligada, qualquer computador que você já autorizou mexe no Android pelo cabo.

**Atualizar o app**: os mesmos passos, com `adb install -r PonteDoTijolo.apk`. O `-r` instala por cima e o app guarda o número do Tijolo e as conversas. Isso só funciona se a versão nova foi assinada com a **mesma chave** (veja *Montar o app*).

### Sem computador (não recomendado)

Dá para desligar a verificação do Play Protect: Play Store → toque na sua foto → **Play Protect** → ⚙ (canto de cima) → desligue **Verificar apps com o Play Protect**. Depois, abra o `PonteDoTijolo.apk` no Android e permita que o app que abriu o arquivo (o Arquivos, o Drive...) instale apps. No Android 6 e 7, essa permissão é uma chave só: Configurações → Segurança → **Fontes desconhecidas**.

- Instalado assim, no Android 13 ou mais novo aparece a *configuração restrita* no passo 3 de *Configurar* (o que fazer está lá).
- Se você religar o Play Protect, uma verificação futura pode achar suspeito um app que lê notificações e manda SMS, e desativá-lo. A ponte então para sem avisar. Pelo computador esse risco é menor, mas não some: se a ponte parar de repente, abra o app e veja se ele ainda está lá.

## 3. Configurar (a tela do app)

Abra o **Ponte do Tijolo** e siga a tela de cima para baixo. Cada linha com ✗ diz o que falta.

1. **O número do Tijolo**, com DDD (o do chip que está no Tijolo, **não** o deste Android) → *Guardar o número*. Pode escrever com ou sem o +55.
2. **Permitir SMS** → Permitir.
3. **Acesso às notificações** → ligue o *Ponte do Tijolo* → Permitir.
   - Só se você instalou sem computador, no Android 13 ou mais novo: se aparecer *Configuração restrita*, volte ao app e toque em **Informações do app**. No menu ⋮ (canto de cima), escolha **Permitir configurações restritas** e repita este passo. Se o SMS também for bloqueado assim, vale o mesmo.
4. **Não economizar bateria** → Permitir. Sem isso, o Android fecha a ponte de madrugada. O **WhatsApp também** precisa ficar sem economia de bateria, senão é ele quem para de mostrar notificações:
   - Android 12 ou mais novo: Configurações → Apps → WhatsApp → Bateria → **Sem restrições**.
   - Android 6 a 11: Configurações → Apps → Acesso especial → Otimização de bateria → Todos os apps → WhatsApp → **Não otimizar**.
   - **Xiaomi / Redmi / POCO**: para a ponte e para o WhatsApp, em Informações do app, ligue *Início automático* e ponha *Economia de bateria* em *Sem restrições*.
   - **Samsung** (Android 11 ou mais novo): Configurações → Bateria (ou Cuidados com o dispositivo → Bateria) → Limites de uso em segundo plano → *Apps que nunca entram em suspensão* → adicione a Ponte do Tijolo e o WhatsApp.
   - **Samsung mais antigo** (Android 7 a 10): Configurações → Manutenção do aparelho → Bateria → *Apps não monitorados* → adicione os dois.
   - Huawei, Oppo, Vivo, Realme e outras marcas: veja o passo a passo do seu modelo em [dontkillmyapp.com](https://dontkillmyapp.com).
   - Android 11 ou mais novo: se o app mostrar a linha sobre *remover permissões*, desligue em Informações do app *"Remover permissões se o app não for usado"* (ou *"Pausar atividade no app se não for usado"*).
5. Com tudo ✓, toque em **Mandar uma SMS de teste ao Tijolo**. No Tijolo chega uma mensagem da **Ponte do WhatsApp**.

As SMS que o Tijolo manda para a ponte também aparecem no app de mensagens do Android. Se incomodar, silencie as notificações desse app.

**Android com dois chips**: em Configurações → Chips (ou Gerenciador de SIM), escolha o chip da ponte para SMS. Não deixe em *Perguntar sempre*: a ponte manda SMS sem ninguém na tela, e aí o Android pode não mandar.

**Se aparecer uma janela perguntando se o app pode continuar mandando muitas SMS**, marque *Lembrar* (ou *Sempre permitir*) e toque em **Permitir**. Enquanto ninguém responde, as SMS ficam paradas. A ponte manda no máximo 20 partes por minuto para não chegar nesse limite (o do Android é 30), mas alguns aparelhos têm um limite menor.

## 4. No Tijolo

No Monitor Serial, diga ao Tijolo qual é o número do chip da ponte, com DDD:

```
ponte 11977776666
```

Ele fica guardado. Para testar o caminho de ida e volta, use `ponte lista`: a ponte responde com as conversas que já conhece. Se preferir, ponha o número em `PONTE_WHATSAPP` no `config.h`.

## 5. Deixar a ponte em casa

- **Na tomada, no Wi-Fi e com sinal**: o Wi-Fi traz o WhatsApp, o sinal leva as SMS.
- **Não deixe o WhatsApp aberto na tela do Android.** Com uma conversa aberta, o WhatsApp não mostra notificação, e nada vai para o Tijolo. Deixe na tela inicial, com a tela apagada. Abrir uma conversa também marca como lida e apaga a notificação; o que já foi para o Tijolo não muda.
- **Tire o PIN do chip** (Configurações → Segurança → Bloqueio do chip). Com PIN, depois de reiniciar o chip fica sem rede até alguém digitar o PIN, e nenhuma SMS sai.
- **Depois de reiniciar** (falta de luz longa, atualização do sistema), no Android 7 ou mais novo com bloqueio de tela, o WhatsApp e a ponte **só voltam a funcionar quando alguém destrava o Android uma vez**. Quando voltam, o Tijolo recebe *"A ponte ligou de novo"*. É uma escolha:
  - **com bloqueio de tela**, quem pegar o Android não entra no seu WhatsApp, mas alguém em casa precisa destravá-lo depois de cada reinício;
  - **sem bloqueio**, a ponte volta sozinha, mas quem tiver o Android na mão tem o seu WhatsApp inteiro.

  Na tomada e com bateria, o Android aguenta faltas de luz curtas sem reiniciar.
- **A vigia**: a cada 30 minutos a ponte confere se ainda está ouvindo o WhatsApp. Se parou, ela tenta religar. Se continuar sem ouvir na vigia seguinte, o Tijolo recebe *"A ponte parou de ouvir o WhatsApp"* e, quando ela voltar, *"A ponte voltou a ouvir o WhatsApp"*. Se o aviso chegar, abra o app no Android; se ele não voltar a ouvir, desligue e ligue de novo o acesso às notificações.
- **A fila**: uma SMS que não sai (sem sinal, sem crédito) fica na fila do app, e a fila inteira espera 1, 2, 5, 10 e 30 minutos e depois tenta de hora em hora, sempre pela mensagem mais antiga. Quando uma SMS sai, a fila anda na hora. Depois de 2 dias sem conseguir, as mensagens mais velhas que isso saem da fila e o Tijolo recebe um aviso dizendo quantas se perderam. O app mostra quantas estão esperando; *Tentar mandar a fila agora* esquece a espera e tenta na hora (use quando o sinal ou o crédito voltar).
- **Pausar e silenciar limpam a fila**: *Pausar o WhatsApp* tira da fila as mensagens que ainda não saíram, e *Silenciar a conversa* tira as daquela conversa. O aviso da ponte passa na frente delas.
- **Segurança**:
  - quem tem o Android da ponte na mão tem o seu WhatsApp. Deixe-o num lugar seguro, como deixaria o seu celular;
  - a ponte só obedece a SMS do número do Tijolo, mas o número de quem manda uma SMS pode ser falsificado. **Não divulgue o número do chip da ponte**: quem souber dele e conseguir falsificar o remetente poderia mandar respostas pelo seu WhatsApp.

## Usando

- **Chegou**: no Tijolo aparece **WhatsApp** em verde e o nome da pessoa ou do grupo. Num grupo, cada linha começa com quem escreveu ("João: bom dia").
- **Responder**: igual a um SMS. Lendo a mensagem, aperte o verde (Responder; sem os botões, segure o 1), escreva e aperte o verde duas vezes. A tela "Enviar para" mostra o nome e "pelo WhatsApp". Depois aparece **"Enviada à ponte"**: a SMS chegou à ponte, e é ela quem põe a resposta no WhatsApp. Se não der, o Tijolo recebe um aviso da ponte dizendo por quê.
- **Opções** (o 5, lendo uma mensagem do WhatsApp): *Silenciar a conversa* (para um grupo animado demais) e *Reativar a conversa*. Lendo um aviso da própria ponte: *Pausar o WhatsApp* (nada mais chega ao Tijolo) e *Retomar o WhatsApp*.
- **Grupos começam desligados**: ligue em *Repassar também os grupos*, no app.
- Mensagens seguidas da mesma conversa vão juntas, numa SMS só. A ponte espera 8 s sem mensagem nova (num grupo, 20 s) e no máximo 30 s (num grupo, 1 minuto).
- **Fotos, áudios, figurinhas**: chegam como `[foto]`, `[audio]`... com a legenda, se houver. Para ver a foto, só no Android.
- **Emoji**: os comuns viram sinais (`:)` `:D` `<3` `(ok)`). Os outros somem; se a mensagem era só emoji, chega `[emoji]`.
- **Chamada do WhatsApp**: não dá para atender, mas a chamada perdida chega como mensagem.

**Atualize o Tijolo e a ponte juntos.** O firmware do Tijolo de antes do código das conversas mostra as SMS da ponte nova como SMS comuns ("WA#7xk Maria..."). E, no Tijolo, as mensagens do WhatsApp que chegaram antes da atualização não servem mais para responder: a ponte avisa e é só responder a uma mensagem mais nova.

### O número e o código de cada conversa

Cada conversa ganha um número e duas letras, como `#7xk`. O número é curto para caber na SMS; as letras saem do nome da conversa no WhatsApp e conferem a resposta. Se a ponte esquecer as conversas (por exemplo, depois de reinstalar o app) e o número 7 passar a ser de outra pessoa, uma resposta a uma mensagem antiga de `#7xk` **não vai para a pessoa errada**: a ponte recusa e o Tijolo recebe *"a conversa 7 agora é outra"*. É só responder numa mensagem mais nova daquela conversa.

### O que o Tijolo manda para a ponte

As opções acima mandam estas SMS ao número da ponte. A ponte ignora SMS de qualquer outro número.

| SMS | O que faz |
|---|---|
| `WA#7xk texto` | responde na conversa 7 (o número e o código vêm na mensagem que chegou) |
| `WA?` | a ponte responde com as conversas recentes, o número e o código de cada uma |
| `WA-` / `WA+` | pausa / retoma tudo |
| `WA-7xk` / `WA+7xk` | silencia / reativa só a conversa 7 |

## Limitações (é bom saber)

- **Só dá para responder, não para começar conversa.** A ponte precisa do botão "Responder" de uma notificação daquela conversa. Ela guarda o botão mesmo depois que a notificação some, mas o perde se o Android reiniciar ou fechar o app. Aí a resposta volta a funcionar quando a conversa mandar outra mensagem; até lá, o Tijolo recebe o aviso *"Não consigo responder a ... agora"*.
- **Mensagens antigas não vão.** Quando a ponte começa a ouvir (na primeira vez, ou depois de o Android reiniciar), das conversas que ela ainda não conhecia só vão as mensagens dos últimos 30 minutos. E a notificação do WhatsApp mostra no máximo umas 25 mensagens por conversa: as mais antigas que isso ficam só no Android.
- **Privacidade**: do WhatsApp até o Android, a criptografia de ponta a ponta continua. **Entre o Android e o Tijolo, as mensagens vão por SMS comum**, que a operadora consegue ler.
- **Custos**: cada SMS longa vai em partes. Sem acento cabem 153 letras por parte; com qualquer acento, 67. A ponte corta cada mensagem em 330 letras (até 6 partes). Num plano sem SMS ilimitado, um grupo movimentado custa caro: silencie pelo Tijolo.
- **A ponte depende do Android**: ligado, na tomada, com Wi-Fi para o WhatsApp e sinal para as SMS.

## Problemas

- **Nada chega no Tijolo**: abra o app no Android. Está tudo ✓? Em *Últimos acontecimentos*:
  - `→ Tijolo: ...` é uma SMS que saiu.
  - `FALHOU a SMS (motivo)` é crédito, sinal ou modo avião no Android; ela fica na fila e é tentada de novo.
  - Se não aparece nada, confira se o WhatsApp está mostrando a notificação no Android (passos 4 e 5 da parte 1, e *Deixar a ponte em casa*). Sem notificação, a ponte não vê.
- **"Ainda não está ouvindo"**: desligue e ligue de novo o acesso às notificações (passo 3). Se continuar, reinicie o Android.
- **A resposta não chega no WhatsApp**: o Tijolo recebe um aviso da ponte com o motivo. O mais comum é *"Não consigo responder a ... agora"*: veja *Limitações*.
- **Para de funcionar depois de um tempo**: é a economia de bateria do Android, na ponte ou no WhatsApp (passo 4).
- **Para de funcionar depois de reiniciar**: falta destravar o Android uma vez (veja *Deixar a ponte em casa*).
- **O Tijolo mostra a SMS crua ("WA#7xk ...") como SMS comum**: o Tijolo não sabe o número da ponte, ou sabe um número errado (`ponte` no Monitor Serial mostra qual é).
- **Uma conversa chega com o nome errado, ou a mesma mensagem chega duas vezes**: toque em *Diagnóstico: anotar as próximas 10 notificações do WhatsApp* e deixe chegar uma mensagem dessa conversa. As linhas `diag:` em *Últimos acontecimentos* mostram como o WhatsApp deste Android identifica as conversas; mande essas linhas junto com a descrição do problema.

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
  - `Ponte` e `Saida`: a fila das SMS para o Tijolo (o limite por minuto e as novas tentativas).
  - `Despertador` e `AoLigar`: a vigia de 30 em 30 minutos e o aviso quando o Android reinicia.
  - `Principal`: a tela.
  - `Protocolo`, `Texto`, `Conversas`, `Novidades` e `Saida`: a lógica em Java puro. É ela que decide o formato das SMS, como limpar o texto, o número e o código de cada conversa, o que é mensagem nova e quando tentar de novo.
- `teste/`: a lógica testada no computador. `TestesAndroid` roda o caminho inteiro com um Android de mentira (`teste/falso/`): de notificação a SMS, de SMS à resposta, a fila, a vigia e o reinício.
- **A chave que assina o app** fica em `chave/ponte.jks`, e a senha dela em `chave/senha.txt`. As duas são criadas na primeira montagem (a senha é sorteada) e ficam **fora do git**. Quem tiver a chave e a senha pode fazer uma "atualização" que lê o seu WhatsApp. Guarde uma cópia das duas num lugar seguro. Para instalar uma versão nova por cima da antiga, é preciso a **mesma** chave. Com outra chave, o Android só aceita depois de desinstalar a antiga, e aí o app esquece o número do Tijolo e as conversas.
  - Também dá para passar a senha em `PONTE_SENHA`.
