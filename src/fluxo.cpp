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
    if (p.maximoGruposRefinamento < 0 || !std::isfinite(p.segundosRefinamento) || p.segundosRefinamento < 0 || p.tamanhoGrupoRefinamento < 2 || p.tamanhoGrupoRefinamento > 6) throw std::invalid_argument("Parametros do fluxo invalidos");
    if (!sa && p.executarSA) validarParametrosSA(p.sa);
    if (p.executarTabu) validarParametrosTabu(p.tabu);
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
        candidata.permitirInviavel = false;
        std::string motivo;
        if (!avaliarSolucao(inst, candidata, &motivo)) throw std::runtime_error("SA retornou solucao inviavel: " + motivo);
        if (candidata.funcaoObjetivo <= sol.funcaoObjetivo) sol = std::move(candidata);
        resultado.saExecutado = true;
    }
    resultado.custoAposSA = sol.funcaoObjetivo;
    std::mt19937 rng(p.semente);
    auto refinar = [&](double segundos, int maximoGrupos) {
        if (segundos <= 0) return;
        Cronometro tempo;
        for (const auto &f : inst.fluxos) {
            if (tempo.segundosDecorridos() >= segundos) break;
            resultado.melhoriasRefinamento += refinarQuantidade(inst, sol, f.id).melhorou;
        }
        for (int i = 0; i < maximoGrupos && inst.fluxos.size() >= 2; ++i) {
            double restante = segundos - tempo.segundosDecorridos();
            if (restante <= 0) break;
            auto grupo = selecionarGrupo(inst, sol, rng, std::min(p.tamanhoGrupoRefinamento, static_cast<int>(inst.fluxos.size())), p.construcao.percentualAleatorio);
            ParametrosRefinamento limites = p.refinamento;
            limites.limiteSegundos = std::min({0.2, restante, limites.limiteSegundos});
            auto busca = refinarGrupo(inst, sol, grupo, limites);
            resultado.melhoriasRefinamento += busca.melhorou;
            resultado.nosBB += busca.nosVisitados;
            resultado.podasBB += busca.podasCusto + busca.podasCapacidade + busca.podasMinimo;
            for (int f : grupo) {
                if (tempo.segundosDecorridos() >= segundos) break;
                resultado.melhoriasRefinamento += refinarQuantidade(inst, sol, f).melhorou;
            }
        }
        resultado.segundosRefinamento += tempo.segundosDecorridos();
    };
    double segundosAntes = 0;
    int gruposAntes = 0;
    if (p.dividirBB && p.executarTabu && p.tabu.tempoMax > 0 && p.tabu.maxIteracoes > 0 && p.tabu.maxPropostas > 0 &&
        p.refinamento.usarBB && inst.fluxos.size() >= 2 && p.maximoGruposRefinamento >= 2) {
        segundosAntes = p.segundosRefinamento / 2;
        gruposAntes = p.maximoGruposRefinamento / 2;
    }
    double segundosFinais = p.segundosRefinamento - segundosAntes;
    refinar(segundosAntes, gruposAntes);
    resultado.custoAntesTabu = sol.funcaoObjetivo;
    if (p.executarTabu) {
        SementeTemporaria escopo(p.semente);
        buscaTabu(inst, sol, p.tabu, &resultado.estatisticasTabu);
        resultado.tabuExecutado = true;
    }
    resultado.custoAposTabu = sol.funcaoObjetivo;
    refinar(segundosFinais, p.maximoGruposRefinamento - gruposAntes);
    std::string motivo;
    if (!avaliarSolucao(inst, sol, &motivo)) throw std::runtime_error("Falha na validacao final: " + motivo);

    return resultado;
}
