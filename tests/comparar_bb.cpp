#include "fluxo.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        std::filesystem::create_directories("output/bb/entradas");
        std::ofstream csv("output/bb/comparacao.csv"), micro("output/bb/mesmas_alternativas.csv");
        csv << "instancia,semente,metodo,custo_inicial,custo_final,ganho_percentual,segundos,chamadas,folhas,nos,podas,viaveis,melhorias\n";
        micro << "instancia,semente,grupo,custo_enum,custo_bb,segundos_enum,segundos_bb,folhas_enum,folhas_bb,nos_bb,podas\n";
        csv << std::setprecision(17); micro << std::setprecision(17);
        const std::vector<std::string> nomes{"enumeracao3", "bb3", "bb_comercial3", "compensado", "contrato", "bb_compensado3", "bb4"};
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            for (unsigned seed : {7u, 42u, 2026u, 11u, 19u}) {
                ParametrosExecucao inicial;
                inicial.semente = seed; inicial.construcao.maximoBlocos = 60; inicial.construcao.limiteSegundos = 60;
                inicial.sa.maxPropostas = 3000; inicial.sa.saMax = 10000; inicial.sa.tempoLimite = 60; inicial.segundosRefinamento = 0;
                const Solucao base = executarFluxo(inst, inicial).solucao;
                salvarSolucao(inst, base, "output/bb/entradas/" + nome + '-' + std::to_string(seed) + ".json");
                std::mt19937 grupos(seed);
                for (int g = 0; g < 5; ++g) {
                    auto ids = selecionarGrupo(inst, base, grupos);
                    ParametrosRefinamento p; p.limiteSegundos = 10; p.maximoNos = 100000; p.maximoCombinacoes = 100000;
                    auto alternativas = gerarAlternativasGrupo(inst, base, ids, p);
                    auto a = base, b = base;
                    Cronometro te; auto re = enumerarGrupo(inst, a, alternativas, p); double se = te.segundosDecorridos();
                    Cronometro tb; auto rb = branchAndBound(inst, b, alternativas, p); double sb = tb.segundosDecorridos();
                    if (!re.concluido || !rb.concluido || a.funcaoObjetivo != b.funcaoObjetivo) throw std::runtime_error("BB divergiu da enumeracao nas mesmas listas");
                    micro << nome << ',' << seed << ',' << g << ',' << a.funcaoObjetivo << ',' << b.funcaoObjetivo << ',' << se << ',' << sb << ','
                          << re.candidatosTestados << ',' << rb.candidatosTestados << ',' << rb.nosVisitados << ',' << rb.podasCusto + rb.podasCapacidade + rb.podasMinimo << '\n';
                }
                for (int ordem = 0; ordem < static_cast<int>(nomes.size()); ++ordem) {
                    int modo = (ordem + seed) % static_cast<int>(nomes.size());
                    Solucao sol = base;
                    std::mt19937 rng(seed); semente(seed);
                    int chamadas = 0, folhas = 0, nos = 0, podas = 0, viaveis = 0, melhorias = 0;
                    Cronometro tempo;
                    while (tempo.segundosDecorridos() < 0.5) {
                        Solucao candidata = sol;
                        bool movimento = false;
                        if (modo == 3 || modo == 4 || (modo == 5 && chamadas % 2)) {
                            if (modo == 4) movimento = movimentoContrato(inst, candidata, 1);
                            else movimento = movimentoCompensado(inst, candidata, 1);
                            viaveis += movimento;
                        } else {
                            int tamanho = 3;
                            if (modo == 6) tamanho = 4;
                            auto ids = selecionarGrupo(inst, sol, rng, tamanho);
                            ParametrosRefinamento p; p.usarBB = modo != 0; p.alternativasComerciais = modo == 2;
                            p.maximoNos = 100000; p.maximoCombinacoes = 100000;
                            p.limiteSegundos = std::min(0.1, 0.5 - tempo.segundosDecorridos());
                            if (p.limiteSegundos <= 0) break;
                            auto r = refinarGrupo(inst, candidata, ids, p);
                            folhas += r.candidatosTestados; nos += r.nosVisitados; podas += r.podasCusto + r.podasCapacidade + r.podasMinimo;
                            movimento = r.melhorou;
                        }
                        ++chamadas;
                        if (movimento && candidata.funcaoObjetivo < sol.funcaoObjetivo) { sol = std::move(candidata); ++melhorias; }
                    }
                    double segundos = tempo.segundosDecorridos();
                    auto conferida = sol;
                    if (!avaliarSolucao(inst, conferida) || conferida.funcaoObjetivo != sol.funcaoObjetivo || sol.funcaoObjetivo > base.funcaoObjetivo) throw std::runtime_error("Resultado de comparacao invalido");
                    salvarSolucao(inst, sol, "output/bb/" + nome + '-' + std::to_string(seed) + '-' + nomes[modo] + ".json");
                    csv << nome << ',' << seed << ',' << nomes[modo] << ',' << base.funcaoObjetivo << ',' << sol.funcaoObjetivo << ','
                        << 100 * (base.funcaoObjetivo - sol.funcaoObjetivo) / base.funcaoObjetivo << ',' << segundos << ',' << chamadas << ',' << folhas << ',' << nos << ',' << podas << ',' << viaveis << ',' << melhorias << '\n';
                    csv.flush();
                }
                std::cout << nome << " semente=" << seed << " sete metodos concluidos" << std::endl;
            }
        }
        csv.close(); micro.close();
        if (!csv || !micro) throw std::runtime_error("Falha ao gravar comparacao");
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }

    return 0;
}
