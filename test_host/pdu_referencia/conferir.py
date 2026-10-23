import sys
from gsmmodem.pdu import decodeSmsPdu
grupos = {}
falhas = 0
n = 0
for linha in open(sys.argv[1]):
    hexpdu, tam, txt, num, total = linha.rstrip("\n").split("\t")
    esperado = bytes.fromhex(txt).decode("utf-8")
    d = decodeSmsPdu(hexpdu)
    n += 1
    if d["tpdu_length"] != int(tam):
        print("tamanho", d["tpdu_length"], tam); falhas += 1
    if d["type"] != "SMS-SUBMIT" or d["number"] != num:
        print("tipo/numero", d["type"], d["number"], num); falhas += 1
    ref = None
    for u in d.get("udh", []) or []:
        if hasattr(u, "reference"):
            ref = u.reference
            if u.parts != int(total): print("partes", u.parts, total); falhas += 1
    if int(total) > 1 and ref is None:
        print("faltou cabeçalho de SMS longo"); falhas += 1
    chave = (num, esperado)
    grupos.setdefault(chave, []).append(d["text"])
for (num, esperado), partes in grupos.items():
    if "".join(partes) != esperado:
        print("TEXTO DIFERENTE:", repr("".join(partes))[:80], "!=", repr(esperado)[:80]); falhas += 1
print("PDUs conferidos:", n, "falhas:", falhas)
sys.exit(1 if falhas else 0)
