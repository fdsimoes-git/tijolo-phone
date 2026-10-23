# Gera vetores de teste com a biblioteca independente python-gsmmodem-new
import sys
from gsmmodem.pdu import encodeSmsSubmitPdu, packSeptets, encodeGsm7

TEXTOS = [
    "hellohello",
    "Oi, tudo bem? Chego as 8h.",
    "Já cheguei, até já! Pão de açúcar",
    "Acento: ação, coração",
    "Colchetes [ok] e chaves {x} e euro €100 ~ ^ | \\ fim",
    "a" * 160,
    "b" * 161,
    "Longo com acento ç " * 12,
    "x" * 459,
    "€" * 80,              # 160 septetos (extensão = 2)
    "€" * 81,              # 162: vira SMS longo
    "",
]
NUMEROS = ["+5511999998888", "11999998888", "27888"]

def utf8hex(s): return s.encode("utf-8").hex().upper()

def submit_para_deliver(hexpdu, oa_bytes=None):
    b = bytes.fromhex(hexpdu)
    i = 0
    sca = b[i]; i += 1 + sca
    fo = b[i]; i += 1
    mr = b[i]; i += 1
    ndig = b[i]; da = b[i:i + 2 + (ndig + 1) // 2]; i += 2 + (ndig + 1) // 2
    pid = b[i]; dcs = b[i + 1]; i += 2
    vpf = (fo >> 3) & 3
    i += 0 if vpf == 0 else (1 if vpf == 2 else 7)
    udl_ud = b[i:]
    udhi = fo & 0x40
    out = bytes([0x00, 0x04 | udhi]) + (oa_bytes if oa_bytes else da) + bytes([pid, dcs])
    out += bytes.fromhex("62012181505040")    # 26/10/12,18:05:05 -  fuso "40" = +04 quartos (+1 h)? (só formato)
    return (out + udl_ud).hex().upper()

with open(sys.argv[1], "w") as f:
    ref = 7
    for n in NUMEROS:
        for t in TEXTOS:
            pdus = encodeSmsSubmitPdu(n, t, reference=ref, requestStatusReport=False)
            total = len(pdus)
            for k, x in enumerate(pdus):
                h = str(x)
                # texto esperado da parte: a biblioteca divide do jeito dela; guardamos o texto inteiro
                f.write("SUBMIT\t%s\t%s\t%s\t%d\t%d\t%d\n" % (h, utf8hex(t), n, ref if total > 1 else 0, total, k + 1))
                f.write("DELIVER\t%s\t%s\t%s\t%d\t%d\t%d\n" % (submit_para_deliver(h), utf8hex(t), n, ref if total > 1 else 0, total, k + 1))
    # remetente alfanumérico "TIM": 3 caracteres = 21 bits = 3 bytes = 6 semi-octetos
    alfa = packSeptets(encodeGsm7("TIM"))
    oa = bytes([len(alfa) * 2, 0xD0]) + bytes(alfa)
    pdus = encodeSmsSubmitPdu("+5511999998888", "Sua recarga caiu.", reference=1, requestStatusReport=False)
    f.write("DELIVER\t%s\t%s\t%s\t%d\t%d\t%d\n" % (submit_para_deliver(str(pdus[0]), oa), utf8hex("Sua recarga caiu."), "TIM", 0, 1, 1))
print("ok")
