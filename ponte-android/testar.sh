#!/bin/sh
# Testa a lógica da ponte no computador, sem Android: o formato das SMS,
# a limpeza do texto, os números das conversas e o que é mensagem nova.
#   sh testar.sh
set -eu
cd "$(dirname "$0")"
saida=saida/testes
rm -rf "$saida"
mkdir -p "$saida"
p=src/br/tijolo/ponte
javac -encoding UTF-8 -d "$saida" \
  $p/Protocolo.java $p/Texto.java $p/Conversas.java $p/Novidades.java \
  teste/br/tijolo/ponte/Testes.java
java -Dfile.encoding=UTF-8 -Dstdout.encoding=UTF-8 -cp "$saida" br.tijolo.ponte.Testes

# O caminho inteiro (notificação -> SMS -> resposta) com um Android de mentira
falso=saida/testes-android
rm -rf "$falso"
mkdir -p "$falso"
javac -encoding UTF-8 -d "$falso" $(find teste/falso -name '*.java') \
  $(ls $p/*.java | grep -v Principal.java) teste/br/tijolo/ponte/TestesAndroid.java
java -Dfile.encoding=UTF-8 -Dstdout.encoding=UTF-8 -cp "$falso" br.tijolo.ponte.TestesAndroid
