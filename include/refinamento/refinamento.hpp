#ifndef REFINAMENTO_HPP
#define REFINAMENTO_HPP

#include "definicao_problema/solucao.hpp"

struct ParametrosRefinamento {
    int maximoAlternativas = 6, maximoCombinacoes = 216;
    double limiteSegundos = 1.0;
    int maximoNos = 10000;
    bool usarBB = true, alternativasComerciais = false;
};
struct ResultadoRefinamento {
    int candidatosTestados = 0;
    bool melhorou = false, concluido = true;
    int nosVisitados = 0, podasCusto = 0, podasCapacidade = 0, podasMinimo = 0;
};
struct AlternativasFluxo { int fluxo; std::vector<AlocacaoFluxo> alocacoes; };

// Entrada viavel. So instala melhoria estrita e recalcula seus caches; nao usa RNG.
// Interior: preserva y e otimiza exatamente a divisao do par atual.
// Extremos com uma rota: usam a reducao de y prevista em alterarAlocacao.
ResultadoRefinamento refinarQuantidade(const Instancia &inst, Solucao &solucao, int fluxo);
// Referencia exaustiva para 2 a 6 fluxos. Parte da mesma entrada e ajusta y como
// alterarAlocacoes. A exatidao se limita as alternativas fornecidas (mais a atual).
ResultadoRefinamento enumerarGrupo(const Instancia &inst, Solucao &solucao, const std::vector<AlternativasFluxo> &grupo, const ParametrosRefinamento &parametros = {});
// B&B exato nas listas fornecidas (incluindo a atual), para 2 a 6 fluxos.
// Concluido significa vizinhanca encerrada, nao otimalidade global da instancia.
ResultadoRefinamento branchAndBound(const Instancia &inst, Solucao &solucao, const std::vector<AlternativasFluxo> &grupo, const ParametrosRefinamento &parametros = {});
std::vector<AlternativasFluxo> gerarAlternativasGrupo(const Instancia &inst, const Solucao &solucao, const std::vector<int> &fluxos, const ParametrosRefinamento &parametros = {});
// Gera listas preservando atual e SPOT e usa B&B por padrao (ou enumeracao).
// Modo basico: 25/50/75%. Comercial: pontos do gerador da construtiva.
ResultadoRefinamento refinarGrupo(const Instancia &inst, Solucao &solucao, const std::vector<int> &fluxos, const ParametrosRefinamento &parametros = {});

#endif
