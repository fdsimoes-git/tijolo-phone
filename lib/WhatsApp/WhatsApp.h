// ============================================================
//  WhatsApp — o lado do Tijolo da "Ponte do WhatsApp"
//
//  Um Android em casa (o app de ponte-android/) repassa as mensagens do
//  WhatsApp por SMS e devolve as respostas. Este módulo conhece o formato
//  dessas SMS (o mesmo de ponte-android/src/br/tijolo/ponte/Protocolo.java):
//
//  Ponte → Tijolo:
//    "WA#7xk Maria\nChego às 8"    mensagem da conversa 7 (num grupo, uma
//                                  linha "Autor: texto" por mensagem)
//    "WA? #3ab Maria\n#7xk Família"  as conversas recentes (resposta ao "WA?")
//    "WA! texto"                   aviso da ponte
//  Tijolo → Ponte:
//    "WA#7xk texto"                responder na conversa 7
//    "WA?"  "WA-"  "WA+"           a lista; pausar e retomar tudo
//    "WA-7xk"  "WA+7xk"            silenciar e reativar a conversa 7
//
//  As duas letras depois do número são o código da conversa: se o número
//  passar a ser de outra conversa na ponte, o código não bate e a ponte
//  recusa a resposta (em vez de mandar para a pessoa errada).
//
//  Na caixa do Tijolo, a mensagem do WhatsApp fica com o "número"
//  "WA#7xk Maria" (a conversa, o código e o nome); a própria ponte é "WA".
//  Responder a "WA#7xk Maria" manda "WA#7xk texto" para o número da ponte
//  (quem troca o endereço pelo número é o main.cpp).
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

bool ehEndereco(const std::string& numero);    // "WA" ou "WA#7xk Maria"
bool ehConversa(const std::string& numero);    // "WA#7xk Maria" (ou "WA#7xk", "WA#7")
int conversa(const std::string& numero);       // 7 (0 se não é conversa)
std::string codigo(const std::string& numero); // "7xk" (o número e o código; "" se não é conversa)
// "Maria"; "WhatsApp" para a própria ponte; "Conversa 7" sem nome
std::string nome(const std::string& numero);
std::string endereco(const std::string& codigo, const std::string& nome);   // ("7xk", "Maria") → "WA#7xk Maria"

// O texto da SMS que vai para a ponte: "WA#7xk " + texto (conversa), o
// texto como está (a própria ponte: comandos) ou, para um número comum,
// também como está.
std::string textoParaPonte(const std::string& numero, const std::string& texto);

// SMS que chegou da ponte → o endereço e o texto para a caixa.
// false: não está no formato da ponte (fica como SMS comum)
bool daPonte(const std::string& sms, std::string& numero, std::string& texto);

std::string silenciar(const std::string& numero);   // "WA#7xk Maria" → "WA-7xk"
std::string reativar(const std::string& numero);    // "WA#7xk Maria" → "WA+7xk"

// O mesmo telefone? "+5511977776666", "011977776666" e "11977776666" são o
// mesmo: compara os últimos 11 dígitos (o DDD e o número; 10 se um deles
// for mais curto). Sem o DDD não vale: outro DDD é outro telefone.
bool mesmoNumero(const std::string& a, const std::string& b);

// O número da ponte como digitado no comando "ponte", arrumado para guardar:
// "11 97777-6666" e "(011) 97777-6666" → "+5511977776666"; "+55 11 9..." fica;
// "+11 97777-6666" (o + sobrando) também vira "+5511977776666". Devolve ""
// e o motivo em `erro` se não servir (curto, letras, celular sem o 9).
std::string numeroDaPonte(const std::string& digitado, std::string& erro);

}  // namespace whatsapp
