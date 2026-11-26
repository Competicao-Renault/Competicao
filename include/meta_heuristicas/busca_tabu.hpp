#ifndef BUSCA_TABU_HPP
#define BUSCA_TABU_HPP

#include "definicao_problema/solucao.hpp"

struct ParametrosTabu {
    int tamanhoLista = 30, tamanhoVizinhanca = 20, maxIteracoes = 1000, maxPropostas = 100000;
    double tempoMax = 1.0;
};
struct EstatisticasTabu {
    int iteracoes = 0, propostas = 0, viaveis = 0, aceitos = 0, bloqueados = 0, aspiracoes = 0, reinicios = 0;
    double custoInicial = 0, melhorCusto = 0, custoCorrente = 0, tempoExecucao = 0;
};
// Ignora caches, ordem dos pares e ordem das reutilizacoes.
std::uint64_t hashTabu(const Solucao &solucao);
void validarParametrosTabu(const ParametrosTabu &parametros);
// Explora pioras, mas devolve em solucao apenas a melhor incumbente viavel.
double buscaTabu(const Instancia &inst, Solucao &solucao, const ParametrosTabu &parametros = {}, EstatisticasTabu *estatisticas = nullptr);
// Interface preservada do cpm2.zip.
double busca_tabu(const Instancia &inst, Solucao &solucao, int tamanhoLista, int tamanhoVizinhanca, double tempoMax);

#endif
