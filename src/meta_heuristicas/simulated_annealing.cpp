#include "meta_heuristicas/simulated_annealing.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <limits>

const char *nomeMovimentoSA(MovimentoSA movimento) {
    static const char *nomes[] = {"aleatorio", "divisao", "substituicao", "companhia", "site", "reutilizacao", "grupo", "quantidade"};
    auto indice = static_cast<std::size_t>(movimento);
    if (indice >= numeroMovimentosSA) return "desconhecido";

    return nomes[indice];
}

namespace {
std::size_t sortearMovimento(const ParametrosSA &p) {
    int total = 0, quantidade = 0;
    std::size_t unico = 0;
    for (std::size_t i = 0; i < numeroMovimentosSA; ++i) if (p.pesosMovimentos[i] > 0) {
        total += p.pesosMovimentos[i]; ++quantidade; unico = i;
    }
    if (quantidade == 1) return unico; // Preserva os sorteios do SA original.
    int sorteio = inteiroAleatorio(1, total);
    for (std::size_t i = 0; i < numeroMovimentosSA; ++i) {
        sorteio -= p.pesosMovimentos[i];
        if (sorteio <= 0) return i;
    }

    return unico;
}
bool propor(const Instancia &inst, Solucao &sol, std::size_t movimento, const ParametrosSA &p) {
    if (movimento == static_cast<std::size_t>(MovimentoSA::Grupo)) return movimentoGrupo(inst, sol, 1, p.percentualAleatorioGrupos);
    using Operador = bool (*)(const Instancia &, Solucao &, int);
    static const Operador operadores[] = {movimentoAleatorio, movimentoDivisao, movimentoSubstituicao, movimentoCompanhia, movimentoSite, movimentoReutilizacao, nullptr, movimentoQuantidade};

    return operadores[movimento](inst, sol, 1);
}
}

void validarParametrosSA(const ParametrosSA &parametros) {
    if (!std::isfinite(parametros.temperaturaInicial) || !std::isfinite(parametros.temperaturaFinal) ||
        parametros.temperaturaFinal <= 0.0 || parametros.temperaturaInicial <= parametros.temperaturaFinal ||
        !(parametros.alfa > 0.0 && parametros.alfa < 1.0)) {
        throw std::invalid_argument("Temperaturas ou alfa invalidos para o SA");
    }
    if (parametros.saMax <= 0 || parametros.maxPropostas <= 0 ||
        !std::isfinite(parametros.tempoLimite) || parametros.tempoLimite <= 0.0) {
        throw std::invalid_argument("Limites de busca invalidos para o SA");
    }
    Inteiro soma = 0;
    for (int peso : parametros.pesosMovimentos) {
        if (peso < 0) throw std::invalid_argument("Peso de movimento negativo");
        soma += peso;
    }
    if (soma <= 0 || soma > std::numeric_limits<int>::max() || parametros.percentualAleatorioGrupos < 0 || parametros.percentualAleatorioGrupos > 100) throw std::invalid_argument("Pesos ou percentual de grupos invalidos");
}

Solucao simulatedAnnealing(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros, EstatisticasSA *estatisticas) {
    const Cronometro cronometro;
    if (estatisticas) *estatisticas = {};
    validarParametrosSA(parametros);
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
            std::size_t movimento = sortearMovimento(parametros);
            if (estatisticas) ++estatisticas->propostasPorMovimento[movimento];
            if (!propor(inst, candidata, movimento, parametros)) {
                continue;
            }
            ++viaveis;
            if (estatisticas) ++estatisticas->viaveisPorMovimento[movimento];
            const double delta = candidata.funcaoObjetivo - atual.funcaoObjetivo;
            if (delta <= 0.0 || realAleatorio(0.0, 1.0) < std::exp(-delta / temperatura)) {
                if (estatisticas) {
                    ++estatisticas->aceitosPorMovimento[movimento];
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
