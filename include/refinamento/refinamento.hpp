#ifndef REFINAMENTO_HPP
#define REFINAMENTO_HPP

#include "definicao_problema/solucao.hpp"

struct ParametrosRefinamento {
    int maximoAlternativas = 6, maximoCombinacoes = 216;
    double limiteSegundos = 1.0;
};
struct ResultadoRefinamento {
    int candidatosTestados = 0;
    bool melhorou = false, concluido = true;
};
struct AlternativasFluxo { int fluxo; std::vector<AlocacaoFluxo> alocacoes; };

// Entrada viavel. So instala melhoria estrita e recalcula seus caches; nao usa RNG.
// Interior: preserva y e otimiza exatamente a divisao do par atual.
// Extremos com uma rota: usam a reducao de y prevista em alterarAlocacao.
ResultadoRefinamento refinarQuantidade(const Instancia &inst, Solucao &solucao, int fluxo);
// Enumera 2 ou 3 fluxos. Cada candidato parte da mesma entrada e ajusta y como
// alterarAlocacoes. A exatidao se limita as alternativas fornecidas (mais a atual).
ResultadoRefinamento enumerarGrupo(const Instancia &inst, Solucao &solucao, const std::vector<AlternativasFluxo> &grupo, const ParametrosRefinamento &parametros = {});
// Gera uma lista curta por fluxo, preservando atual e SPOT, e chama a enumeracao.
// Pares robustos usam divisoes 25/50/75%; nao enumera todas as quantidades.
ResultadoRefinamento refinarGrupo(const Instancia &inst, Solucao &solucao, const std::vector<int> &fluxos, const ParametrosRefinamento &parametros = {});

#endif
