#include "fluxo.hpp"
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
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
                          << "[--construcao-segundos T] [--refinamento-segundos T] instancia.json [outras...]\n";
                return 0;
            }
            if (arg == "--saida" || arg == "--semente" || arg == "--construcao-segundos" || arg == "--refinamento-segundos") {
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
            // Integracao pendente: passar aqui o callback que executa o SA.
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
                      << " saida=" << destino.u8string() << '\n';
        }
    } catch (const std::exception &erro) { std::cerr << "Erro: " << erro.what() << '\n'; return 1; }

    return 0;
}
