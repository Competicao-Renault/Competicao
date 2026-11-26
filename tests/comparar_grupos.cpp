#include "refinamento/refinamento.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

namespace {
std::vector<int> selecionar(const Instancia &inst, const Solucao &sol, int estrategia,
                           const std::vector<std::vector<Inteiro>> &ofertas, std::mt19937 &rng,
                           std::vector<int> &ordemAleatoria, std::size_t &posicao, bool &aleatorio) {
    // As dirigidas reservam 30% das chamadas para grupos totalmente aleatorios.
    aleatorio = estrategia == 0 || std::uniform_int_distribution<int>(0, 99)(rng) < 30;
    std::vector<int> ids(inst.fluxos.size());
    std::iota(ids.begin(), ids.end(), 0);
    if (!aleatorio) {
        int companhia = -1;
        if (estrategia == 1) {
            std::vector<int> elegiveis;
            for (const auto &m : inst.companhias) {
                if (m.id != inst.companhiaSpot && std::count_if(ofertas[m.id].begin(), ofertas[m.id].end(), [](Inteiro v) { return v >= 0; }) >= 3) elegiveis.push_back(m.id);
            }
            if (!elegiveis.empty()) companhia = elegiveis[std::uniform_int_distribution<int>(0, static_cast<int>(elegiveis.size()) - 1)(rng)];
            else aleatorio = true;
        }
        if (!aleatorio) {
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
                if (companhia >= 0 && ofertas[companhia][f.id] >= 0) prioridade[f.id] -= static_cast<double>(ofertas[companhia][f.id]) * f.demanda;
            }
            if (companhia >= 0) ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int f) { return ofertas[companhia][f] < 0; }), ids.end());
            std::stable_sort(ids.begin(), ids.end(), [&](int a, int b) { return prioridade[a] > prioridade[b]; });
            // Sorteia entre os 20 prioritarios para evitar repetir sempre o mesmo trio.
            if (ids.size() > 20) ids.resize(20);
        }
    }
    if (aleatorio) {
        // Preserva a referencia anterior: embaralha todos os fluxos e os visita
        // em trios sem repeticao ate esgotar a ordem, antes de embaralhar de novo.
        if (posicao + 3 > ordemAleatoria.size()) { std::shuffle(ordemAleatoria.begin(), ordemAleatoria.end(), rng); posicao = 0; }
        std::vector<int> trio{ordemAleatoria[posicao], ordemAleatoria[posicao+1], ordemAleatoria[posicao+2]};
        posicao += 3;

        return trio;
    }
    std::shuffle(ids.begin(), ids.end(), rng);
    ids.resize(3);

    return ids;
}
}

int main(int argc, char **argv) {
    try {
        double limite = 1;
        int sementes = 5;
        if (argc > 1) limite = std::stod(argv[1]);
        if (argc > 2) sementes = std::stoi(argv[2]);
        if (argc > 3 || !std::isfinite(limite) || limite <= 0 || sementes <= 0) throw std::invalid_argument("Uso: comparar_grupos [segundos=1] [sementes=5]");
        const std::vector<std::string> nomes{"aleatoria", "companhia", "pior_cenario"};
        std::filesystem::create_directories("output/comparacao_grupos");
        std::ofstream csv("output/comparacao_grupos/resultados.csv");
        if (!csv) throw std::runtime_error("Erro ao criar CSV");
        csv << "instancia,estrategia,semente,custo_inicial,custo_final,reducao_percentual,segundos,grupos,grupos_melhorados,grupos_interrompidos,candidatos_grupo,quantidades_melhoradas,ganho_grupos,ganho_quantidades,pior_cenario,grupos_aleatorios,grupos_dirigidos\n";
        csv << std::fixed << std::setprecision(6);
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            for (int semente = 1; semente <= sementes; ++semente) for (int ordem = 0; ordem < 3; ++ordem) {
                int estrategia = (ordem + semente - 1) % 3; // Alterna a ordem de execucao.
                auto sol = criarSolucaoSpot(inst);
                std::mt19937 rng(semente);
                std::vector<int> ordemAleatoria(inst.fluxos.size());
                std::iota(ordemAleatoria.begin(), ordemAleatoria.end(), 0);
                std::size_t posicao = ordemAleatoria.size();
                double inicial = sol.funcaoObjetivo, ganhoGrupo = 0, ganhoQuantidade = 0;
                int grupos = 0, melhorados = 0, interrompidos = 0, candidatos = 0, quantidades = 0, aleatorios = 0;
                Cronometro tempo;
                std::vector<std::vector<Inteiro>> ofertas(inst.companhias.size(), std::vector<Inteiro>(inst.fluxos.size(), -1));
                for (const auto &r : inst.rotas) {
                    auto &c = ofertas[r.companhia][r.fluxo];
                    if (c < 0 || inst.custosRotas[r.id] < c) c = inst.custosRotas[r.id];
                }
                while (tempo.segundosDecorridos() < limite) {
                    bool aleatorio = false;
                    auto fluxos = selecionar(inst, sol, estrategia, ofertas, rng, ordemAleatoria, posicao, aleatorio);
                    ParametrosRefinamento p;
                    p.limiteSegundos = std::min(0.2, limite - tempo.segundosDecorridos());
                    if (p.limiteSegundos <= 0) break;
                    double anterior = sol.funcaoObjetivo;
                    auto r = refinarGrupo(inst, sol, fluxos, p);
                    ++grupos; aleatorios += aleatorio; melhorados += r.melhorou; interrompidos += !r.concluido;
                    candidatos += r.candidatosTestados;
                    ganhoGrupo += anterior - sol.funcaoObjetivo;
                    for (int f : fluxos) {
                        if (tempo.segundosDecorridos() >= limite) break;
                        anterior = sol.funcaoObjetivo;
                        auto q = refinarQuantidade(inst, sol, f);
                        quantidades += q.melhorou;
                        ganhoQuantidade += anterior - sol.funcaoObjetivo;
                    }
                }
                double segundos = tempo.segundosDecorridos();
                auto copia = sol;
                if (!avaliarSolucao(inst, copia) || copia.funcaoObjetivo != sol.funcaoObjetivo || sol.funcaoObjetivo > inicial) throw std::runtime_error("Resultado inconsistente");
                double reducao = 100 * (inicial - sol.funcaoObjetivo) / inicial;
                salvarSolucao(inst, sol, "output/comparacao_grupos/" + nome + '-' + nomes[estrategia] + "-semente-" + std::to_string(semente) + ".json");
                csv << nome << ',' << nomes[estrategia] << ',' << semente << ',' << inicial << ',' << sol.funcaoObjetivo << ',' << reducao << ','
                    << segundos << ',' << grupos << ',' << melhorados << ',' << interrompidos << ',' << candidatos << ',' << quantidades << ','
                    << ganhoGrupo << ',' << ganhoQuantidade << ',' << sol.piorCenario << ',' << aleatorios << ',' << grupos - aleatorios << '\n';
                csv.flush();
                std::cout << nome << ' ' << nomes[estrategia] << " seed=" << semente << " custo=" << std::fixed << std::setprecision(0)
                          << sol.funcaoObjetivo << " reducao=" << std::setprecision(2) << reducao << "% grupos=" << grupos << std::endl;
            }
        }
        csv.close();
        if (!csv) throw std::runtime_error("Erro ao gravar CSV");
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }

    return 0;
}
