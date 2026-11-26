#include "meta_heuristicas/simulated_annealing.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "fluxo.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <numeric>

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
    inst.custosRotas[1] = 10;
    ParametrosTabu tabu; tabu.tamanhoLista = 3; tabu.tamanhoVizinhanca = 50; tabu.maxIteracoes = 4; tabu.maxPropostas = 200; tabu.tempoMax = 60;
    auto cadeia = inicial; EstatisticasTabu e;
    semente(123); buscaTabu(inst, cadeia, tabu, &e);
    verificar(e.bloqueados > 0 && mesmaSolucao(cadeia, inicial), "Memoria tabu nao bloqueou revisitas de mesmo custo");
}

void testarMovimentosIntegrados() {
    Instancia i;
    i.portos = {{0, "A", "A", 100, {}}, {1, "B", "B", 100, {}}, {2, "SPOT", "SPOT", 100, {}}};
    i.sites = {{0, "A", 20, {}, {}}, {1, "B", 20, {}, {}}};
    i.companhias = {{0, "M0", 0, 100, 2}, {1, "M1", 0, 100, 2}, {2, "SPOT", 0, 40, 2}};
    i.fluxos = {{0, 0, 1, 20, {0, 1, 2, 6}}, {1, 1, 0, 20, {3, 4, 5, 7}}};
    i.rotas = {{0,0,0,0,1,10}, {1,0,1,0,1,9}, {2,0,2,2,2,100}, {3,1,0,1,0,10},
               {4,1,1,1,0,9}, {5,1,2,2,2,100}, {6,0,1,0,1,1}, {7,1,1,1,0,1}};
    i.custosRotas = {10, 9, 100, 10, 9, 100, 1, 1};
    i.custosRodoviarios.assign(2, {0, 0, 0});
    i.cenarios = {{0, {}}, {1, {6, 7}}}; i.indisponivel = {{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,1,1}};
    i.companhiaSpot = 2; i.portoSpot = 2; i.demandaTotal = 40;
    auto sol = criarSolucaoSpot(i);
    verificar(trocarCompanhia(i, sol, 2, 0), "Migracao de companhia nao aplicada");
    verificar(trocarCompanhia(i, sol, 0, 1) && sol.alocacoes[0].rota1 == 1 && sol.alocacoes[1].rota1 == 4, "Migracao escolheu rota barata sem cobertura robusta");
    verificar(unificarSite(i, sol, 0, 0), "Unificacao de site falhou");
    auto antes = sol;
    verificar(!migrarFluxos(i, sol, {0, 0}, 0, 1) && mesmaSolucao(sol, antes), "Migracao repetida alterou a solucao");
    auto falta = i; falta.fluxos[1].rotas = {3, 5};
    verificar(!trocarCompanhia(falta, sol, 0, 1) && mesmaSolucao(sol, antes), "Migracao parcial foi aceita sem oferta para todos");
    verificar(!migrarFluxos(i, sol, {0}, -2, 1), "Companhia negativa invalida aceita");
    verificar(redividirFluxo(i, sol, 0, 0, 2, 10) && redividirFluxo(i, sol, 1, 3, 5, 10), "Divisao de fluxo falhou");
    verificar(substituirPar(i, sol, 0, 1) && substituirPar(i, sol, 0, 0), "Substituicao de par falhou");
    semente(13);
    verificar(movimentoReutilizacao(i, sol, 100) && !sol.reutilizacoes.empty(), "Movimento de reutilizacao nao criou pares");
    auto unificada = sol;
    verificar(unificarSite(i, unificada, 0, 1) && unificada.alocacoes[0].rota1 == 1 && unificada.alocacoes[0].rota2 == -1 && unificada.alocacoes[1].rota1 == 4, "Migracao de duas rotas nao consolidou volumes");
    for (std::size_t familia = 0; familia < numeroMovimentosSA; ++familia) {
        ParametrosSA p{1000, 1, 0.9, 1000, 100, 60};
        p.pesosMovimentos.fill(0); p.pesosMovimentos[familia] = 1;
        EstatisticasSA e;
        semente(123);
        auto r = simulatedAnnealing(i, sol, p, &e);
        verificar(e.propostas == 100 && e.propostasPorMovimento[familia] == 100 && e.viaveisPorMovimento[familia] > 0, "Familia nao foi acionada ou nao gerou candidatos");
        verificar(std::accumulate(e.viaveisPorMovimento.begin(), e.viaveisPorMovimento.end(), 0) == e.candidatosViaveis && r.viavel && r.funcaoObjetivo <= sol.funcaoObjetivo, "Contadores ou incumbente invalidos");
        auto reavaliada = r;
        verificar(avaliarSolucao(i, reavaliada) && mesmaSolucao(r, reavaliada), "Cache incorreto apos novos movimentos");
    }
    ParametrosSA p;
    p.pesosMovimentos.fill(0); verificarRejeicao(i, sol, p);
    p = {}; p.pesosMovimentos[0] = -1; verificarRejeicao(i, sol, p);
    p = {}; p.percentualAleatorioGrupos = 101; verificarRejeicao(i, sol, p);
    p = {}; p.maxPropostas = 200; p.saMax = 1000; p.tempoLimite = 60;
    EstatisticasSA e1, e2;
    semente(321); auto r1 = simulatedAnnealing(i, sol, p, &e1); auto estado = gerador;
    semente(321); auto r2 = simulatedAnnealing(i, sol, p, &e2);
    verificar(mesmaSolucao(r1, r2) && gerador == estado && e1.propostasPorMovimento == e2.propostasPorMovimento, "Mistura de movimentos nao reproduziu a semente");
    auto permutada = sol;
    for (auto &a : permutada.alocacoes) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
    std::reverse(permutada.reutilizacoes.begin(), permutada.reutilizacoes.end());
    permutada.funcaoObjetivo = -1;
    verificar(hashTabu(sol) == hashTabu(permutada), "Hash tabu depende de slots, ordem ou caches");
    ParametrosTabu tabu; tabu.maxIteracoes = 30; tabu.maxPropostas = 200; tabu.tamanhoVizinhanca = 10; tabu.tamanhoLista = 3; tabu.tempoMax = 60;
    auto t1 = sol, t2 = sol;
    EstatisticasTabu et1, et2;
    semente(456); double custoTabu = buscaTabu(i, t1, tabu, &et1); estado = gerador;
    semente(456); buscaTabu(i, t2, tabu, &et2);
    verificar(mesmaSolucao(t1, t2) && gerador == estado && et1.propostas == et2.propostas, "Tabu nao reproduziu semente e limites");
    verificar(t1.viavel && custoTabu <= sol.funcaoObjetivo && custoTabu <= et1.custoCorrente && et1.propostas <= 200 && et1.viaveis <= et1.propostas && et1.aceitos <= et1.iteracoes, "Incumbente ou contadores tabu invalidos");
    auto conferida = t1;
    verificar(avaliarSolucao(i, conferida) && mesmaSolucao(t1, conferida), "Tabu retornou caches incorretos");
    auto zero = sol; tabu.tempoMax = 0;
    buscaTabu(i, zero, tabu, &et1);
    verificar(mesmaSolucao(zero, sol) && et1.propostas == 0, "Tabu com tempo zero alterou a entrada");
    verificar(busca_tabu(i, zero, 3, 10, 0) == sol.funcaoObjetivo, "Interface original da tabu incompativel");
    auto inviavel = sol; inviavel.viavel = false; bool rejeitouEntrada = false;
    try { buscaTabu(i, inviavel, tabu); } catch (const std::invalid_argument &) { rejeitouEntrada = true; }
    verificar(rejeitouEntrada && !inviavel.viavel, "Tabu aceitou entrada inviavel");
    for (int caso = 0; caso < 4; ++caso) {
        auto invalida = tabu;
        if (caso == 0) invalida.tamanhoLista = 0;
        if (caso == 1) invalida.tamanhoVizinhanca = 0;
        if (caso == 2) invalida.tempoMax = std::numeric_limits<double>::quiet_NaN();
        if (caso == 3) invalida.maxPropostas = -1;
        bool rejeitou = false;
        try { buscaTabu(i, zero, invalida); } catch (const std::invalid_argument &) { rejeitou = true; }
        verificar(rejeitou && mesmaSolucao(zero, sol), "Tabu aceitou parametro invalido ou alterou entrada");
    }
    ParametrosExecucao fluxo;
    fluxo.construcao.limiteSegundos = 0; fluxo.segundosRefinamento = 0.05; fluxo.sa = p;
    semente(777); estado = gerador;
    auto resultado = executarFluxo(i, fluxo);
    verificar(resultado.saExecutado && resultado.estatisticasSA.propostas == 200 && resultado.solucao.funcaoObjetivo <= resultado.custoAposSA && gerador == estado, "Fluxo completo ou isolamento do RNG incorreto");
    fluxo.executarTabu = true; fluxo.tabu = tabu; fluxo.tabu.tempoMax = 60;
    resultado = executarFluxo(i, fluxo);
    verificar(resultado.tabuExecutado && resultado.custoAposTabu <= resultado.custoAposSA && resultado.solucao.funcaoObjetivo <= resultado.custoAposTabu && gerador == estado, "Tabu nao foi integrada entre SA e refinamento");
}
}

int main() {
    try {
        const Instancia inst = lerInstancia("instancia/alliances.json");
        const Solucao inicial = criarSolucaoSpot(inst);
        ParametrosSA parametros{100000, 1, 0.9, 100, 120, 60};
        parametros.pesosMovimentos = {1, 0, 0, 0, 0, 0, 0, 0};
        testarValidacao(inst, inicial, parametros);
        testarBusca(inst, inicial, parametros);
        testarLimites(inst, inicial, parametros);
        testarNiveisEContadores();
        testarMovimentosIntegrados();
        std::cout << verificacoes << " verificacoes de SA passaram\n";
    } catch (const std::exception &erro) {
        std::cerr << erro.what() << '\n';
        return 1;
    }

    return 0;
}
