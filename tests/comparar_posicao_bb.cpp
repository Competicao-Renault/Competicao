#include "fluxo.hpp"
#include "definicao_problema/json.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
Solucao lerEntrada(const Instancia &inst, const std::string &caminho) {
    std::ifstream arquivo(std::filesystem::u8path(caminho));
    if (!arquivo) throw std::runtime_error("Entrada pareada ausente: " + caminho);
    std::string texto((std::istreambuf_iterator<char>(arquivo)), std::istreambuf_iterator<char>());
    auto json = leitura::LeitorJson(texto).ler();
    Solucao sol; sol.alocacoes.resize(inst.fluxos.size());
    for (const auto &x : json.campo("x").itens()) {
        auto r = x.campo("route").inteiro(), q = x.campo("n").inteiro();
        if (r < 0 || r >= static_cast<Inteiro>(inst.rotas.size()) || q <= 0 || q > std::numeric_limits<int>::max()) throw std::runtime_error("Alocacao da entrada invalida");
        auto &a = sol.alocacoes[inst.rotas[r].fluxo];
        if (a.rota1 < 0) { a.rota1 = static_cast<int>(r); a.quantidade1 = static_cast<int>(q); }
        else if (a.rota2 < 0) { a.rota2 = static_cast<int>(r); a.quantidade2 = static_cast<int>(q); }
        else throw std::runtime_error("Mais de duas rotas na entrada");
    }
    for (const auto &y : json.campo("y").itens()) {
        auto r1 = y.campo("from").inteiro(), r2 = y.campo("to").inteiro(), q = y.campo("n").inteiro();
        if (r1 < 0 || r2 < 0 || r1 >= static_cast<Inteiro>(inst.rotas.size()) || r2 >= static_cast<Inteiro>(inst.rotas.size()) || q <= 0 || q > std::numeric_limits<int>::max()) throw std::runtime_error("Reutilizacao da entrada invalida");
        sol.reutilizacoes.push_back({static_cast<int>(r1), static_cast<int>(r2), static_cast<int>(q)});
    }
    if (!avaliarSolucao(inst, sol)) throw std::runtime_error("Entrada pareada inviavel");

    return sol;
}
struct Medidas { int nos = 0, podas = 0, chamadas = 0; };
void refinar(const Instancia &inst, Solucao &sol, std::mt19937 &rng, double limite, Medidas &m) {
    Cronometro tempo;
    for (const auto &f : inst.fluxos) {
        if (tempo.segundosDecorridos() >= limite) break;
        refinarQuantidade(inst, sol, f.id);
    }
    while (tempo.segundosDecorridos() < limite) {
        auto grupo = selecionarGrupo(inst, sol, rng);
        ParametrosRefinamento p;
        p.limiteSegundos = std::min(0.2, limite - tempo.segundosDecorridos());
        if (p.limiteSegundos <= 0) break;
        auto r = refinarGrupo(inst, sol, grupo, p);
        ++m.chamadas; m.nos += r.nosVisitados; m.podas += r.podasCusto + r.podasCapacidade + r.podasMinimo;
        for (int f : grupo) {
            if (tempo.segundosDecorridos() >= limite) break;
            refinarQuantidade(inst, sol, f);
        }
    }
}
}

int main(int argc, char **argv) {
    try {
        const std::vector<std::string> metodos{"bb_so", "tabu_so", "bb_antes", "bb_depois", "bb_dividido"};
        const std::vector<unsigned> sementes{7, 42, 2026, 11, 19};
        std::vector<std::size_t> ativos{0, 1, 2, 3, 4};
        double orcamento = 1;
        std::string pasta = "output/posicao_bb";
        if (argc == 2 && std::string(argv[1]) == "prolongado") { orcamento = 2; pasta += "_longo"; ativos = {2, 3, 4}; }
        else if (argc != 1) throw std::invalid_argument("Uso: comparar_posicao_bb [prolongado]");
        std::filesystem::create_directories(pasta + "/entradas");
        std::ofstream csv(pasta + "/comparacao.csv");
        csv << "instancia,semente,metodo,custo_inicial,custo_intermediario1,custo_intermediario2,custo_final,ganho_percentual,segundos,bb_segundos,tabu_segundos,bb_nos,bb_podas,bb_chamadas,tabu_propostas\n" << std::setprecision(17);
        for (const std::string nome : {"alliances", "reliability", "strikes"}) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            for (std::size_t s = 0; s < sementes.size(); ++s) {
                unsigned seed = sementes[s];
                const auto base = lerEntrada(inst, "output/bb/entradas/" + nome + '-' + std::to_string(seed) + ".json");
                salvarSolucao(inst, base, pasta + "/entradas/" + nome + '-' + std::to_string(seed) + ".json");
                for (std::size_t ordem = 0; ordem < ativos.size(); ++ordem) {
                    std::size_t modo = ativos[(ordem + s) % ativos.size()]; // Alterna a ordem entre sementes.
                    Solucao sol = base;
                    std::mt19937 rng(seed); semente(seed);
                    Medidas medidas;
                    int propostas = 0;
                    double tempoBB = 0, tempoTabu = 0, intermediario1 = base.funcaoObjetivo, intermediario2 = base.funcaoObjetivo;
                    Cronometro total;
                    auto bb = [&](double limite) { Cronometro t; refinar(inst, sol, rng, limite, medidas); tempoBB += t.segundosDecorridos(); };
                    auto tabu = [&](double limite) {
                        ParametrosTabu p; p.tempoMax = limite; p.maxIteracoes = p.maxPropostas = 100000000;
                        EstatisticasTabu e; Cronometro t; buscaTabu(inst, sol, p, &e);
                        tempoTabu += t.segundosDecorridos(); propostas += e.propostas;
                    };
                    if (modo == 0) bb(orcamento);
                    else if (modo == 1) tabu(orcamento);
                    else if (modo == 2) { bb(orcamento/2); intermediario1 = sol.funcaoObjetivo; tabu(orcamento/2); }
                    else if (modo == 3) { tabu(orcamento/2); intermediario1 = sol.funcaoObjetivo; bb(orcamento/2); }
                    else { bb(orcamento/4); intermediario1 = sol.funcaoObjetivo; tabu(orcamento/2); intermediario2 = sol.funcaoObjetivo; bb(orcamento/4); }
                    if (modo < 2) intermediario1 = sol.funcaoObjetivo;
                    if (modo != 4) intermediario2 = sol.funcaoObjetivo;
                    double segundos = total.segundosDecorridos();
                    auto copia = sol;
                    if (!avaliarSolucao(inst, copia) || copia.funcaoObjetivo != sol.funcaoObjetivo || sol.funcaoObjetivo > base.funcaoObjetivo) throw std::runtime_error("Resultado de posicao invalido");
                    salvarSolucao(inst, sol, pasta + '/' + nome + '-' + std::to_string(seed) + '-' + metodos[modo] + ".json");
                    csv << nome << ',' << seed << ',' << metodos[modo] << ',' << base.funcaoObjetivo << ',' << intermediario1 << ',' << intermediario2 << ',' << sol.funcaoObjetivo << ','
                        << 100*(base.funcaoObjetivo-sol.funcaoObjetivo)/base.funcaoObjetivo << ',' << segundos << ',' << tempoBB << ',' << tempoTabu << ','
                        << medidas.nos << ',' << medidas.podas << ',' << medidas.chamadas << ',' << propostas << '\n';
                    csv.flush();
                }
                std::cout << nome << " semente=" << seed << ' ' << ativos.size() << " posicoes concluidas" << std::endl;
            }
        }
        csv.close();
        if (!csv) throw std::runtime_error("Falha ao gravar comparacao de posicoes");
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }

    return 0;
}
