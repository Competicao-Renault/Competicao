#include "fluxo.hpp"
#include "refinamento/refinamento.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;

struct Configuracao {
    std::string nome;
    bool sa, tabu, refinar;
    ParametrosSA pSA;
    ParametrosTabu pTabu;
    int grupos;
    double segundos;
    ParametrosRefinamento grupo;
};

Configuracao lerConfiguracao(std::istream &entrada) {
    Configuracao c{};
    auto &p = c.pSA;
    auto &t = c.pTabu;
    entrada >> c.nome >> c.sa >> c.tabu >> c.refinar
            >> p.temperaturaInicial >> p.temperaturaFinal >> p.alfa >> p.saMax >> p.maxPropostas
            >> p.tempoLimite >> p.percentualAleatorioGrupos;
    for (auto &peso : p.pesosMovimentos) {
        entrada >> peso;
    }
    entrada >> t.tamanhoLista >> t.tamanhoVizinhanca >> t.maxIteracoes >> t.maxPropostas >> t.tempoMax
            >> c.grupos >> c.segundos >> c.grupo.maximoAlternativas >> c.grupo.maximoCombinacoes >> c.grupo.limiteSegundos;
    if (c.sa) {
        validarParametrosSA(p);
    }
    if (c.tabu) {
        validarParametrosTabu(t);
    }

    return c;
}

void validar(const Instancia &inst, Solucao &sol, double esperado) {
    const int pior = sol.piorCenario;
    std::string motivo;
    if (!avaliarSolucao(inst, sol, &motivo) || sol.funcaoObjetivo != esperado || sol.piorCenario != pior) {
        throw std::runtime_error("Reavaliacao divergente em " + inst.meta.nome + " " + motivo);
    }
}

Inteiro volumeSpot(const Instancia &inst, const Solucao &sol) {
    Inteiro volume = 0;
    for (const auto &a : sol.alocacoes) {
        if (inst.rotas[a.rota1].companhia == inst.companhiaSpot) {
            volume += a.quantidade1;
        }
        if (a.rota2 >= 0 && inst.rotas[a.rota2].companhia == inst.companhiaSpot) {
            volume += a.quantidade2;
        }
    }

    return volume;
}

// Mesmo encadeamento do fluxo, aplicável a uma solução já construída.
int refinar(const Instancia &inst, Solucao &sol, unsigned seed, int percentual, const Configuracao &c, double &segundos) {
    const Cronometro tempo;
    int melhorias = 0;
    for (const auto &f : inst.fluxos) {
        if (tempo.segundosDecorridos() >= c.segundos) {
            break;
        }
        melhorias += refinarQuantidade(inst, sol, f.id).melhorou;
    }
    std::mt19937 rng(seed);
    for (int i = 0; i < c.grupos && inst.fluxos.size() >= 2; ++i) {
        const double restante = c.segundos - tempo.segundosDecorridos();
        if (restante <= 0) {
            break;
        }
        auto grupo = selecionarGrupo(inst, sol, rng, std::min(3, static_cast<int>(inst.fluxos.size())), percentual);
        auto limites = c.grupo;
        limites.limiteSegundos = std::min(limites.limiteSegundos, restante);
        melhorias += refinarGrupo(inst, sol, grupo, limites).melhorou;
        for (int f : grupo) {
            if (tempo.segundosDecorridos() >= c.segundos) {
                break;
            }
            melhorias += refinarQuantidade(inst, sol, f).melhorou;
        }
    }
    segundos = tempo.segundosDecorridos();

    return melhorias;
}

const char *motivoSA(const EstatisticasSA &e) {
    if (e.motivoEncerramento == MotivoEncerramentoSA::LimitePropostas) {
        return "propostas";
    }
    if (e.motivoEncerramento == MotivoEncerramentoSA::LimiteTempo) {
        return "tempo";
    }

    return "temperatura";
}

// A API de Tabu só devolve contadores. A classificação usa os limites atingidos.
const char *motivoTabu(const EstatisticasTabu &e, const ParametrosTabu &p) {
    if (e.propostas >= p.maxPropostas) {
        return "propostas";
    }
    if (e.iteracoes >= p.maxIteracoes) {
        return "iteracoes";
    }
    if (e.tempoExecucao >= p.tempoMax) {
        return "tempo";
    }

    return "nao_identificado";
}

void executar(const Instancia &inst, const Solucao &inicial, unsigned seed, const Configuracao &c, int percentual,
              const fs::path &base, std::ostream &execucoes, std::ostream &operadores, const EstatisticasConstrucao &construcao) {
    Solucao sol = inicial;
    const std::string id = inst.meta.nome + "," + c.nome + "," + std::to_string(seed);
    std::string arquivoSA, arquivoTabu;
    EstatisticasSA e;
    EstatisticasTabu t;
    if (c.sa) {
        semente(seed);
        sol = simulatedAnnealing(inst, sol, c.pSA, &e);
        validar(inst, sol, e.melhorCusto);
        arquivoSA = base.filename().string() + "-sa.json";
        salvarSolucao(inst, sol, (base.parent_path() / arquivoSA).string());
        for (std::size_t i = 0; i < numeroMovimentosSA; ++i) {
            operadores << id << ',' << nomeMovimentoSA(static_cast<MovimentoSA>(i)) << ',' << c.pSA.pesosMovimentos[i]
                       << ',' << e.propostasPorMovimento[i] << ',' << e.avaliadosPorMovimento[i] << ',' << e.viaveisPorMovimento[i] << ',' << e.aceitosPorMovimento[i] << '\n';
        }
    }
    const double custoSA = sol.funcaoObjetivo;
    if (c.tabu) {
        semente(seed);
        const double custo = buscaTabu(inst, sol, c.pTabu, &t);
        validar(inst, sol, custo);
        if (custo != t.melhorCusto) {
            throw std::runtime_error("Estatistica de custo da Tabu diverge");
        }
        arquivoTabu = base.filename().string() + "-tabu.json";
        salvarSolucao(inst, sol, (base.parent_path() / arquivoTabu).string());
    }
    const double custoTabu = sol.funcaoObjetivo;
    double tempoRef = 0;
    int melhoriasRef = 0;
    if (c.refinar) {
        melhoriasRef = refinar(inst, sol, seed, percentual, c, tempoRef);
    }
    validar(inst, sol, sol.funcaoObjetivo);
    const std::string arquivoFinal = base.filename().string() + "-final.json";
    salvarSolucao(inst, sol, (base.parent_path() / arquivoFinal).string());

    double reducao = 0;
    if (inicial.funcaoObjetivo != 0) {
        reducao = 100 * (inicial.funcaoObjetivo - sol.funcaoObjetivo) / std::abs(inicial.funcaoObjetivo);
    }
    const char *paradaSA = "", *paradaTabu = "";
    if (c.sa) {
        paradaSA = motivoSA(e);
    }
    if (c.tabu) {
        paradaTabu = motivoTabu(t, c.pTabu);
    }
    execucoes << id << ',' << c.sa << ',' << c.tabu << ',' << c.refinar << ',' << inicial.funcaoObjetivo
              << ',' << custoSA << ',' << custoTabu << ',' << sol.funcaoObjetivo << ',' << reducao
              << ',' << construcao.segundos << ',' << e.tempoExecucao << ',' << t.tempoExecucao << ',' << tempoRef
              << ',' << volumeSpot(inst, inicial) << ',' << volumeSpot(inst, sol) << ',' << sol.piorCenario << ",1," << melhoriasRef
              << ',' << paradaSA << ',' << e.propostas << ',' << e.candidatosAvaliados << ',' << e.candidatosViaveis << ',' << e.propostas - e.candidatosAvaliados
              << ',' << e.melhoriasAceitas << ',' << e.empatesAceitos << ',' << e.piorasAceitas << ',' << e.recordes << ',' << e.niveisConcluidos
              << ',' << e.temperaturaFinal << ',' << e.custoInicial << ',' << e.melhorCusto << ',' << e.custoCorrente
              << ',' << paradaTabu << ',' << t.iteracoes << ',' << t.propostas << ',' << t.viaveis << ',' << t.aceitos << ',' << t.bloqueados
              << ',' << t.aspiracoes << ',' << t.reinicios << ',' << t.custoInicial << ',' << t.melhorCusto << ',' << t.custoCorrente
              << ',' << arquivoSA << ',' << arquivoTabu << ',' << arquivoFinal << '\n';
}

int main(int argc, char **argv) {
    try {
        if (argc != 3) {
            throw std::invalid_argument("Uso: bancada plano diretorio");
        }
        std::ifstream entrada(argv[1]);
        entrada.exceptions(std::ios::failbit | std::ios::badbit);
        const fs::path pasta = argv[2], solucoes = pasta / "solucoes";
        fs::create_directory(solucoes);
        ParametrosConstrucao p;
        entrada >> p.maximoBlocos >> p.maximoFluxosBloco >> p.alternativasPorFluxo >> p.propostasPorBloco
                >> p.tamanhoRCL >> p.percentualAleatorio >> p.limiteSegundos;
        std::size_t quantidade;
        entrada >> quantidade;
        std::vector<std::string> nomes(quantidade);
        for (auto &nome : nomes) {
            entrada >> nome;
        }
        entrada >> quantidade;
        std::vector<unsigned> sementes(quantidade);
        for (auto &seed : sementes) {
            entrada >> seed;
        }
        entrada >> quantidade;
        std::vector<Configuracao> configs;
        for (std::size_t i = 0; i < quantidade; ++i) {
            configs.push_back(lerConfiguracao(entrada));
        }

        std::ofstream execucoes(pasta / "execucoes.csv"), operadores(pasta / "operadores_sa.csv"), iniciais(pasta / "construcoes.csv");
        for (auto *arquivo : {&execucoes, &operadores, &iniciais}) {
            arquivo->exceptions(std::ios::failbit | std::ios::badbit);
            *arquivo << std::setprecision(std::numeric_limits<double>::max_digits10);
        }
        execucoes << "instancia,configuracao,semente,sa,tabu,refinamento,custo_inicial,custo_apos_sa,custo_apos_tabu,custo_final,reducao_pct,"
                  << "tempo_construcao_s,tempo_sa_s,tempo_tabu_s,tempo_refinamento_s,spot_inicial,spot_final,pior_cenario,viavel,melhorias_refinamento,"
                  << "motivo_sa,propostas_sa,avaliados_sa,viaveis_sa,descartes_sa,melhorias_sa,empates_sa,pioras_sa,recordes_sa,niveis_sa,"
                  << "temperatura_atingida,custo_inicial_sa,melhor_custo_sa,custo_corrente_sa,"
                  << "motivo_tabu_inferido,iteracoes_tabu,propostas_tabu,viaveis_tabu,aceitos_tabu,bloqueados_tabu,aspiracoes_tabu,"
                  << "reinicios_tabu,custo_inicial_tabu,melhor_custo_tabu,custo_corrente_tabu,json_sa,json_tabu,json_final\n";
        operadores << "instancia,configuracao,semente,operador,peso,propostas,avaliados,viaveis,aceitos\n";
        iniciais << "instancia,semente,custo,spot,tempo_s,blocos,candidatos,blocos_aceitos,json\n";
        for (const auto &nome : nomes) {
            const auto inst = lerInstancia("instancia/" + nome + ".json");
            for (unsigned seed : sementes) {
                EstatisticasConstrucao e;
                Solucao inicial = construirSolucao(inst, p, seed, &e);
                validar(inst, inicial, inicial.funcaoObjetivo);
                const auto prefixo = nome + "-" + std::to_string(seed);
                const auto json = prefixo + "-inicial.json";
                salvarSolucao(inst, inicial, (solucoes / json).string());
                iniciais << nome << ',' << seed << ',' << inicial.funcaoObjetivo << ',' << volumeSpot(inst, inicial)
                         << ',' << e.segundos << ',' << e.blocos << ',' << e.candidatos << ',' << e.blocosAceitos << ',' << json << '\n';
                for (const auto &c : configs) {
                    executar(inst, inicial, seed, c, p.percentualAleatorio, solucoes / (prefixo + "-" + c.nome), execucoes, operadores, e);
                    std::cout << nome << " semente=" << seed << " configuracao=" << c.nome << std::endl;
                }
            }
        }
        execucoes.close();
        operadores.close();
        iniciais.close();
    } catch (const std::exception &erro) {
        std::cerr << erro.what() << '\n';
        return 1;
    }

    return 0;
}
