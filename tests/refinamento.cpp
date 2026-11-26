#include "refinamento/refinamento.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
int verificacoes = 0;
void verificar(bool condicao, const char *mensagem) {
    ++verificacoes;
    if (!condicao) throw std::runtime_error(mensagem);
}
Instancia exemplo() {
    Instancia i;
    i.meta.alpha = 0.05;
    i.portos = {{0, "A", "A", 120, {}}, {1, "B", "B", 120, {}}, {2, "SPOT", "SPOT", 120, {}}};
    for (int t = 0; t < 3; ++t) i.sites.push_back({t, "SITE", 20, {}, {}});
    i.companhias = {{0, "M0", 10, 100, 3}, {1, "M1", 20, 100, 3}, {2, "SPOT", 0, 60, 3}};
    for (int f = 0; f < 3; ++f) {
        i.fluxos.push_back({f, f, (f + 1) % 3, 20, {3*f, 3*f+1, 3*f+2}});
        i.rotas.push_back({3*f, f, 0, 0, 1, 10 + 5*f});
        i.rotas.push_back({3*f+1, f, 1, 0, 1, 20 + 5*f});
        i.rotas.push_back({3*f+2, f, 2, 2, 2, 100});
        i.custosRotas.insert(i.custosRotas.end(), {20 + 5*f, 30 + 5*f, 104});
    }
    i.cenarios = {{0, {}}, {1, {3}}, {2, {7}}};
    i.indisponivel.assign(3, std::vector<unsigned char>(9, 0));
    i.indisponivel[1][3] = i.indisponivel[2][7] = 1;
    i.custosRodoviarios.assign(3, {2, 3, 1});
    i.companhiaSpot = 2; i.portoSpot = 2; i.demandaTotal = 60;

    return i;
}
Solucao dividida(const Instancia &i) {
    Solucao s;
    for (int f = 0; f < 3; ++f) s.alocacoes.push_back({3*f, 3*f+1, 10, 10});
    verificar(avaliarSolucao(i, s), "Exemplo dividido invalido");

    return s;
}
double exaustivoQuantidade(const Instancia &i, const Solucao &base, int f) {
    const auto a = base.alocacoes[f];
    double melhor = base.funcaoObjetivo;
    for (int q = 1; q < i.fluxos[f].demanda; ++q) {
        auto s = base;
        s.alocacoes[f] = {a.rota1, a.rota2, q, static_cast<int>(i.fluxos[f].demanda) - q};
        if (avaliarSolucao(i, s)) melhor = std::min(melhor, s.funcaoObjetivo);
    }
    for (int r : {a.rota1, a.rota2}) {
        auto s = base;
        if (alterarAlocacao(i, s, f, {r, -1, static_cast<int>(i.fluxos[f].demanda), 0})) melhor = std::min(melhor, s.funcaoObjetivo);
    }

    return melhor;
}
void testarQuantidades() {
    auto i = exemplo();
    auto s = dividida(i);
    double esperado = exaustivoQuantidade(i, s, 0);
    auto r = refinarQuantidade(i, s, 0);
    verificar(r.melhorou && r.concluido && r.candidatosTestados <= 3 && s.funcaoObjetivo == esperado, "Quantidade nao atingiu melhor custo");
    verificar(s.alocacoes[0].rota2 == -1, "Extremo com uma rota nao foi explorado");
    i = exemplo(); i.companhias[0].capacidade = 45; s = dividida(i);
    r = refinarQuantidade(i, s, 0);
    verificar(r.melhorou && s.alocacoes[0].quantidade1 == 15, "Capacidade apos incidente nao limitou a quantidade");
    i.meta.alpha = 0.2; i.companhias[0].capacidade = 100; s = dividida(i);
    auto anterior = s;
    r = refinarQuantidade(i, s, 0);
    verificar(!r.melhorou && s.funcaoObjetivo == anterior.funcaoObjetivo && s.alocacoes[0].quantidade1 == 10, "Minimo em incidente foi ignorado");
    i = exemplo(); i.companhias[0].capacidade = 45; s = dividida(i);
    s.reutilizacoes = {{1, 4, 8}, {7, 1, 9}};
    verificar(avaliarSolucao(i, s), "Exemplo com reutilizacao invalido");
    esperado = exaustivoQuantidade(i, s, 0);
    r = refinarQuantidade(i, s, 0);
    verificar(s.funcaoObjetivo == esperado && s.alocacoes[0].quantidade1 == 11 && s.reutilizacoes[1].quantidade == 9, "Limites agregados de y ignorados");
    auto spot = criarSolucaoSpot(i);
    verificar(refinarQuantidade(i, spot, 0).candidatosTestados == 0, "Par inexistente foi refinado");
    // Compara o intervalo analitico com todos os inteiros, variando custos,
    // manutencao, minimos e reutilizacoes. Portos iguais exercitam coeficientes 2.
    std::mt19937 rng(123);
    for (int caso = 0; caso < 200; ++caso) {
        i = exemplo();
        i.meta.alpha = (rng() % 21) / 100.0;
        i.companhias[0].capacidade = 40 + rng() % 21;
        i.companhias[1].capacidade = 40 + rng() % 21;
        for (auto &rota : i.rotas) if (rota.companhia != i.companhiaSpot) {
            rota.portoOrigem = rng() % 3; rota.portoDestino = rng() % 3;
            rota.custoMaritimo = rng() % 101;
            i.custosRotas[rota.id] = rota.custoMaritimo + 2*i.custosRodoviarios[0][rota.portoOrigem] + 2*i.custosRodoviarios[0][rota.portoDestino];
        }
        s = dividida(i);
        for (int f = 0; f < 3; ++f) for (int m = 0; m < 2; ++m) {
            int q = rng() % 11;
            if (q > 0) s.reutilizacoes.push_back({3*f+m, 3*((f+1)%3)+m, q});
        }
        // Define manutencao a partir do maior uso da solucao inicial.
        std::vector<Inteiro> maximo(3, 0);
        for (const auto &cenario : i.cenarios) {
            std::vector<Inteiro> carga(3, 0);
            for (const auto &a : s.alocacoes) {
                int q1 = a.quantidade1, q2 = a.quantidade2;
                if (i.indisponivel[cenario.id][a.rota1]) { q1 = 0; q2 = 20; }
                if (i.indisponivel[cenario.id][a.rota2]) { q1 = 20; q2 = 0; }
                for (const auto &v : {std::make_pair(a.rota1, q1), std::make_pair(a.rota2, q2)}) {
                    const auto &rota = i.rotas[v.first];
                    carga[rota.portoOrigem] += v.second; carga[rota.portoDestino] += v.second;
                }
            }
            for (int p = 0; p < 3; ++p) maximo[p] = std::max(maximo[p], carga[p]);
        }
        for (int p = 0; p < 3; ++p) i.portos[p].capacidade = maximo[p] + rng() % 6;
        verificar(avaliarSolucao(i, s), "Caso diferencial invalido");
        int f = rng() % 3;
        if (caso % 2) { auto &a = s.alocacoes[f]; std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
        esperado = exaustivoQuantidade(i, s, f);
        refinarQuantidade(i, s, f);
        verificar(s.funcaoObjetivo == esperado && s.viavel, "Quantidade divergiu da enumeracao exaustiva");
    }
}
void testarGrupos() {
    auto i = exemplo(); i.meta.alpha = 0.25;
    auto s = criarSolucaoSpot(i);
    auto candidato = s;
    verificar(!alterarAlocacao(i, candidato, 0, {0, -1, 20, 0}), "Ativacao isolada deveria falhar");
    std::vector<AlternativasFluxo> grupo{{0, {{0, -1, 20, 0}}}, {2, {{6, -1, 20, 0}}}};
    auto r = enumerarGrupo(i, s, grupo);
    verificar(r.melhorou && r.concluido && r.candidatosTestados == 4 && s.alocacoes[0].rota1 == 0 && s.alocacoes[2].rota1 == 6, "Grupo nao ativou companhia conjuntamente");
    auto melhor = s;
    s = criarSolucaoSpot(i);
    grupo.push_back({1, {{4, -1, 20, 0}}});
    r = enumerarGrupo(i, s, grupo);
    verificar(r.concluido && r.candidatosTestados == 8 && s.funcaoObjetivo == melhor.funcaoObjetivo, "Grupo de tres fluxos incorreto");
    s = criarSolucaoSpot(i);
    r = refinarGrupo(i, s, {0, 2});
    verificar(r.melhorou && s.viavel && s.funcaoObjetivo == melhor.funcaoObjetivo, "Geracao automatica de alternativas falhou");
    s = criarSolucaoSpot(i); auto original = s;
    ParametrosRefinamento p; p.maximoCombinacoes = 1;
    r = enumerarGrupo(i, s, grupo, p);
    verificar(!r.concluido && !r.melhorou && r.candidatosTestados == 1 && s.funcaoObjetivo == original.funcaoObjetivo, "Orcamento de combinacoes ignorado");
    p.maximoCombinacoes = 8;
    r = enumerarGrupo(i, s, grupo, p);
    verificar(r.concluido && r.candidatosTestados == 8, "Limite exato marcou busca incompleta");
    s = original; p.maximoCombinacoes = 7;
    r = enumerarGrupo(i, s, grupo, p);
    verificar(!r.concluido && r.melhorou && s.funcaoObjetivo == melhor.funcaoObjetivo, "Interrupcao perdeu a melhor solucao encontrada");
    p.limiteSegundos = 1e-12; s = original;
    r = refinarGrupo(i, s, {0, 2}, p);
    verificar(!r.concluido && !r.melhorou && s.funcaoObjetivo == original.funcaoObjetivo, "Limite de tempo ignorado");
    bool rejeitou = false;
    try { refinarGrupo(i, s, {0, 0}); } catch (const std::invalid_argument &) { rejeitou = true; }
    verificar(rejeitou && s.funcaoObjetivo == original.funcaoObjetivo, "Grupo repetido foi aceito");
    rejeitou = false;
    try { enumerarGrupo(i, s, {{0, {{3, -1, 20, 0}}}, {2, {}}}); } catch (const std::invalid_argument &) { rejeitou = true; }
    verificar(rejeitou, "Rota de outro fluxo foi aceita");
    rejeitou = false; candidato = s; candidato.alocacoes[0].quantidade1 = 0;
    try { refinarQuantidade(i, candidato, 0); } catch (const std::invalid_argument &) { rejeitou = true; }
    verificar(rejeitou && candidato.alocacoes[0].quantidade1 == 0, "Entrada invalida foi modificada");
    semente(456); int proximo = inteiroAleatorio(0, 1000000);
    semente(456); refinarGrupo(i, s, {0, 2});
    verificar(inteiroAleatorio(0, 1000000) == proximo, "Refinamento consumiu RNG do SA");
}
}

int main() {
    try {
        testarQuantidades(); testarGrupos();
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            auto s = criarSolucaoSpot(inst);
            double custo = s.funcaoObjetivo;
            ParametrosRefinamento p; p.limiteSegundos = 0.2;
            refinarGrupo(inst, s, {0, 1, 2}, p);
            verificar(s.viavel && s.funcaoObjetivo <= custo, "Refinamento piorou instancia oficial");
            auto copia = s;
            verificar(avaliarSolucao(inst, copia) && copia.funcaoObjetivo == s.funcaoObjetivo, "Cache inconsistente na instancia oficial");
        }
        std::cout << verificacoes << " verificacoes de refinamento passaram\n";
    } catch (const std::exception &erro) { std::cerr << erro.what() << '\n'; return 1; }

    return 0;
}
