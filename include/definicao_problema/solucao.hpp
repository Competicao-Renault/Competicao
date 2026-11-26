#ifndef SOLUCAO_HPP
#define SOLUCAO_HPP

#include "definicao_problema/instancia.hpp"
#include <limits>

struct AlocacaoFluxo {
    int rota1 = -1, rota2 = -1;
    int quantidade1 = 0, quantidade2 = 0;
};
struct Reutilizacao { int rotaOrigem, rotaDestino, quantidade; };
struct Solucao {
    std::vector<AlocacaoFluxo> alocacoes; // Indexadas pelo ID do fluxo.
    std::vector<Reutilizacao> reutilizacoes;
    std::vector<double> custosCenarios;
    double funcaoObjetivo = std::numeric_limits<double>::infinity();
    int piorCenario = -1;
    bool viavel = false;
};

// Recalcula todos os caches. Inviabilidade resulta em objetivo infinito e motivo opcional.
bool avaliarSolucao(const Instancia &inst, Solucao &solucao, std::string *motivo = nullptr);
Solucao criarSolucaoSpot(const Instancia &inst);
// Exporta somente solucoes viaveis, no formato oficial {x, y}, sem os caches.
void salvarSolucao(const Instancia &inst, const Solucao &solucao, const std::string &caminho);

#endif
