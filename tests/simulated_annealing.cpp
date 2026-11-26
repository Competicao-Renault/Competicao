#include "meta_heuristicas/simulated_annealing.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {
int verificacoes = 0;
void verificar(bool condicao, const char *mensagem) {
    ++verificacoes;
    if (!condicao) {
        throw std::runtime_error(mensagem);
    }
}

bool mesmaSolucao(const Solucao &a, const Solucao &b) {
    auto alocacao = [](const AlocacaoFluxo &x) {
        return std::tie(x.rota1, x.rota2, x.quantidade1, x.quantidade2);
    };
    auto reutilizacao = [](const Reutilizacao &y) {
        return std::tie(y.rotaOrigem, y.rotaDestino, y.quantidade);
    };
    return a.viavel == b.viavel && a.funcaoObjetivo == b.funcaoObjetivo &&
           a.piorCenario == b.piorCenario && a.custosCenarios == b.custosCenarios &&
           std::equal(a.alocacoes.begin(), a.alocacoes.end(), b.alocacoes.begin(), b.alocacoes.end(),
                      [&](const auto &x, const auto &y) { return alocacao(x) == alocacao(y); }) &&
           std::equal(a.reutilizacoes.begin(), a.reutilizacoes.end(), b.reutilizacoes.begin(), b.reutilizacoes.end(),
                      [&](const auto &x, const auto &y) { return reutilizacao(x) == reutilizacao(y); });
}

void verificarRejeicao(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros) {
    try {
        simulatedAnnealing(inst, inicial, parametros);
    } catch (const std::invalid_argument &) {
        ++verificacoes;
        return;
    }
    throw std::runtime_error("Entrada invalida foi aceita");
}

void testarValidacao(const Instancia &inst, const Solucao &inicial, const ParametrosSA &padrao) {
    const double infinito = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const ParametrosSA parametros : {
        ParametrosSA{infinito, 1, 0.9, 10, 100, 60}, {100, nan, 0.9, 10, 100, 60},
        {100, 0, 0.9, 10, 100, 60}, {1, 1, 0.9, 10, 100, 60},
        {100, 1, 0, 10, 100, 60}, {100, 1, 1, 10, 100, 60}, {100, 1, nan, 10, 100, 60},
        {100, 1, 0.9, 0, 100, 60}, {100, 1, 0.9, 10, 0, 60},
        {100, 1, 0.9, 10, 100, infinito}, {100, 1, 0.9, 10, 100, -1}
    }) {
        verificarRejeicao(inst, inicial, parametros);
    }
    Solucao invalida = inicial;
    invalida.viavel = false;
    verificarRejeicao(inst, invalida, padrao);
    invalida = inicial;
    invalida.funcaoObjetivo = nan;
    verificarRejeicao(inst, invalida, padrao);
    invalida = inicial;
    invalida.alocacoes.front().quantidade1 = 0;
    verificarRejeicao(inst, invalida, padrao);
}

void testarBusca(const Instancia &inst, const Solucao &inicial, const ParametrosSA &parametros) {
    const Solucao original = inicial;
    semente(123);
    const Solucao resultado = simulatedAnnealing(inst, inicial, parametros);
    const auto estadoGerador = gerador;
    semente(123);
    EstatisticasSA estatisticas;
    const Solucao repetida = simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    const int aceitos = estatisticas.melhoriasAceitas + estatisticas.empatesAceitos + estatisticas.piorasAceitas;
    verificar(estatisticas.propostas >= estatisticas.candidatosViaveis && aceitos <= estatisticas.candidatosViaveis &&
              estatisticas.recordes <= estatisticas.melhoriasAceitas, "Contadores de SA inconsistentes");
    verificar(estatisticas.custoInicial == inicial.funcaoObjetivo && estatisticas.melhorCusto == repetida.funcaoObjetivo &&
              estatisticas.melhorCusto <= estatisticas.custoInicial && estatisticas.melhorCusto <= estatisticas.custoCorrente,
              "Custos registrados inconsistentes");
    verificar(mesmaSolucao(resultado, repetida) && gerador == estadoGerador, "Busca nao reprodutivel");
    verificar(mesmaSolucao(inicial, original), "Solucao inicial foi modificada");
    verificar(resultado.viavel && resultado.funcaoObjetivo <= inicial.funcaoObjetivo, "Busca perdeu a solucao inicial");
    Solucao reavaliada = resultado;
    verificar(avaliarSolucao(inst, reavaliada) && mesmaSolucao(resultado, reavaliada), "Resultado invalido ou custo inconsistente");
}

void testarLimites(Instancia inst, const Solucao &inicial, ParametrosSA parametros) {
    parametros.pesosMovimentos = {1, 0, 0, 0, 0, 0, 0, 0};
    for (auto &fluxo : inst.fluxos) {
        fluxo.rotas = {inicial.alocacoes[fluxo.id].rota1};
    }
    parametros.maxPropostas = 7;
    semente(456);
    Solucao candidata = inicial;
    for (int i = 0; i < parametros.maxPropostas; ++i) {
        verificar(!movimentoAleatorio(inst, candidata, 1), "O caso de teste gerou um candidato");
    }
    const auto aposOrcamento = gerador;
    semente(456);
    const Solucao resultado = simulatedAnnealing(inst, inicial, parametros);
    verificar(mesmaSolucao(resultado, inicial) && gerador == aposOrcamento, "Descartes desrespeitaram o orcamento");

    parametros.maxPropostas = std::numeric_limits<int>::max();
    parametros.tempoLimite = std::numeric_limits<double>::min();
    const auto antesPrazo = gerador;
    EstatisticasSA estatisticas;
    estatisticas.propostas = 999;
    estatisticas.candidatosViaveis = 999;
    estatisticas.niveisConcluidos = 999;
    const Solucao comPrazo = simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.propostas == 0 && estatisticas.candidatosViaveis == 0 && estatisticas.niveisConcluidos == 0 &&
              estatisticas.motivoEncerramento == MotivoEncerramentoSA::LimiteTempo &&
              estatisticas.temperaturaFinal == parametros.temperaturaInicial &&
              estatisticas.tempoExecucao >= parametros.tempoLimite, "Prazo imediato ou reinicializacao incorreta");
    verificar(mesmaSolucao(comPrazo, inicial) && gerador == antesPrazo, "Busca iniciou proposta apos o prazo");
}
void testarNiveisEContadores() {
    Instancia inst;
    inst.portos = {{0, "P", "PORTO", 2, {}}};
    inst.sites = {{0, "A", 0, {}, {}}, {1, "B", 0, {}, {}}};
    inst.companhias = {{0, "SPOT", 0, 1, 2}};
    inst.fluxos = {{0, 0, 1, 1, {0, 1}}};
    inst.rotas = {{0, 0, 0, 0, 0, 10}, {1, 0, 0, 0, 0, 10}};
    inst.cenarios = {{0, {}}};
    inst.indisponivel = {{0, 0}};
    inst.custosRotas = {10, 10};
    inst.companhiaSpot = 0;
    Solucao inicial;
    inicial.alocacoes = {{0, -1, 1, 0}};
    verificar(avaliarSolucao(inst, inicial), "Exemplo termico inviavel");

    ParametrosSA parametros{2, 1, 0.5, 1, 100, 60};
    parametros.pesosMovimentos = {1, 0, 0, 0, 0, 0, 0, 0};
    EstatisticasSA estatisticas;
    semente(123);
    simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.motivoEncerramento == MotivoEncerramentoSA::TemperaturaFinal &&
              estatisticas.temperaturaFinal == 1 && estatisticas.niveisConcluidos == 1 &&
              estatisticas.candidatosViaveis == 1 && estatisticas.empatesAceitos == 1 &&
              estatisticas.recordes == 0, "Nivel completo ou empate incorreto");

    parametros.maxPropostas = estatisticas.propostas;
    semente(123);
    simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.niveisConcluidos == 1 &&
              estatisticas.motivoEncerramento == MotivoEncerramentoSA::LimitePropostas,
              "Precedencia ou contagem no limite exato incorreta");

    parametros.saMax = 2;
    inst.custosRotas[1] = 9;
    semente(123);
    simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.niveisConcluidos == 0 && estatisticas.candidatosViaveis == 1 &&
              estatisticas.melhoriasAceitas == 1 && estatisticas.empatesAceitos == 0 &&
              estatisticas.recordes == 1 && estatisticas.melhorCusto == 9 &&
              estatisticas.motivoEncerramento == MotivoEncerramentoSA::LimitePropostas,
              "Nivel interrompido ou melhoria incorreta");

    inst.custosRotas[1] = 11;
    parametros.temperaturaInicial = 1e20;
    semente(123);
    simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.piorasAceitas == 1 && estatisticas.melhoriasAceitas == 0 &&
              estatisticas.recordes == 0 && estatisticas.melhorCusto == 10 &&
              estatisticas.custoCorrente == 11, "Piora aceita alterou o melhor custo");

    inst.custosRotas[1] = 10000;
    parametros.temperaturaInicial = 2;
    semente(123);
    simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.candidatosViaveis == 1 && estatisticas.melhoriasAceitas == 0 &&
              estatisticas.empatesAceitos == 0 && estatisticas.piorasAceitas == 0 &&
              estatisticas.custoCorrente == 10, "Piora rejeitada entrou na contagem de aceites");

    inst.indisponivel[0][1] = 1;
    parametros.temperaturaInicial = 1e20;
    parametros.permitirInviavel = true;
    semente(123);
    auto melhor = simulatedAnnealing(inst, inicial, parametros, &estatisticas);
    verificar(estatisticas.inviaveisAceitos == 1 && estatisticas.candidatosViaveis == 0 &&
              melhor.viavel && !melhor.permitirInviavel && melhor.funcaoObjetivo == 10,
              "SA nao separou a cadeia penalizada da incumbente viavel");
}
}

int main() {
    try {
        const Instancia inst = lerInstancia("instancia/alliances.json");
        const Solucao inicial = criarSolucaoSpot(inst);
        const ParametrosSA parametros{100000, 1, 0.9, 100, 120, 60};
        testarValidacao(inst, inicial, parametros);
        testarBusca(inst, inicial, parametros);
        testarLimites(inst, inicial, parametros);
        testarNiveisEContadores();
        std::cout << verificacoes << " verificacoes de SA passaram\n";
    } catch (const std::exception &erro) {
        std::cerr << erro.what() << '\n';
        return 1;
    }

    return 0;
}
