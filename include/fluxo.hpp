#ifndef FLUXO_HPP
#define FLUXO_HPP

#include "construtivo/construcao.hpp"
#include <functional>

using EtapaSA = std::function<void(const Instancia &, Solucao &, unsigned)>;
struct ParametrosExecucao {
    ParametrosConstrucao construcao;
    unsigned semente = 1;
    int maximoGruposRefinamento = 100;
    double segundosRefinamento = 0.5;
};
struct ResultadoExecucao {
    Solucao solucao;
    EstatisticasConstrucao construcao;
    double custoConstrucao = 0, custoAposSA = 0, segundosRefinamento = 0;
    int melhoriasRefinamento = 0;
    bool saExecutado = false;
};

// Construir -> SA opcional -> refinar -> validar. Sem callback, pula SA.
// O callback recebe uma copia; resultados inviaveis lancam erro e pioras nao
// substituem a melhor solucao. Capturas permitem adaptar os parametros do SA.
ResultadoExecucao executarFluxo(const Instancia &inst, const ParametrosExecucao &parametros = {}, const EtapaSA &sa = {});

#endif
