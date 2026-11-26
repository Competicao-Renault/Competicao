#include "fluxo.hpp"
#include "refinamento/refinamento.hpp"
#include "utilitarios/cronometro.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
struct SementeTemporaria {
    std::mt19937 anterior = gerador;
    explicit SementeTemporaria(unsigned valor) { semente(valor); }
    ~SementeTemporaria() { gerador = anterior; }
};
}

ResultadoExecucao executarFluxo(const Instancia &inst, const ParametrosExecucao &p, const EtapaSA &sa) {
    if (p.maximoGruposRefinamento < 0 || !std::isfinite(p.segundosRefinamento) || p.segundosRefinamento < 0) throw std::invalid_argument("Parametros do fluxo invalidos");
    if (!sa && p.executarSA) validarParametrosSA(p.sa);
    ResultadoExecucao resultado;
    auto &sol = resultado.solucao;
    sol = construirSolucao(inst, p.construcao, p.semente, &resultado.construcao);
    resultado.custoConstrucao = sol.funcaoObjetivo;
    if (sa || p.executarSA) {
        Solucao candidata = sol;
        if (sa) sa(inst, candidata, p.semente);
        else {
            SementeTemporaria escopo(p.semente);
            candidata = simulatedAnnealing(inst, sol, p.sa, &resultado.estatisticasSA);
        }
        std::string motivo;
        if (!avaliarSolucao(inst, candidata, &motivo)) throw std::runtime_error("SA retornou solucao inviavel: " + motivo);
        if (candidata.funcaoObjetivo <= sol.funcaoObjetivo) sol = std::move(candidata);
        resultado.saExecutado = true;
    }
    resultado.custoAposSA = sol.funcaoObjetivo;
    Cronometro tempo;
    for (const auto &f : inst.fluxos) {
        if (tempo.segundosDecorridos() >= p.segundosRefinamento) break;
        resultado.melhoriasRefinamento += refinarQuantidade(inst, sol, f.id).melhorou;
    }
    std::mt19937 rng(p.semente);
    for (int i = 0; i < p.maximoGruposRefinamento && inst.fluxos.size() >= 2; ++i) {
        double restante = p.segundosRefinamento - tempo.segundosDecorridos();
        if (restante <= 0) break;
        auto grupo = selecionarGrupo(inst, sol, rng, std::min(3, static_cast<int>(inst.fluxos.size())), p.construcao.percentualAleatorio);
        ParametrosRefinamento limites;
        limites.limiteSegundos = std::min(0.2, restante);
        resultado.melhoriasRefinamento += refinarGrupo(inst, sol, grupo, limites).melhorou;
        for (int f : grupo) {
            if (tempo.segundosDecorridos() >= p.segundosRefinamento) break;
            resultado.melhoriasRefinamento += refinarQuantidade(inst, sol, f).melhorou;
        }
    }
    resultado.segundosRefinamento = tempo.segundosDecorridos();
    std::string motivo;
    if (!avaliarSolucao(inst, sol, &motivo)) throw std::runtime_error("Falha na validacao final: " + motivo);

    return resultado;
}
