#ifndef SIMULATED_ANNEALING_HPP
#define SIMULATED_ANNEALING_HPP

#include "definicao_problema/solucao.hpp"
#include <array>

enum class MovimentoSA { Aleatorio, Divisao, Substituicao, Companhia, Site, Reutilizacao, Grupo, Quantidade, Total };
inline constexpr std::size_t numeroMovimentosSA = static_cast<std::size_t>(MovimentoSA::Total);

struct ParametrosSA {
    double temperaturaInicial = 1000000, temperaturaFinal = 10000, alfa = 0.95;
    int saMax = 200; // Candidatos viaveis por nivel de temperatura.
    int maxPropostas = 100000; // Inclui descartes, sem repeticoes ocultas por movimento.
    double tempoLimite = 1.0;
    std::array<int, numeroMovimentosSA> pesosMovimentos{1, 2, 2, 1, 1, 1, 1, 2};
    int percentualAleatorioGrupos = 30;
};

enum class MotivoEncerramentoSA { TemperaturaFinal, LimitePropostas, LimiteTempo };

struct EstatisticasSA {
    int propostas = 0;
    int candidatosViaveis = 0;
    int melhoriasAceitas = 0;
    int empatesAceitos = 0;
    int piorasAceitas = 0;
    int recordes = 0;
    int niveisConcluidos = 0;
    double custoInicial = 0.0;
    double melhorCusto = 0.0;
    double custoCorrente = 0.0;
    double temperaturaFinal = 0.0;
    double tempoExecucao = 0.0; // Segundos.
    std::array<int, numeroMovimentosSA> propostasPorMovimento{}, viaveisPorMovimento{}, aceitosPorMovimento{};
    MotivoEncerramentoSA motivoEncerramento = MotivoEncerramentoSA::TemperaturaFinal;
};

// Retorna a melhor solução viável encontrada, partindo de uma solução inicial viável.
Solucao simulatedAnnealing(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros, EstatisticasSA *estatisticas = nullptr);
void validarParametrosSA(const ParametrosSA &parametros);
const char *nomeMovimentoSA(MovimentoSA movimento);

#endif
