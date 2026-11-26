#include "fluxo.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
std::vector<std::string> argumentosUtf8(int argc, char **argv) {
#ifdef _WIN32
    (void)argc; (void)argv;
    int quantidade = 0;
    std::unique_ptr<wchar_t *, decltype(&LocalFree)> argumentos(CommandLineToArgvW(GetCommandLineW(), &quantidade), LocalFree);
    if (!argumentos) throw std::runtime_error("Nao foi possivel ler os argumentos Unicode");
    std::vector<std::string> resultado;
    for (int i = 0; i < quantidade; ++i) resultado.push_back(std::filesystem::path(argumentos.get()[i]).u8string());

    return resultado;
#else
    return {argv, argv + argc};
#endif
}

void lerParametroSA(const std::string &arg, const std::string &valor, ParametrosSA &p) {
    std::size_t fim = 0;
    if (arg == "--sa-max") p.saMax = std::stoi(valor, &fim);
    else if (arg == "--sa-max-propostas") p.maxPropostas = std::stoi(valor, &fim);
    else if (arg == "--sa-temperatura-inicial") p.temperaturaInicial = std::stod(valor, &fim);
    else if (arg == "--sa-temperatura-final") p.temperaturaFinal = std::stod(valor, &fim);
    else if (arg == "--sa-alfa") p.alfa = std::stod(valor, &fim);
    else if (arg == "--sa-segundos") p.tempoLimite = std::stod(valor, &fim);
    else if (arg == "--sa-aleatorio-grupos") p.percentualAleatorioGrupos = std::stoi(valor, &fim);
    else if (arg == "--sa-pesos") {
        std::istringstream entrada(valor);
        std::string peso;
        std::size_t i = 0;
        while (std::getline(entrada, peso, ',')) {
            if (i == numeroMovimentosSA) throw std::invalid_argument("Quantidade de pesos invalida");
            p.pesosMovimentos[i++] = std::stoi(peso, &fim);
            if (fim != peso.size()) throw std::invalid_argument("Peso invalido");
        }
        if (i != numeroMovimentosSA || valor.empty() || valor.back() == ',') throw std::invalid_argument("Informe oito pesos separados por virgula");
        return;
    } else throw std::invalid_argument("Opcao desconhecida: " + arg);
    if (fim != valor.size()) throw std::invalid_argument("Valor invalido para " + arg);
}
void lerParametroTabu(const std::string &arg, const std::string &valor, ParametrosTabu &p) {
    std::size_t fim = 0;
    if (arg == "--tabu-segundos") p.tempoMax = std::stod(valor, &fim);
    else if (arg == "--tabu-lista") p.tamanhoLista = std::stoi(valor, &fim);
    else if (arg == "--tabu-vizinhos") p.tamanhoVizinhanca = std::stoi(valor, &fim);
    else if (arg == "--tabu-iteracoes") p.maxIteracoes = std::stoi(valor, &fim);
    else if (arg == "--tabu-max-propostas") p.maxPropostas = std::stoi(valor, &fim);
    else throw std::invalid_argument("Opcao desconhecida: " + arg);
    if (fim != valor.size()) throw std::invalid_argument("Valor invalido para " + arg);
}
}

int main(int argc, char **argv) {
    try {
        const auto argumentos = argumentosUtf8(argc, argv);
        ParametrosExecucao parametros;
        std::string saida;
        std::vector<std::string> arquivos;
        for (std::size_t i = 1; i < argumentos.size(); ++i) {
            std::string arg = argumentos[i];
            if (arg == "--help") {
                std::cout << "Uso: competicao [--saida solucao.json] [--semente N] "
                          << "[--construcao-segundos T] [--refinamento-segundos T] [--sa|--sem-sa] instancia.json [outras...]\n"
                          << "SA: --sa-segundos T --sa-max N --sa-max-propostas N --sa-temperatura-inicial T "
                          << "--sa-temperatura-final T --sa-alfa A --sa-pesos p1,...,p8 --sa-aleatorio-grupos P\n"
                          << "Tabu opcional: --tabu|--sem-tabu --tabu-segundos T --tabu-lista N --tabu-vizinhos N "
                          << "--tabu-iteracoes N --tabu-max-propostas N\n"
                          << "Refinamento: --bb|--enumeracao --alternativas-comerciais|--alternativas-basicas\n";
                return 0;
            }
            if (arg == "--sa" || arg == "--sem-sa") { parametros.executarSA = arg == "--sa"; continue; }
            if (arg == "--bb" || arg == "--enumeracao") { parametros.refinamento.usarBB = arg == "--bb"; continue; }
            if (arg == "--alternativas-comerciais" || arg == "--alternativas-basicas") { parametros.refinamento.alternativasComerciais = arg == "--alternativas-comerciais"; continue; }
            if (arg == "--tabu" || arg == "--sem-tabu") { parametros.executarTabu = arg == "--tabu"; continue; }
            if (arg.compare(0, 7, "--tabu-") == 0) {
                if (++i == argumentos.size()) throw std::invalid_argument("Falta valor para " + arg);
                lerParametroTabu(arg, argumentos[i], parametros.tabu);
                parametros.executarTabu = true;
            } else if (arg.compare(0, 5, "--sa-") == 0) {
                if (++i == argumentos.size()) throw std::invalid_argument("Falta valor para " + arg);
                lerParametroSA(arg, argumentos[i], parametros.sa);
            } else if (arg == "--saida" || arg == "--semente" || arg == "--construcao-segundos" || arg == "--refinamento-segundos") {
                if (++i == argumentos.size()) throw std::invalid_argument("Falta valor para " + arg);
                std::string valor = argumentos[i];
                std::size_t fim = 0;
                if (arg == "--saida") saida = valor;
                else if (arg == "--semente") {
                    auto numero = std::stoull(valor, &fim);
                    if (valor.empty() || valor.front() == '-' || fim != valor.size() || numero > std::numeric_limits<unsigned>::max()) throw std::invalid_argument("Semente invalida");
                    parametros.semente = static_cast<unsigned>(numero);
                } else {
                    double segundos = std::stod(valor, &fim);
                    if (fim != valor.size()) throw std::invalid_argument("Tempo invalido");
                    if (arg == "--construcao-segundos") parametros.construcao.limiteSegundos = segundos;
                    else parametros.segundosRefinamento = segundos;
                }
            } else {
                if (arg.compare(0, 2, "--") == 0) throw std::invalid_argument("Opcao desconhecida: " + arg);
                arquivos.push_back(arg);
            }
        }
        if (arquivos.empty() || (!saida.empty() && arquivos.size() != 1)) throw std::invalid_argument("Informe instancias; --saida exige uma unica instancia. Consulte --help.");
        for (const auto &arquivo : arquivos) {
            const auto inst = lerInstancia(arquivo);
            const auto resultado = executarFluxo(inst, parametros);
            std::filesystem::path destino = std::filesystem::u8path(saida);
            if (saida.empty()) destino = std::filesystem::path("output/solucoes") / (std::filesystem::u8path(arquivo).stem().u8string() + ".json");
            if (!destino.parent_path().empty()) std::filesystem::create_directories(destino.parent_path());
            salvarSolucao(inst, resultado.solucao, destino.u8string());
            std::cout << inst.meta.nome << std::fixed << std::setprecision(2)
                      << ": inicial=" << resultado.custoConstrucao << " apos_SA=" << resultado.custoAposSA
                      << " final=" << resultado.solucao.funcaoObjetivo << " SA=" << std::boolalpha << resultado.saExecutado
                      << " blocos=" << resultado.construcao.blocos << " aceitos=" << resultado.construcao.blocosAceitos
                      << " construcao_s=" << resultado.construcao.segundos << " refinamento_s=" << resultado.segundosRefinamento
                      << " SA_s=" << resultado.estatisticasSA.tempoExecucao << " propostas=" << resultado.estatisticasSA.propostas
                      << " Tabu=" << resultado.tabuExecutado << " apos_tabu=" << resultado.custoAposTabu
                      << " tabu_s=" << resultado.estatisticasTabu.tempoExecucao << " tabu_propostas=" << resultado.estatisticasTabu.propostas
                      << " BB_nos=" << resultado.nosBB << " BB_podas=" << resultado.podasBB
                      << " saida=" << destino.u8string() << '\n';
            if (resultado.saExecutado) for (std::size_t i = 0; i < numeroMovimentosSA; ++i) {
                const auto &e = resultado.estatisticasSA;
                std::cout << "  " << nomeMovimentoSA(static_cast<MovimentoSA>(i)) << ": propostas=" << e.propostasPorMovimento[i]
                          << " viaveis=" << e.viaveisPorMovimento[i] << " aceitos=" << e.aceitosPorMovimento[i] << '\n';
            }
        }
    } catch (const std::exception &erro) { std::cerr << "Erro: " << erro.what() << '\n'; return 1; }

    return 0;
}
