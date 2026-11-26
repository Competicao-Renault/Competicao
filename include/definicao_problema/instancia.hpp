#ifndef INSTANCIA_HPP
#define INSTANCIA_HPP

#include <cstdint>
#include <string>
#include <vector>

using Inteiro = std::int64_t;

struct Localizacao { double latitude = 0, longitude = 0; };
struct Dimensoes { int portos = 0, sites = 0, companhias = 0, fluxos = 0, rotas = 0, cenarios = 0; };
struct Metadados {
    std::string nome, descricao, unidadeVolume, unidadeCusto;
    double alpha = 0;
    Dimensoes dimensoes;
};
struct Porto {
    int id;
    std::string codigo, nome;
    Inteiro capacidade;
    Localizacao localizacao;
};
struct AcessoPorto { int porto; Inteiro custo; };
struct Site {
    int id;
    std::string nome;
    Inteiro capacidadeReutilizacao;
    Localizacao localizacao;
    std::vector<AcessoPorto> acessos;
};
struct Companhia {
    int id;
    std::string nome;
    Inteiro custoFixo, capacidade;
    int maximoSites;
};
struct Fluxo {
    int id, origem, destino;
    Inteiro demanda;
    std::vector<int> rotas;
};
struct Rota {
    int id, fluxo, companhia, portoOrigem, portoDestino;
    Inteiro custoMaritimo;
};
struct Cenario { int id; std::vector<int> rotasIndisponiveis; };

struct Instancia {
    Metadados meta;
    std::vector<Porto> portos;
    std::vector<Site> sites;
    std::vector<Companhia> companhias;
    std::vector<Fluxo> fluxos;
    std::vector<Rota> rotas;
    std::vector<Cenario> cenarios;
    int companhiaSpot = -1, portoSpot = -1;
    Inteiro demandaTotal = 0;
    // -1 indica acesso inexistente; custo zero e um acesso valido.
    std::vector<std::vector<Inteiro>> custosRodoviarios;
    std::vector<Inteiro> custosRotas; // Custo unitario sem reutilizacao.
    std::vector<std::vector<unsigned char>> indisponivel; // [cenario][rota]
};

// Retorna uma instancia validada ou lanca std::runtime_error com o contexto.
Instancia lerInstancia(const std::string &caminho);

#endif
