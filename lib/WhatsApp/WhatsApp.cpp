#include "WhatsApp.h"

namespace whatsapp {

const char* const PONTE = "WA";
const char* const PAUSAR = "WA-";
const char* const RETOMAR = "WA+";
const char* const LISTA = "WA?";

namespace {

bool digito(char c) { return c >= '0' && c <= '9'; }
bool letra(char c) { return c >= 'a' && c <= 'z'; }

// "WA#7xk..." → n = 7, cod = "7xk" e fim = a posição depois do código
bool lerConversa(const std::string& s, int& n, std::string& cod, size_t& fim) {
  if (s.size() < 4 || s.compare(0, 3, "WA#") != 0) return false;
  size_t i = 3;
  n = 0;
  while (i < s.size() && digito(s[i])) {
    if (i >= 6) return false;                     // mais de 3 dígitos
    n = n * 10 + (s[i++] - '0');
  }
  if (i == 3 || n < 1) return false;
  // o código: duas letras minúsculas (opcional)
  if (i + 1 < s.size() && letra(s[i]) && letra(s[i + 1])) i += 2;
  if (i < s.size() && s[i] != ' ' && s[i] != '\n') return false;
  cod = s.substr(3, i - 3);
  fim = i;
  return true;
}

std::string aparar(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\n' || s[a] == '\r' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\n' || s[b - 1] == '\r' || s[b - 1] == '\t')) b--;
  return s.substr(a, b - a);
}

std::string digitos(const std::string& s) {
  std::string r;
  for (char c : s)
    if (digito(c)) r += c;
  return r;
}

}  // namespace

bool ehConversa(const std::string& numero) {
  int n;
  size_t fim;
  std::string c;
  return lerConversa(numero, n, c, fim);
}

bool ehEndereco(const std::string& numero) { return numero == PONTE || ehConversa(numero); }

int conversa(const std::string& numero) {
  int n;
  size_t fim;
  std::string c;
  return lerConversa(numero, n, c, fim) ? n : 0;
}

std::string codigo(const std::string& numero) {
  int n;
  size_t fim;
  std::string c;
  return lerConversa(numero, n, c, fim) ? c : "";
}

std::string nome(const std::string& numero) {
  if (numero == PONTE) return "WhatsApp";
  int n;
  size_t fim;
  std::string c;
  if (!lerConversa(numero, n, c, fim)) return numero;
  const std::string t = aparar(numero.substr(fim));
  return t.empty() ? "Conversa " + std::to_string(n) : t;
}

std::string endereco(const std::string& codigo, const std::string& nome) {
  std::string t;
  for (char c : aparar(nome)) t += (c == '\n' || c == '\r') ? ' ' : c;
  return "WA#" + codigo + (t.empty() ? "" : " " + t);
}

std::string textoParaPonte(const std::string& numero, const std::string& texto) {
  const std::string c = codigo(numero);
  return c.empty() ? texto : "WA#" + c + " " + texto;
}

bool daPonte(const std::string& sms, std::string& numero, std::string& texto) {
  int n;
  size_t fim;
  std::string c;
  if (lerConversa(sms, n, c, fim)) {               // "WA#7xk Maria\ntexto"
    const size_t quebra = sms.find('\n', fim);
    const std::string titulo = quebra == std::string::npos ? sms.substr(fim) : sms.substr(fim, quebra - fim);
    numero = endereco(c, titulo);
    texto = quebra == std::string::npos ? "" : sms.substr(quebra + 1);
    return true;
  }
  if (sms.compare(0, 3, "WA!") == 0) {            // aviso
    numero = PONTE;
    texto = aparar(sms.substr(3));
    return true;
  }
  if (sms.compare(0, 3, "WA?") == 0) {            // a lista das conversas
    numero = PONTE;
    texto = "Conversas no WhatsApp:\n" + aparar(sms.substr(3));
    return true;
  }
  return false;
}

std::string silenciar(const std::string& numero) { return "WA-" + codigo(numero); }
std::string reativar(const std::string& numero) { return "WA+" + codigo(numero); }

bool mesmoNumero(const std::string& a, const std::string& b) {
  const std::string x = digitos(a), y = digitos(b);
  size_t n = x.size() < y.size() ? x.size() : y.size();
  if (n > 11) n = 11;
  if (n < 10) return false;
  return x.compare(x.size() - n, n, y, y.size() - n, n) == 0;
}

}  // namespace whatsapp
