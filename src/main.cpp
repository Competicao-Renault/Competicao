#include "definicao_problema/instancia.hpp"
#include "definicao_problema/solucao.hpp"
#include <iomanip>
#include <iostream>

int main(int argc, char **argv) {
    bool exportar = argc > 1 && std::string(argv[1]) == "--saida";
    if (argc < 2 || (exportar && argc != 4)) {
        std::cerr << "Uso: competicao <instancia.json> [outras instancias...]\n"
                  << "     competicao --saida <solucao.json> <instancia.json>\n";
        return 1;
    }
    try {
        int inicio = 1;
        if (exportar) inicio = 3;
        for (int i = inicio; i < argc; ++i) {
            const Instancia inst = lerInstancia(argv[i]);
            const Solucao solucao = criarSolucaoSpot(inst);
            std::cout << inst.meta.nome << ": " << inst.portos.size() << " portos, " << inst.sites.size()
                      << " sites, " << inst.companhias.size() << " companhias, " << inst.fluxos.size()
                      << " fluxos, " << inst.rotas.size() << " rotas, " << inst.cenarios.size()
                      << " cenarios, " << inst.demandaTotal << " TEU, alpha=" << inst.meta.alpha
                      << ", custo SPOT=" << std::fixed << std::setprecision(2) << solucao.funcaoObjetivo
                      << std::defaultfloat << std::setprecision(6) << '\n';
            if (exportar) salvarSolucao(inst, solucao, argv[2]);
        }
    } catch (const std::exception &erro) { std::cerr << "Erro: " << erro.what() << '\n'; return 1; }

    return 0;
}
