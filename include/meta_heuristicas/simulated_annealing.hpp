#ifndef SIMULATED_ANNEALING_HPP
#define SIMULATED_ANNEALING_HPP

#include "definicao_problema/solucao.hpp"

struct ParametrosSA {
    double temperaturaInicial;
    double temperaturaFinal;
    double alfa;
    int saMax; // Candidatos viáveis avaliados por nível de temperatura.
    int maxPropostas; // Limite global de tentativas, incluindo propostas descartadas.
    double tempoLimite; // Limite de execução em segundos.
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
    MotivoEncerramentoSA motivoEncerramento = MotivoEncerramentoSA::TemperaturaFinal;
};

// Retorna a melhor solução viável encontrada, partindo de uma solução inicial viável.
Solucao simulatedAnnealing(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros, EstatisticasSA *estatisticas = nullptr);

#endif
