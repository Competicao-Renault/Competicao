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

// Ensaio isolado: SPOT -> grupos aleatorios -> quantidades dos fluxos do grupo.
// Nao implementa nem substitui a futura construtiva ou o SA.
int main(int argc, char **argv) {
    try {
        double limite = 5;
        int sementes = 3;
        if (argc > 1) limite = std::stod(argv[1]);
        if (argc > 2) sementes = std::stoi(argv[2]);
        if (argc > 3 || !std::isfinite(limite) || limite <= 0 || sementes <= 0) throw std::invalid_argument("Uso: simulacao [segundos_por_execucao=5] [sementes=3]");
        std::filesystem::create_directories("output/simulacao");
        std::ofstream csv("output/simulacao/resultados.csv");
        if (!csv) throw std::runtime_error("Nao foi possivel criar resultados.csv");
        csv << "instancia,semente,custo_inicial,custo_final,reducao_percentual,segundos,grupos,grupos_melhorados,grupos_interrompidos,candidatos_grupo,quantidades_melhoradas,ganho_grupos,ganho_quantidades,segundos_grupos,segundos_quantidades,pior_cenario,volume_spot\n";
        csv << std::fixed << std::setprecision(6);
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            for (int semente = 1; semente <= sementes; ++semente) {
                auto sol = criarSolucaoSpot(inst);
                double inicial = sol.funcaoObjetivo, ganhoGrupo = 0, ganhoQuantidade = 0, tempoGrupo = 0, tempoQuantidade = 0;
                int grupos = 0, melhorados = 0, interrompidos = 0, candidatos = 0, quantidades = 0;
                std::mt19937 rng(semente);
                std::vector<int> ordem(inst.fluxos.size());
                std::iota(ordem.begin(), ordem.end(), 0);
                std::size_t pos = ordem.size();
                Cronometro tempo;
                while (grupos < 100 && tempo.segundosDecorridos() < limite) {
                    if (pos + 3 > ordem.size()) { std::shuffle(ordem.begin(), ordem.end(), rng); pos = 0; }
                    std::vector<int> fluxos{ordem[pos], ordem[pos+1], ordem[pos+2]};
                    pos += 3;
                    ParametrosRefinamento p;
                    p.limiteSegundos = std::min(0.2, limite - tempo.segundosDecorridos());
                    if (p.limiteSegundos <= 0) break;
                    double anterior = sol.funcaoObjetivo;
                    Cronometro etapa;
                    auto r = refinarGrupo(inst, sol, fluxos, p);
                    tempoGrupo += etapa.segundosDecorridos();
                    ++grupos; melhorados += r.melhorou; interrompidos += !r.concluido;
                    candidatos += r.candidatosTestados;
                    ganhoGrupo += anterior - sol.funcaoObjetivo;
                    Cronometro ajustes;
                    for (int f : fluxos) {
                        if (tempo.segundosDecorridos() >= limite) break;
                        anterior = sol.funcaoObjetivo;
                        auto q = refinarQuantidade(inst, sol, f);
                        quantidades += q.melhorou;
                        ganhoQuantidade += anterior - sol.funcaoObjetivo;
                    }
                    tempoQuantidade += ajustes.segundosDecorridos();
                }
                double segundos = tempo.segundosDecorridos();
                auto conferencia = sol;
                if (!avaliarSolucao(inst, conferencia) || conferencia.funcaoObjetivo != sol.funcaoObjetivo || sol.funcaoObjetivo > inicial) throw std::runtime_error("Solucao final inconsistente");
                Inteiro volumeSpot = 0;
                for (const auto &a : sol.alocacoes) {
                    if (inst.rotas[a.rota1].companhia == inst.companhiaSpot) volumeSpot += a.quantidade1;
                    if (a.rota2 >= 0 && inst.rotas[a.rota2].companhia == inst.companhiaSpot) volumeSpot += a.quantidade2;
                }
                double reducao = 100 * (inicial - sol.funcaoObjetivo) / inicial;
                salvarSolucao(inst, sol, "output/simulacao/" + nome + "-semente-" + std::to_string(semente) + ".json");
                csv << nome << ',' << semente << ',' << inicial << ',' << sol.funcaoObjetivo << ',' << reducao << ',' << segundos << ','
                    << grupos << ',' << melhorados << ',' << interrompidos << ',' << candidatos << ',' << quantidades << ','
                    << ganhoGrupo << ',' << ganhoQuantidade << ',' << tempoGrupo << ',' << tempoQuantidade << ',' << sol.piorCenario << ',' << volumeSpot << '\n';
                csv.flush();
                std::cout << nome << " semente=" << semente << " custo=" << std::fixed << std::setprecision(0) << sol.funcaoObjetivo
                          << " reducao=" << std::setprecision(2) << reducao << "% tempo=" << segundos << "s grupos=" << grupos
                          << " melhorias=" << melhorados << '+' << quantidades << std::endl;
            }
        }
        csv.close();
        if (!csv) throw std::runtime_error("Erro ao gravar resultados.csv");
    } catch (const std::exception &erro) { std::cerr << erro.what() << '\n'; return 1; }

    return 0;
}
