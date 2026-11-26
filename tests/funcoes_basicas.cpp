#include "definicao_problema/solucao.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int verificacoes = 0;
void verificar(bool condicao, const char *mensagem) {
    ++verificacoes;
    if (!condicao) throw std::runtime_error(mensagem);
}
void preparar(Instancia &i) {
    i.indisponivel.assign(i.cenarios.size(), std::vector<unsigned char>(i.rotas.size(), 0));
    for (const auto &s : i.cenarios) for (int r : s.rotasIndisponiveis) i.indisponivel[s.id][r] = 1;
}
Instancia exemplo() {
    Instancia i;
    i.meta.alpha = 0.05;
    i.portos = {{0, "A", "A", 100, {}}, {1, "B", "B", 100, {}}, {2, "SPOT", "SPOT", 60, {}}};
    i.sites = {{0, "A", 10, {}, {}}, {1, "B", 10, {}, {}}};
    i.companhias = {{0, "M0", 10, 100, 2}, {1, "M1", 20, 100, 2}, {2, "SPOT", 0, 30, 2}};
    i.fluxos = {{0, 0, 1, 10, {0, 1, 2}}, {1, 1, 0, 20, {3, 4, 5}}};
    i.rotas = {{0, 0, 0, 0, 1, 10}, {1, 0, 1, 0, 1, 20}, {2, 0, 2, 2, 2, 100},
               {3, 1, 0, 1, 0, 15}, {4, 1, 1, 1, 0, 25}, {5, 1, 2, 2, 2, 100}};
    i.cenarios = {{0, {}}, {1, {0, 3}}, {2, {1, 4}}};
    i.custosRodoviarios = {{2, 3, 1}, {2, 3, 1}};
    i.custosRotas = {20, 30, 104, 25, 35, 104};
    i.companhiaSpot = 2; i.portoSpot = 2; i.demandaTotal = 30;
    preparar(i);

    return i;
}
Solucao dividida(const Instancia &i) {
    Solucao s;
    s.alocacoes = {{0, 1, 6, 4}, {3, 4, 12, 8}};
    s.reutilizacoes = {{0, 3, 5}, {3, 0, 4}, {1, 4, 2}};
    verificar(avaliarSolucao(i, s), "Exemplo dividido inviavel");

    return s;
}
void testarAvaliacao() {
    auto i = exemplo();
    auto s = dividida(i);
    verificar(s.custosCenarios == std::vector<double>({822, 1028, 674}), "Custos por cenario incorretos");
    verificar(s.funcaoObjetivo == 1028 && s.piorCenario == 1, "Objetivo robusto incorreto");
    auto outroPorto = i; outroPorto.rotas[3].portoOrigem = 0; outroPorto.custosRotas[3] = 23;
    verificar(avaliarSolucao(outroPorto, s) && s.custosCenarios == std::vector<double>({803, 1028, 639}), "Reutilizacao entre portos distintos incorreta");
    auto spot = criarSolucaoSpot(i);
    verificar(spot.funcaoObjetivo == 3120 && spot.viavel, "SPOT incorreta");
    auto ruim = i;
    ruim.companhias[2].capacidade = 1000;
    verificar(avaliarSolucao(ruim, spot), "SPOT nao foi isenta do minimo comercial");
    ruim = i;
    ruim.portos[2].capacidade = 59;
    verificar(!avaliarSolucao(ruim, spot), "SPOT deve contar duas manutencoes");
    verificar(!spot.viavel && std::isinf(spot.funcaoObjetivo), "Cache invalido preservou custo antigo");
    ruim = i; ruim.companhias[1].capacidade = 25;
    verificar(!avaliarSolucao(ruim, s) && s.piorCenario == 1, "Capacidade apos redirecionamento ignorada");
    ruim = i; ruim.meta.alpha = 0.1; ruim.cenarios.push_back({3, {4}}); preparar(ruim);
    verificar(!avaliarSolucao(ruim, s) && s.piorCenario == 3, "Minimo comercial em incidente ignorado");
    ruim = i; ruim.companhias[0].maximoSites = 1;
    verificar(!avaliarSolucao(ruim, s), "Limite de sites ignorado");
    ruim = i; ruim.sites[1].capacidadeReutilizacao = 6;
    verificar(!avaliarSolucao(ruim, s), "Capacidade de reutilizacao ignorada");
    ruim = i; ruim.cenarios.push_back({3, {0, 1}}); preparar(ruim);
    verificar(!avaliarSolucao(ruim, s), "Falha conjunta ignorada");
    s = dividida(i); s.reutilizacoes.push_back(s.reutilizacoes[0]);
    verificar(!avaliarSolucao(i, s), "Par de reutilizacao duplicado aceito");
    s = dividida(i); s.reutilizacoes = {{0, 3, 7}};
    verificar(!avaliarSolucao(i, s), "Reutilizacao acima do volume aceita");
    s = dividida(i); s.reutilizacoes = {{0, 4, 1}};
    verificar(!avaliarSolucao(i, s), "Reutilizacao entre companhias aceita");
    s = dividida(i); s.reutilizacoes = {{0, 0, 1}};
    verificar(!avaliarSolucao(i, s), "Reutilizacao entre sites distintos aceita");
    s = dividida(i); s.alocacoes[0].quantidade1 = -1;
    verificar(!avaliarSolucao(i, s), "Quantidade negativa aceita");
    s = dividida(i); s.alocacoes[0].rota2 = 0;
    verificar(!avaliarSolucao(i, s), "Rotas duplicadas aceitas");
    s = dividida(i); s.alocacoes[0].rota2 = 99;
    verificar(!avaliarSolucao(i, s), "Indice invalido aceito");
    bool bloqueou = false;
    try { salvarSolucao(i, s, "bin/nao-deve-ser-criado.json"); }
    catch (const std::runtime_error &) { bloqueou = true; }
    verificar(bloqueou, "Exportacao de solucao inviavel permitida");
}
void testarMovimentos() {
    auto i = exemplo();
    auto s = dividida(i);
    std::string motivo;
    verificar(parRobusto(i, 0, 1) && !parRobusto(i, 0) && !parRobusto(i, 0, 3) && parRobusto(i, 2), "Pares robustos incorretos");
    verificar(transferirQuantidade(i, s, 0, 1), "Transferencia valida rejeitada");
    verificar(s.alocacoes[0].quantidade1 == 7 && s.alocacoes[0].quantidade2 == 3 && s.viavel, "Transferencia nao aplicada");
    auto anterior = s;
    verificar(!transferirQuantidade(i, s, 0, 99, &motivo) && !motivo.empty(), "Transferencia invalida aceita");
    verificar(s.funcaoObjetivo == anterior.funcaoObjetivo && s.alocacoes[0].quantidade1 == 7 && s.viavel, "Movimento rejeitado alterou solucao");
    verificar(!substituirRota(i, s, 0, 1, 3), "Rota de outro fluxo aceita");
    verificar(substituirRota(i, s, 0, 1, 2), "Substituicao valida rejeitada");
    verificar(s.reutilizacoes.size() == 1 && s.reutilizacoes[0].rotaOrigem == 1, "Reutilizacoes obsoletas mantidas");
    verificar(!transferirQuantidade(i, s, 0, -7), "Eliminacao da unica rota robusta aceita");
    verificar(alterarAlocacao(i, s, 0, {1, 2, 3, 7}), "Reordenacao valida rejeitada");
    verificar(transferirQuantidade(i, s, 0, -3), "Eliminacao da primeira rota rejeitada");
    verificar(s.alocacoes[0].rota1 == 2 && s.alocacoes[0].rota2 == -1 && s.alocacoes[0].quantidade1 == 10, "Normalizacao incorreta");
    s = dividida(i); s.reutilizacoes = {{1, 4, 2}};
    verificar(definirReutilizacao(i, s, 1, 4, 3), "Aumento de reutilizacao rejeitado");
    verificar(!definirReutilizacao(i, s, 1, 4, 11), "Reutilizacao excessiva aceita");
    verificar(definirReutilizacao(i, s, 1, 4, 0) && s.reutilizacoes.empty(), "Remocao de reutilizacao falhou");
    verificar(!alterarAlocacoes(i, s, {{0, {2, -1, 10, 0}}, {0, {2, -1, 10, 0}}}), "Fluxo repetido no bloco aceito");
    verificar(!alterarAlocacao(i, s, -1, {}), "Fluxo negativo aceito");
    i = exemplo(); i.meta.alpha = 0.1; s = criarSolucaoSpot(i);
    verificar(!alterarAlocacao(i, s, 0, {0, 2, 6, 4}), "Ativacao abaixo do minimo aceita");
    verificar(alterarAlocacoes(i, s, {{0, {0, 2, 6, 4}}, {1, {3, 5, 6, 14}}}), "Ativacao conjunta rejeitada");
    i = exemplo(); s = dividida(i);
    verificar(transferirQuantidade(i, s, 0, -4), "Reducao de volume rejeitada");
    verificar(s.reutilizacoes[1].quantidade == 2, "Reutilizacao nao foi limitada ao novo volume");
    i.sites[0].capacidadeReutilizacao = 100;
    auto a = criarSolucaoSpot(i), b = a;
    semente(123); bool mudouA = movimentoAleatorio(i, a, 200);
    semente(123); bool mudouB = movimentoAleatorio(i, b, 200);
    verificar(mudouA && mudouB && a.funcaoObjetivo == b.funcaoObjetivo && a.alocacoes[0].rota1 == b.alocacoes[0].rota1 && a.alocacoes[1].quantidade1 == b.alocacoes[1].quantidade1, "Movimentos nao reprodutiveis");
    std::vector<int> v1{1, 2, 3, 4, 5}, v2 = v1;
    semente(45); embaralhaVetor(v1); semente(45); embaralhaVetor(v2);
    verificar(v1 == v2, "Embaralhamento nao respeita a semente compartilhada");
}
}

int main(int argc, char **argv) {
    try {
        testarAvaliacao(); testarMovimentos();
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            auto sol = criarSolucaoSpot(inst);
            verificar(sol.funcaoObjetivo == 515357081 && sol.viavel, "Baseline oficial incorreto");
            verificar(std::all_of(sol.custosCenarios.begin(), sol.custosCenarios.end(), [](double custo) { return custo == 515357081; }), "Cenarios SPOT inconsistentes");
            semente(7);
            for (int k = 0; k < 5; ++k) {
                movimentoAleatorio(inst, sol, 100);
                auto copia = sol;
                verificar(avaliarSolucao(inst, copia) && copia.funcaoObjetivo == sol.funcaoObjetivo, "Cache inconsistente apos movimento oficial");
            }
        }
        if (argc == 2) salvarSolucao(exemplo(), dividida(exemplo()), argv[1]);
        std::cout << verificacoes << " verificacoes passaram\n";
    } catch (const std::exception &erro) { std::cerr << erro.what() << '\n'; return 1; }

    return 0;
}
