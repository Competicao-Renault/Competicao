#ifndef FLUXO_HPP
#define FLUXO_HPP

#include "construtivo/construcao.hpp"
#include "meta_heuristicas/simulated_annealing.hpp"
#include "meta_heuristicas/busca_tabu.hpp"
#include "refinamento/refinamento.hpp"
#include <functional>

using EtapaSA = std::function<void(const Instancia &, Solucao &, unsigned)>;
struct ParametrosExecucao {
    ParametrosConstrucao construcao;
    ParametrosSA sa;
    bool executarSA = true;
    ParametrosTabu tabu;
    bool executarTabu = false;
    unsigned semente = 1;
    int maximoGruposRefinamento = 100;
    double segundosRefinamento = 0.5;
    ParametrosRefinamento refinamento;
    int tamanhoGrupoRefinamento = 3;
};
struct ResultadoExecucao {
    Solucao solucao;
    EstatisticasConstrucao construcao;
    EstatisticasSA estatisticasSA;
    EstatisticasTabu estatisticasTabu;
    double custoAposTabu = 0;
    bool tabuExecutado = false;
    double custoConstrucao = 0, custoAposSA = 0, segundosRefinamento = 0;
    int melhoriasRefinamento = 0;
    int nosBB = 0, podasBB = 0;
    bool saExecutado = false;
};

// Construir -> SA integrado (ou callback) -> tabu opcional -> refinar -> validar.
// Sem callback usa ParametrosExecucao::executarSA; callback explicito tem prioridade.
// O callback recebe uma copia; resultados inviaveis lancam erro e pioras nao
// substituem a melhor solucao. Capturas permitem adaptar os parametros do SA.
ResultadoExecucao executarFluxo(const Instancia &inst, const ParametrosExecucao &parametros = {}, const EtapaSA &sa = {});

#endif
