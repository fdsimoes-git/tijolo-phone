// ============================================================
//  WhatsApp — o lado do Tijolo da "Ponte do WhatsApp"
//
//  Um Android em casa (o app de ponte-android/) repassa as mensagens do
//  WhatsApp por SMS e devolve as respostas. Este módulo conhece o formato
//  dessas SMS (o mesmo de ponte-android/src/br/tijolo/ponte/Protocolo.java):
//
//  Ponte → Tijolo:
//    "WA#7 Maria\nChego às 8"      mensagem da conversa 7 (num grupo, uma
//                                  linha "Autor: texto" por mensagem)
//    "WA? #3 Maria\n#7 Família"    as conversas recentes (resposta ao "WA?")
//    "WA! texto"                   aviso da ponte
//  Tijolo → Ponte:
//    "WA#7 texto"                  responder na conversa 7
//    "WA?"  "WA-"  "WA+"           a lista; pausar e retomar tudo
//    "WA-7"  "WA+7"                silenciar e reativar a conversa 7
//
//  Na caixa do Tijolo, a mensagem do WhatsApp fica com o "número"
//  "WA#7 Maria" (o número da conversa na ponte e o nome); a própria ponte
//  é "WA". Responder a "WA#7 Maria" manda "WA#7 texto" para o número da
//  ponte (quem troca o endereço pelo número é o main.cpp).
//
//  Não depende do Arduino: os testes rodam no computador.
// ============================================================
#pragma once
#include <string>

namespace whatsapp {

extern const char* const PONTE;     // "WA": a própria ponte
extern const char* const PAUSAR;    // "WA-"
extern const char* const RETOMAR;   // "WA+"
extern const char* const LISTA;     // "WA?"

bool ehEndereco(const std::string& numero);    // "WA" ou "WA#7 Maria"
bool ehConversa(const std::string& numero);    // "WA#7 Maria" (ou "WA#7")
int conversa(const std::string& numero);       // 7 (0 se não é conversa)
// "Maria"; "WhatsApp" para a própria ponte; "Conversa 7" sem nome
std::string nome(const std::string& numero);
std::string endereco(int conversa, const std::string& nome);   // "WA#7 Maria"

// O texto da SMS que vai para a ponte: "WA#7 " + texto (conversa), o texto
// como está (a própria ponte: comandos) ou, para um número comum, também
// como está.
std::string textoParaPonte(const std::string& numero, const std::string& texto);

// SMS que chegou da ponte → o endereço e o texto para a caixa.
// false: não está no formato da ponte (fica como SMS comum)
bool daPonte(const std::string& sms, std::string& numero, std::string& texto);

std::string silenciar(int conversa);   // "WA-7"
std::string reativar(int conversa);    // "WA+7"

// O mesmo telefone? "+5511977776666", "11977776666" e "977776666" são o
// mesmo: compara os últimos 9 dígitos (8, se um deles for mais curto).
bool mesmoNumero(const std::string& a, const std::string& b);

}  // namespace whatsapp
