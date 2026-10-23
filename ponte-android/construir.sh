#!/bin/sh
# Monta o PonteDoTijolo.apk sem Gradle nem Android Studio, só com as
# ferramentas de linha de comando do Android. No Ubuntu/Debian:
#
#   sudo apt install openjdk-17-jdk-headless android-sdk android-sdk-platform-23 \
#                    apksigner zipalign aapt dalvik-exchange
#   sh construir.sh
#
# Sai em saida/PonteDoTijolo.apk. A chave que assina o app fica em
# chave/ponte.jks (fora do git): guarde uma cópia. As atualizações precisam
# da MESMA chave, senão o Android só instala depois de desinstalar a versão
# velha (e aí o app esquece o número do Tijolo e as conversas).
set -eu
cd "$(dirname "$0")"

SDK=${ANDROID_HOME:-/usr/lib/android-sdk}
AJ=${ANDROID_JAR:-$SDK/platforms/android-23/android.jar}
BT=${BUILD_TOOLS:-$SDK/build-tools/debian}
CHAVE=${PONTE_CHAVE:-chave/ponte.jks}
SENHA=${PONTE_SENHA:-ponte-do-tijolo}

ferramenta() {   # o caminho de uma ferramenta: no build-tools ou no PATH
  if [ -x "$BT/$1" ]; then echo "$BT/$1"; elif command -v "$1" >/dev/null 2>&1; then command -v "$1"
  else echo "Falta a ferramenta '$1' (veja o começo deste arquivo)" >&2; exit 1; fi
}
AAPT=$(ferramenta aapt); DX=$(ferramenta dx); ZIPALIGN=$(ferramenta zipalign); APKSIGNER=$(ferramenta apksigner)
[ -f "$AJ" ] || { echo "Falta $AJ (android-sdk-platform-23)" >&2; exit 1; }

echo "1/6 testes (a lógica e o caminho inteiro, com um Android de mentira)"
mkdir -p saida
if ! sh testar.sh > saida/testes.txt 2>&1; then cat saida/testes.txt; echo "Os testes falharam: não montei o app." >&2; exit 1; fi
grep "verificações" saida/testes.txt | sed 's/^/    /'

echo "2/6 recursos (R.java)"
rm -rf saida/gen saida/obj saida/apk
mkdir -p saida/gen saida/obj saida/apk
"$AAPT" package -f -m -J saida/gen -M AndroidManifest.xml -S res -I "$AJ"

echo "3/6 compilando o Java"
# Java 8, sem lambdas: o dx não sabe convertê-las
javac -encoding UTF-8 -source 8 -target 8 -bootclasspath "$AJ" -classpath "$AJ" -Xlint:-options \
  -d saida/obj $(find src saida/gen -name '*.java')

echo "4/6 convertendo para o Android (classes.dex)"
"$DX" --dex --min-sdk-version=21 --output=saida/apk/classes.dex saida/obj

echo "5/6 empacotando"
"$AAPT" package -f -M AndroidManifest.xml -S res -I "$AJ" -F saida/sem-assinatura.apk
(cd saida/apk && "$AAPT" add -f ../sem-assinatura.apk classes.dex >/dev/null)
"$ZIPALIGN" -f 4 saida/sem-assinatura.apk saida/alinhado.apk

echo "6/6 assinando"
if [ ! -f "$CHAVE" ]; then
  echo "    (primeira vez: criando a chave $CHAVE — guarde uma cópia!)"
  mkdir -p "$(dirname "$CHAVE")"
  keytool -genkeypair -keystore "$CHAVE" -storepass "$SENHA" -keypass "$SENHA" -alias ponte \
    -keyalg RSA -keysize 2048 -validity 20000 -dname "CN=Ponte do Tijolo" >/dev/null 2>&1
fi
"$APKSIGNER" sign --ks "$CHAVE" --ks-pass "pass:$SENHA" --ks-key-alias ponte \
  --v4-signing-enabled false --out saida/PonteDoTijolo.apk saida/alinhado.apk
"$APKSIGNER" verify saida/PonteDoTijolo.apk
rm -f saida/sem-assinatura.apk saida/alinhado.apk
echo "Pronto: saida/PonteDoTijolo.apk ($(wc -c < saida/PonteDoTijolo.apk) bytes)"
