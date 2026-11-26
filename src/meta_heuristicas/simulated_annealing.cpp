#include "meta_heuristicas/simulated_annealing.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

Solucao simulatedAnnealing(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros, EstatisticasSA *estatisticas) {
    const Cronometro cronometro;
    if (estatisticas) {
        *estatisticas = {};
    }
    if (!std::isfinite(parametros.temperaturaInicial) || !std::isfinite(parametros.temperaturaFinal) ||
        parametros.temperaturaFinal <= 0.0 || parametros.temperaturaInicial <= parametros.temperaturaFinal ||
        !(parametros.alfa > 0.0 && parametros.alfa < 1.0)) {
        throw std::invalid_argument("Temperaturas ou alfa invalidos para o SA");
    }
    if (parametros.saMax <= 0 || parametros.maxPropostas <= 0 ||
        !std::isfinite(parametros.tempoLimite) || parametros.tempoLimite <= 0.0) {
        throw std::invalid_argument("Limites de busca invalidos para o SA");
    }

    Solucao atual = inicial;
    std::string motivo;
    if (!inicial.viavel || !std::isfinite(inicial.funcaoObjetivo) || !avaliarSolucao(inst, atual, &motivo)) {
        throw std::invalid_argument("SA exige solucao inicial viavel com custo valido. " + motivo);
    }
    if (estatisticas) {
        estatisticas->custoInicial = atual.funcaoObjetivo;
    }
    Solucao melhor = atual;
    Solucao candidata;
    int propostas = 0;
    double temperatura = parametros.temperaturaInicial;
    bool prazoEsgotado = false;
    auto podePropor = [&] {
        if (propostas >= parametros.maxPropostas) {
            return false;
        }
        prazoEsgotado = cronometro.segundosDecorridos() >= parametros.tempoLimite;
        return !prazoEsgotado;
    };

    while (temperatura > parametros.temperaturaFinal && podePropor()) {
        int viaveis = 0;
        for (; viaveis < parametros.saMax && podePropor();) {
            candidata = atual;
            ++propostas;
            if (!movimentoAleatorio(inst, candidata, 1)) {
                continue;
            }
            ++viaveis;
            const double delta = candidata.funcaoObjetivo - atual.funcaoObjetivo;
            if (delta <= 0.0 || realAleatorio(0.0, 1.0) < std::exp(-delta / temperatura)) {
                if (estatisticas) {
                    if (delta < 0.0) {
                        ++estatisticas->melhoriasAceitas;
                    } else if (delta == 0.0) {
                        ++estatisticas->empatesAceitos;
                    } else {
                        ++estatisticas->piorasAceitas;
                    }
                }
                std::swap(atual, candidata);
                if (atual.funcaoObjetivo < melhor.funcaoObjetivo) {
                    melhor = atual;
                    if (estatisticas) {
                        ++estatisticas->recordes;
                    }
                }
            }
        }

        if (estatisticas) {
            estatisticas->candidatosViaveis += viaveis;
            if (viaveis == parametros.saMax) {
                ++estatisticas->niveisConcluidos;
            }
        }
        const double resfriada = temperatura * parametros.alfa;
        // Garante progresso caso o produto arredonde para a temperatura atual.
        temperatura = std::min(resfriada, std::nextafter(temperatura, 0.0));
    }

    if (estatisticas) {
        estatisticas->propostas = propostas;
        estatisticas->melhorCusto = melhor.funcaoObjetivo;
        estatisticas->custoCorrente = atual.funcaoObjetivo;
        estatisticas->temperaturaFinal = temperatura;
        estatisticas->tempoExecucao = cronometro.segundosDecorridos();
        // Precedência: orçamento, prazo observado nas checagens, temperatura.
        if (propostas >= parametros.maxPropostas) {
            estatisticas->motivoEncerramento = MotivoEncerramentoSA::LimitePropostas;
        } else if (prazoEsgotado) {
            estatisticas->motivoEncerramento = MotivoEncerramentoSA::LimiteTempo;
        }
    }

    return melhor;
}
