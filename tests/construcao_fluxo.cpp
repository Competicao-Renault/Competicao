#include "fluxo.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
int verificacoes = 0;
void verificar(bool condicao, const char *mensagem) {
    ++verificacoes;
    if (!condicao) throw std::runtime_error(mensagem);
}
Instancia exemplo() {
    Instancia i;
    i.meta.alpha = 0.5;
    i.portos = {{0, "A", "A", 40, {}}, {1, "B", "B", 40, {}}, {2, "SPOT", "SPOT", 40, {}}};
    i.sites = {{0, "A", 10, {}, {}}, {1, "B", 10, {}, {}}};
    i.companhias = {{0, "M0", 0, 10, 2}, {1, "M1", 0, 10, 2}, {2, "SPOT", 0, 20, 2}};
    i.fluxos = {{0, 0, 1, 10, {0, 1}}, {1, 1, 0, 10, {2, 3}}};
    i.rotas = {{0, 0, 0, 0, 1, 10}, {1, 0, 2, 2, 2, 100}, {2, 1, 1, 1, 0, 20}, {3, 1, 2, 2, 2, 100}};
    i.custosRotas = {10, 100, 20, 100};
    i.custosRodoviarios.assign(2, {0, 0, 0});
    i.cenarios = {{0, {}}, {1, {0}}, {2, {2}}};
    i.indisponivel = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}};
    i.companhiaSpot = 2; i.portoSpot = 2; i.demandaTotal = 20;

    return i;
}
std::vector<int> assinatura(const Solucao &sol) {
    std::vector<int> valores;
    for (const auto &a : sol.alocacoes) valores.insert(valores.end(), {a.rota1, a.rota2, a.quantidade1, a.quantidade2});

    return valores;
}
void testarConstrucao() {
    const auto i = exemplo();
    const auto spot = criarSolucaoSpot(i);
    ParametrosConstrucao p; p.maximoFluxosBloco = 1; p.maximoBlocos = 1; p.limiteSegundos = 5;
    EstatisticasConstrucao dados;
    auto s = construirSolucao(i, p, 1, &dados);
    verificar(s.viavel && s.funcaoObjetivo == spot.funcaoObjetivo && dados.blocosAceitos == 1, "Construtiva rejeitou empate util");
    verificar(std::accumulate(s.custosCenarios.begin(), s.custosCenarios.end(), 0.0) < 6000, "Desempate nao melhorou os cenarios");
    p.maximoBlocos = 12;
    auto a = construirSolucao(i, p, 3), b = construirSolucao(i, p, 3);
    verificar(a.funcaoObjetivo < spot.funcaoObjetivo && a.viavel, "Construtiva nao superou plateau");
    verificar(assinatura(a) == assinatura(b) && a.custosCenarios == b.custosCenarios, "Semente nao reproduziu construcao");
    auto copia = a;
    verificar(avaliarSolucao(i, copia) && copia.funcaoObjetivo == a.funcaoObjetivo, "Caches da construtiva inconsistentes");
    p.limiteSegundos = 0;
    s = construirSolucao(i, p, 5, &dados);
    verificar(s.funcaoObjetivo == 2000 && dados.blocos == 0, "Orcamento zero nao retornou SPOT");
    p.limiteSegundos = 5;
    semente(42); int esperado = inteiroAleatorio(0, 1000000);
    semente(42); construirSolucao(i, p, 42);
    verificar(inteiroAleatorio(0, 1000000) == esperado, "Construtiva consumiu RNG global");
    for (int aleatorio : {0, 30, 100}) {
        std::mt19937 rng(7);
        auto grupo = selecionarGrupo(i, a, rng, 2, aleatorio);
        std::sort(grupo.begin(), grupo.end());
        verificar(grupo == std::vector<int>({0, 1}), "Selecao retornou fluxos duplicados");
    }
    bool rejeitou = false;
    try { p.percentualAleatorio = 101; construirSolucao(i, p); } catch (const std::invalid_argument &) { rejeitou = true; }
    verificar(rejeitou, "Parametro invalido aceito");
}
void testarFluxo() {
    const auto inst = exemplo();
    ParametrosExecucao p; p.executarSA = false; p.construcao.limiteSegundos = 0; p.segundosRefinamento = 0; p.semente = 9;
    auto semSA = executarFluxo(inst, p);
    verificar(!semSA.saExecutado && semSA.solucao.viavel && semSA.solucao.funcaoObjetivo == 2000, "Fluxo sem SA incorreto");
    auto melhor = criarSolucaoSpot(inst);
    verificar(alterarAlocacoes(inst, melhor, {{0, {0, 1, 9, 1}}, {1, {2, 3, 9, 1}}}), "Exemplo para callback invalido");
    int chamadas = 0;
    auto comSA = executarFluxo(inst, p, [&](const Instancia &, Solucao &s, unsigned seed) {
        verificar(seed == p.semente && s.viavel, "Contrato do callback incorreto");
        ++chamadas; s = melhor;
    });
    verificar(comSA.saExecutado && chamadas == 1 && comSA.custoAposSA == melhor.funcaoObjetivo && comSA.solucao.viavel, "SA nao foi integrado no ponto correto");
    p.construcao.limiteSegundos = 5; p.construcao.maximoBlocos = 12; p.construcao.maximoFluxosBloco = 1;
    auto piora = executarFluxo(inst, p, [](const Instancia &i, Solucao &s, unsigned) { s = criarSolucaoSpot(i); });
    verificar(piora.saExecutado && piora.custoAposSA == piora.custoConstrucao && piora.solucao.funcaoObjetivo < 2000, "SA pior descartou incumbente");
    bool rejeitou = false;
    try { executarFluxo(inst, p, [](const Instancia &, Solucao &s, unsigned) { s.alocacoes[0].quantidade1 = 0; }); }
    catch (const std::runtime_error &) { rejeitou = true; }
    verificar(rejeitou, "Callback inviavel foi aceito");
    p.construcao.limiteSegundos = 0; p.segundosRefinamento = 0.1;
    auto refinado = executarFluxo(inst, p);
    verificar(!refinado.saExecutado && refinado.solucao.funcaoObjetivo < refinado.custoConstrucao, "Refinamento final nao executou");
}
}

int main() {
    try {
        testarConstrucao(); testarFluxo();
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            ParametrosExecucao p; p.executarSA = false; p.construcao.limiteSegundos = 0.15; p.segundosRefinamento = 0.1;
            auto r = executarFluxo(inst, p);
            auto copia = r.solucao;
            verificar(avaliarSolucao(inst, copia) && copia.funcaoObjetivo == r.solucao.funcaoObjetivo, "Fluxo oficial retornou cache incorreto");
            verificar(r.solucao.funcaoObjetivo <= r.custoConstrucao && r.custoConstrucao <= 515357081 && !r.saExecutado, "Garantia de custo ou estado do SA incorreto");
        }
        std::cout << verificacoes << " verificacoes de construcao e fluxo passaram\n";
    } catch (const std::exception &erro) { std::cerr << erro.what() << '\n'; return 1; }

    return 0;
}
