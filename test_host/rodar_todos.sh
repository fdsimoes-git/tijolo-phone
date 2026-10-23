#!/bin/sh
# ============================================================
#  Roda todos os testes do computador (sem a placa)
#
#  Da raiz do projeto:   sh test_host/rodar_todos.sh
#  Precisa só de um compilador C++ (g++). As telas da interface ficam em
#  test_host/saida/telas/ (imagens .bmp).
# ============================================================
cd "$(dirname "$0")/.." || exit 1
SAIDA="${TMPDIR:-/tmp}/tijolo_testes"
mkdir -p "$SAIDA"
CXX="${CXX:-g++} -std=c++17 -Wall -Wextra"
falhas=0

testar() {   # nome, e o resto é a linha do compilador
  nome="$1"
  shift
  printf '%-12s ' "$nome"
  if ! $CXX "$@" -o "$SAIDA/$nome" 2> "$SAIDA/$nome.erros"; then
    echo "NÃO COMPILOU (veja $SAIDA/$nome.erros)"
    falhas=$((falhas + 1))
    return
  fi
  if "$SAIDA/$nome" > "$SAIDA/$nome.saida" 2>&1; then
    tail -n 1 "$SAIDA/$nome.saida"
  else
    echo "FALHOU:"
    grep -i "falh" "$SAIDA/$nome.saida" | head -20
    falhas=$((falhas + 1))
  fi
}

testar texto -I lib/Texto -I lib/Toque test_host/texto_test.cpp
testar pdu -I lib/Texto -I lib/Pdu test_host/pdu_test.cpp
testar modem -I test_host/shim -I lib/Texto -I lib/Pdu -I lib/Modem test_host/modem_sim_test.cpp lib/Modem/Modem.cpp
testar teclado -I lib/Teclado test_host/teclado_test.cpp lib/Teclado/Teclado.cpp
testar caixa -I lib/Caixa test_host/caixa_test.cpp lib/Caixa/Caixa.cpp
testar multitoque -I lib/Texto -I lib/Escrita test_host/multitoque_test.cpp lib/Escrita/Multitoque.cpp
testar whatsapp -I lib/WhatsApp test_host/whatsapp_test.cpp lib/WhatsApp/WhatsApp.cpp
testar interface -I lib/Texto -I lib/Tela -I lib/Teclado -I lib/Interface -I lib/Caixa -I lib/Escrita -I lib/Pdu \
  -I lib/WhatsApp test_host/interface_test.cpp lib/Tela/Quadro.cpp lib/Tela/Fontes.cpp lib/Teclado/Teclado.cpp \
  lib/Caixa/Caixa.cpp lib/Escrita/Multitoque.cpp lib/WhatsApp/WhatsApp.cpp lib/Interface/Interface.cpp
testar diagnostico -I lib/Tela -I lib/Medidor -I lib/Memoria test_host/diagnostico_test.cpp
testar vigia -I lib/Tela test_host/vigia_test.cpp

if [ "$falhas" -eq 0 ]; then echo "Tudo certo."; else echo "$falhas conjunto(s) com problema."; fi
exit "$falhas"
