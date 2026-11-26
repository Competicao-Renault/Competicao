#ifndef CONSTRUCAO_HPP
#define CONSTRUCAO_HPP

#include "definicao_problema/solucao.hpp"
#include <random>

struct ParametrosConstrucao {
    int maximoBlocos = 200, maximoFluxosBloco = 6;
    int alternativasPorFluxo = 8, propostasPorBloco = 64, tamanhoRCL = 5;
    int percentualAleatorio = 30;
    double limiteSegundos = 1.0;
};
struct EstatisticasConstrucao {
    int blocos = 0, candidatos = 0, blocosAceitos = 0;
    double segundos = 0;
};

// Compartilhada com o refinamento: parcela aleatoria e restante dividido entre
// companhia e pior cenario. Exige solucao previamente avaliada e viavel.
// Escolhe IDs distintos, sem usar o RNG global.
std::vector<int> selecionarGrupo(const Instancia &inst, const Solucao &solucao, std::mt19937 &rng, int tamanho = 3, int percentualAleatorio = 30);
// Gerador comercial compartilhado; exige entrada avaliada. Retorna vazio no prazo.
std::vector<std::vector<AlocacaoFluxo>> gerarAlternativasBloco(const Instancia &inst, const Solucao &solucao, const std::vector<int> &grupo, int maximo, double limiteSegundos);
// SPOT -> blocos gulosos aleatorizados; desempata por soma dos custos e SPOT.
// Retorna sempre viavel e nunca pior que SPOT; limite zero retorna a baseline.
Solucao construirSolucao(const Instancia &inst, const ParametrosConstrucao &parametros = {}, unsigned semente = 1, EstatisticasConstrucao *estatisticas = nullptr);

#endif
