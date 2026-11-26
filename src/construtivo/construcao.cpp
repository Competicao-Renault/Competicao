#include "construtivo/construcao.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <tuple>

namespace {
using Chave = std::tuple<double, long double, Inteiro>;
Chave chave(const Instancia &inst, const Solucao &sol) {
    Inteiro spot = 0;
    for (const auto &a : sol.alocacoes) {
        if (inst.rotas[a.rota1].companhia == inst.companhiaSpot) spot += a.quantidade1;
        if (a.rota2 >= 0 && inst.rotas[a.rota2].companhia == inst.companhiaSpot) spot += a.quantidade2;
    }

    return {sol.funcaoObjetivo, std::accumulate(sol.custosCenarios.begin(), sol.custosCenarios.end(), 0.0L), spot};
}
int sortear(std::mt19937 &rng, int limite) { return std::uniform_int_distribution<int>(0, limite - 1)(rng); }
Inteiro cargaFluxo(const Instancia &inst, const Solucao &sol, int f, int s, int companhia) {
    const auto &a = sol.alocacoes[f];
    int q1 = a.quantidade1, q2 = a.quantidade2;
    if (inst.indisponivel[s][a.rota1]) { q1 = 0; q2 = static_cast<int>(inst.fluxos[f].demanda); }
    else if (a.rota2 >= 0 && inst.indisponivel[s][a.rota2]) { q1 = static_cast<int>(inst.fluxos[f].demanda); q2 = 0; }
    Inteiro carga = 0;
    if (inst.rotas[a.rota1].companhia == companhia) carga += q1;
    if (a.rota2 >= 0 && inst.rotas[a.rota2].companhia == companhia) carga += q2;

    return carga;
}
std::vector<AlocacaoFluxo> alternativas(const Instancia &inst, const Solucao &base, int f, const std::vector<int> &grupo,
                                     const std::vector<std::vector<Inteiro>> &cargas, int maximo, const Cronometro &tempo, double limite) {
    const auto &fluxo = inst.fluxos[f];
    int n = static_cast<int>(fluxo.demanda);
    std::vector<AlocacaoFluxo> lista;
    auto incluir = [&](AlocacaoFluxo a) {
        if (a.quantidade2 == 0) a.rota2 = -1;
        if (a.rota2 >= 0 && a.rota1 > a.rota2) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
        for (const auto &b : lista) if (std::tie(a.rota1, a.rota2, a.quantidade1, a.quantidade2) == std::tie(b.rota1, b.rota2, b.quantidade1, b.quantidade2)) return;
        lista.push_back(a);
    };
    incluir(base.alocacoes[f]);
    for (int r : fluxo.rotas) if (inst.rotas[r].companhia == inst.companhiaSpot) incluir({r, -1, n, 0});
    const std::size_t reservadas = lista.size();
    std::vector<std::pair<std::pair<double, double>, AlocacaoFluxo>> opcoes;
    auto adicionar = [&](AlocacaoFluxo a) {
        double pior = 0, soma = 0;
        for (const auto &s : inst.cenarios) {
            double custo = static_cast<double>(inst.custosRotas[a.rota1]) * a.quantidade1;
            if (a.rota2 >= 0) {
                custo += static_cast<double>(inst.custosRotas[a.rota2]) * a.quantidade2;
                if (inst.indisponivel[s.id][a.rota1]) custo = static_cast<double>(inst.custosRotas[a.rota2]) * n;
                else if (inst.indisponivel[s.id][a.rota2]) custo = static_cast<double>(inst.custosRotas[a.rota1]) * n;
            }
            pior = std::max(pior, custo); soma += custo;
        }
        opcoes.push_back({{pior, soma}, a});
    };
    for (std::size_t i = 0; i < fluxo.rotas.size(); ++i) {
        if (tempo.segundosDecorridos() >= limite) break;
        int r1 = fluxo.rotas[i];
        if (parRobusto(inst, r1)) adicionar({r1, -1, n, 0});
        for (std::size_t j = i + 1; j < fluxo.rotas.size(); ++j) {
            int r2 = fluxo.rotas[j];
            if (n < 2 || !parRobusto(inst, r1, r2)) continue;
            std::vector<Inteiro> quantidades{1, n - 1, n / 4, n / 2, 3 * Inteiro(n) / 4};
            for (int posicao : {0, 1}) {
                int m = inst.rotas[r1].companhia;
                if (posicao == 1) m = inst.rotas[r2].companhia;
                Inteiro minimo = 0, necessidade = 0, folga = inst.companhias[m].capacidade, demandaGrupo = 0;
                if (m != inst.companhiaSpot) minimo = static_cast<Inteiro>(std::ceil(static_cast<long double>(inst.meta.alpha) * inst.companhias[m].capacidade - 1e-9L));
                for (int g : grupo) {
                    bool atende = false;
                    for (int r : inst.fluxos[g].rotas) if (inst.rotas[r].companhia == m) atende = true;
                    if (atende) demandaGrupo += inst.fluxos[g].demanda;
                }
                for (const auto &s : inst.cenarios) if (!inst.indisponivel[s.id][r1] && !inst.indisponivel[s.id][r2]) {
                    Inteiro outros = cargas[s.id][m] - cargaFluxo(inst, base, f, s.id, m);
                    necessidade = std::max(necessidade, minimo - outros);
                    folga = std::min(folga, inst.companhias[m].capacidade - outros);
                }
                // Pontos comerciais exatos e parcelas proporcionais ao bloco.
                std::vector<Inteiro> limites{necessidade, folga};
                if (demandaGrupo > 0) {
                    limites.push_back(static_cast<Inteiro>(std::ceil(static_cast<long double>(necessidade) * n / demandaGrupo)));
                    limites.push_back(static_cast<Inteiro>(static_cast<long double>(folga) * n / demandaGrupo));
                }
                for (Inteiro q : limites) {
                    if (posicao == 1) q = n - q;
                    if (q > 0 && q < n) for (int ajuste : {-1, 0, 1}) quantidades.push_back(q + ajuste);
                }
            }
            std::sort(quantidades.begin(), quantidades.end());
            quantidades.erase(std::unique(quantidades.begin(), quantidades.end()), quantidades.end());
            for (Inteiro q : quantidades) if (q > 0 && q < n) adicionar({r1, r2, static_cast<int>(q), static_cast<int>(n - q)});
        }
    }
    std::stable_sort(opcoes.begin(), opcoes.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &opcao : opcoes) {
        if (lista.size() >= static_cast<std::size_t>(maximo)) break;
        // Limita duas divisoes do mesmo par, preservando diversidade de companhias.
        int repeticoes = 0;
        for (std::size_t k = reservadas; k < lista.size(); ++k) if (lista[k].rota1 == opcao.second.rota1 && lista[k].rota2 == opcao.second.rota2) ++repeticoes;
        if (repeticoes < 2) incluir(opcao.second);
    }

    return lista;
}
}

std::vector<int> selecionarGrupo(const Instancia &inst, const Solucao &sol, std::mt19937 &rng, int tamanho, int percentualAleatorio) {
    if (tamanho <= 0 || tamanho > static_cast<int>(inst.fluxos.size()) || percentualAleatorio < 0 || percentualAleatorio > 100 || !sol.viavel || sol.piorCenario < 0 || sol.piorCenario >= static_cast<int>(inst.cenarios.size()) || sol.alocacoes.size() != inst.fluxos.size()) throw std::invalid_argument("Parametros de selecao invalidos");
    std::vector<int> ids(inst.fluxos.size());
    std::iota(ids.begin(), ids.end(), 0);
    if (sortear(rng, 100) >= percentualAleatorio) {
        int companhia = -1;
        std::vector<Inteiro> ofertas(inst.fluxos.size(), -1);
        if (sortear(rng, 2) == 0) {
            std::vector<int> elegiveis;
            std::vector<std::vector<Inteiro>> custos(inst.companhias.size(), ofertas);
            for (const auto &r : inst.rotas) {
                auto &c = custos[r.companhia][r.fluxo];
                if (c < 0 || inst.custosRotas[r.id] < c) c = inst.custosRotas[r.id];
            }
            for (const auto &m : inst.companhias) if (m.id != inst.companhiaSpot && std::count_if(custos[m.id].begin(), custos[m.id].end(), [](Inteiro c) { return c >= 0; }) >= tamanho) elegiveis.push_back(m.id);
            if (!elegiveis.empty()) { companhia = elegiveis[sortear(rng, static_cast<int>(elegiveis.size()))]; ofertas = std::move(custos[companhia]); }
        }
        std::vector<double> prioridade(inst.fluxos.size(), 0);
        const auto &falha = inst.indisponivel[sol.piorCenario];
        for (const auto &f : inst.fluxos) {
            const auto &a = sol.alocacoes[f.id];
            if (falha[a.rota1]) prioridade[f.id] = static_cast<double>(inst.custosRotas[a.rota2]) * f.demanda;
            else if (a.rota2 >= 0 && falha[a.rota2]) prioridade[f.id] = static_cast<double>(inst.custosRotas[a.rota1]) * f.demanda;
            else {
                prioridade[f.id] = static_cast<double>(inst.custosRotas[a.rota1]) * a.quantidade1;
                if (a.rota2 >= 0) prioridade[f.id] += static_cast<double>(inst.custosRotas[a.rota2]) * a.quantidade2;
            }
            if (companhia >= 0 && ofertas[f.id] >= 0) prioridade[f.id] -= static_cast<double>(ofertas[f.id]) * f.demanda;
        }
        if (companhia >= 0) ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int f) { return ofertas[f] < 0; }), ids.end());
        std::stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return prioridade[a] > prioridade[b]; });
        if (ids.size() > static_cast<std::size_t>(std::max(20, tamanho))) ids.resize(std::max(20, tamanho));
    }
    std::shuffle(ids.begin(), ids.end(), rng);
    ids.resize(tamanho);

    return ids;
}

std::vector<std::vector<AlocacaoFluxo>> gerarAlternativasBloco(const Instancia &inst, const Solucao &sol, const std::vector<int> &grupo, int maximo, double limite) {
    if (!sol.viavel || sol.alocacoes.size() != inst.fluxos.size() || grupo.empty() || maximo < 2 || !std::isfinite(limite) || limite < 0) throw std::invalid_argument("Parametros do gerador de alternativas invalidos");
    Cronometro tempo;
    std::vector<unsigned char> visto(inst.fluxos.size(), 0);
    for (int f : grupo) {
        if (f < 0 || f >= static_cast<int>(inst.fluxos.size()) || visto[f]) throw std::invalid_argument("Fluxo invalido ou repetido");
        visto[f] = 1;
    }
    std::vector<std::vector<Inteiro>> cargas(inst.cenarios.size(), std::vector<Inteiro>(inst.companhias.size(), 0));
    for (const auto &s : inst.cenarios) for (const auto &f : inst.fluxos) {
        const auto &a = sol.alocacoes[f.id];
        int m1 = inst.rotas[a.rota1].companhia;
        cargas[s.id][m1] += cargaFluxo(inst, sol, f.id, s.id, m1);
        if (a.rota2 >= 0 && inst.rotas[a.rota2].companhia != m1) {
            int m2 = inst.rotas[a.rota2].companhia;
            cargas[s.id][m2] += cargaFluxo(inst, sol, f.id, s.id, m2);
        }
    }
    std::vector<std::vector<AlocacaoFluxo>> listas;
    for (int f : grupo) {
        if (tempo.segundosDecorridos() >= limite) return {};
        listas.push_back(alternativas(inst, sol, f, grupo, cargas, maximo, tempo, limite));
    }
    if (tempo.segundosDecorridos() >= limite) return {};

    return listas;
}

Solucao construirSolucao(const Instancia &inst, const ParametrosConstrucao &p, unsigned semente, EstatisticasConstrucao *estatisticas) {
    if (p.maximoBlocos < 0 || p.maximoFluxosBloco < 1 || p.alternativasPorFluxo < 2 || p.propostasPorBloco < 1 || p.tamanhoRCL < 1 || p.percentualAleatorio < 0 || p.percentualAleatorio > 100 || !std::isfinite(p.limiteSegundos) || p.limiteSegundos < 0) throw std::invalid_argument("Parametros da construtiva invalidos");
    Cronometro tempo;
    Solucao sol = criarSolucaoSpot(inst);
    EstatisticasConstrucao dados;
    std::mt19937 rng(semente);
    int maximo = std::min(p.maximoFluxosBloco, static_cast<int>(inst.fluxos.size()));
    int tamanhoInicial = std::min(3, maximo), tamanho = tamanhoInicial;
    while (dados.blocos < p.maximoBlocos && tempo.segundosDecorridos() < p.limiteSegundos) {
        auto grupo = selecionarGrupo(inst, sol, rng, tamanho, p.percentualAleatorio);
        ++dados.blocos;
        auto listas = gerarAlternativasBloco(inst, sol, grupo, p.alternativasPorFluxo, std::max(0.0, p.limiteSegundos - tempo.segundosDecorridos()));
        if (listas.empty()) break;
        std::vector<Solucao> rcl;
        const Chave atual = chave(inst, sol);
        for (int tentativa = 0; tentativa < p.propostasPorBloco && tempo.segundosDecorridos() < p.limiteSegundos; ++tentativa) {
            std::vector<std::pair<int, AlocacaoFluxo>> alteracoes;
            for (std::size_t k = 0; k < grupo.size(); ++k) {
                int opcao = sortear(rng, static_cast<int>(listas[k].size()));
                // A primeira proposta combina as primeiras ofertas novas nao-SPOT.
                if (tentativa == 0) for (std::size_t j = 1; j < listas[k].size(); ++j) {
                    const auto &a = listas[k][j];
                    if (a.rota2 >= 0 || inst.rotas[a.rota1].companhia != inst.companhiaSpot) { opcao = static_cast<int>(j); break; }
                }
                alteracoes.push_back({grupo[k], listas[k][opcao]});
            }
            Solucao candidata = sol;
            ++dados.candidatos;
            if (!alterarAlocacoes(inst, candidata, alteracoes) || !(chave(inst, candidata) < atual)) continue;
            rcl.push_back(std::move(candidata));
            std::stable_sort(rcl.begin(), rcl.end(), [&](const Solucao &a, const Solucao &b) { return chave(inst, a) < chave(inst, b); });
            if (rcl.size() > static_cast<std::size_t>(p.tamanhoRCL)) rcl.resize(p.tamanhoRCL);
        }
        if (!rcl.empty()) { sol = std::move(rcl[sortear(rng, static_cast<int>(rcl.size()))]); ++dados.blocosAceitos; tamanho = tamanhoInicial; }
        else if (tamanho < maximo) ++tamanho;
        else tamanho = tamanhoInicial;
    }
    dados.segundos = tempo.segundosDecorridos();
    if (estatisticas) *estatisticas = dados;

    return sol;
}
