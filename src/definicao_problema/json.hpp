#ifndef JSON_HPP
#define JSON_HPP

#include <cctype>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace leitura {
struct Json {
    enum Tipo { Nulo, Booleano, Numero, Texto, Lista, Objeto } tipo = Nulo;
    std::string valor;
    std::vector<Json> lista;
    std::map<std::string, Json> objeto;

    const Json &campo(const std::string &nome) const {
        if (tipo != Objeto || !objeto.count(nome)) throw std::runtime_error("Campo ausente: " + nome);
        return objeto.at(nome);
    }
    const std::vector<Json> &itens() const {
        if (tipo != Lista) throw std::runtime_error("Esperada uma lista JSON");
        return lista;
    }
    std::string texto() const {
        if (tipo != Texto) throw std::runtime_error("Esperado um texto JSON");
        return valor;
    }
    std::int64_t inteiro() const {
        if (tipo != Numero || valor.find_first_of(".eE") != std::string::npos) throw std::runtime_error("Esperado um inteiro JSON");
        return std::stoll(valor);
    }
    double real() const {
        if (tipo != Numero) throw std::runtime_error("Esperado um numero JSON");
        return std::stod(valor);
    }
};

class LeitorJson {
    const std::string &fonte;
    std::size_t pos = 0;
    [[noreturn]] void erro() const { throw std::runtime_error("JSON invalido no byte " + std::to_string(pos)); }
    void espacos() { while (pos < fonte.size() && std::isspace(static_cast<unsigned char>(fonte[pos]))) ++pos; }
    bool consumir(char c) {
        espacos();
        if (pos == fonte.size() || fonte[pos] != c) return false;
        ++pos;
        return true;
    }
    unsigned hexadecimal() {
        unsigned codigo = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos == fonte.size()) erro();
            char c = fonte[pos++];
            codigo *= 16;
            if (c >= '0' && c <= '9') codigo += c - '0';
            else if (c >= 'a' && c <= 'f') codigo += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') codigo += c - 'A' + 10;
            else erro();
        }
        return codigo;
    }
    std::string texto() {
        if (!consumir('"')) erro();
        std::string saida;
        while (pos < fonte.size()) {
            unsigned char c = fonte[pos++];
            if (c == '"') return saida;
            if (c < 32) erro();
            if (c != '\\') { saida += static_cast<char>(c); continue; }
            if (pos == fonte.size()) erro();
            c = fonte[pos++];
            const std::string escapes = "\"\\/bfnrt", valores = "\"\\/\b\f\n\r\t";
            auto indice = escapes.find(static_cast<char>(c));
            if (indice != std::string::npos) { saida += valores[indice]; continue; }
            if (c != 'u') erro();
            unsigned codigo = hexadecimal();
            if (codigo >= 0xD800 && codigo <= 0xDBFF) {
                if (fonte.compare(pos, 2, "\\u") != 0) erro();
                pos += 2;
                unsigned baixo = hexadecimal();
                if (baixo < 0xDC00 || baixo > 0xDFFF) erro();
                codigo = 0x10000 + ((codigo - 0xD800) << 10) + baixo - 0xDC00;
            } else if (codigo >= 0xDC00 && codigo <= 0xDFFF) erro();
            if (codigo <= 0x7F) saida += static_cast<char>(codigo);
            else {
                int bits = 6;
                if (codigo > 0xFFFF) { saida += static_cast<char>(0xF0 | (codigo >> 18)); bits = 12; }
                else if (codigo > 0x7FF) { saida += static_cast<char>(0xE0 | (codigo >> 12)); bits = 6; }
                else { saida += static_cast<char>(0xC0 | (codigo >> 6)); bits = 0; }
                for (; bits >= 0; bits -= 6) saida += static_cast<char>(0x80 | ((codigo >> bits) & 63));
            }
        }
        erro();
    }
    Json valor(int profundidade) {
        espacos();
        if (pos == fonte.size() || profundidade > 64) erro();
        Json j;
        char c = fonte[pos];
        if (c == '{' || c == '[') {
            ++pos;
            char fim = ']';
            j.tipo = Json::Lista;
            if (c == '{') { fim = '}'; j.tipo = Json::Objeto; }
            if (consumir(fim)) return j;
            do {
                if (j.tipo == Json::Objeto) {
                    std::string chave = texto();
                    if (!consumir(':')) erro();
                    if (!j.objeto.emplace(chave, valor(profundidade + 1)).second) erro();
                } else j.lista.push_back(valor(profundidade + 1));
                if (consumir(fim)) return j;
            } while (consumir(','));
            erro();
        }
        if (c == '"') { j.tipo = Json::Texto; j.valor = texto(); return j; }
        for (const std::string literal : {"null", "true", "false"}) {
            if (fonte.compare(pos, literal.size(), literal) != 0) continue;
            j.tipo = Json::Booleano;
            if (literal == "null") j.tipo = Json::Nulo;
            pos += literal.size();
            return j;
        }
        std::size_t inicio = pos;
        if (fonte[pos] == '-') ++pos;
        auto digitos = [&]() {
            std::size_t antes = pos;
            while (pos < fonte.size() && fonte[pos] >= '0' && fonte[pos] <= '9') ++pos;
            if (pos == antes) erro();
        };
        if (pos < fonte.size() && fonte[pos] == '0') ++pos;
        else digitos();
        if (pos < fonte.size() && fonte[pos] == '.') { ++pos; digitos(); }
        if (pos < fonte.size() && (fonte[pos] == 'e' || fonte[pos] == 'E')) {
            ++pos;
            if (pos < fonte.size() && (fonte[pos] == '+' || fonte[pos] == '-')) ++pos;
            digitos();
        }
        j.tipo = Json::Numero;
        j.valor = fonte.substr(inicio, pos - inicio);
        return j;
    }
public:
    explicit LeitorJson(const std::string &texto) : fonte(texto) {}
    Json ler() {
        // Aceita BOM UTF-8 no inicio de arquivos salvos por editores Windows.
        if (fonte.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
        Json resultado = valor(0);
        espacos();
        if (pos != fonte.size()) erro();
        return resultado;
    }
};
}
#endif
