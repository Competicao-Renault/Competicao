#include "definicao_problema/instancia.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) { std::cerr << "Uso: competicao <instancia.json> [outras instancias...]\n"; return 1; }
    try {
        for (int i = 1; i < argc; ++i) {
            const Instancia inst = lerInstancia(argv[i]);
            std::cout << inst.meta.nome << ": " << inst.portos.size() << " portos, " << inst.sites.size()
                      << " sites, " << inst.companhias.size() << " companhias, " << inst.fluxos.size()
                      << " fluxos, " << inst.rotas.size() << " rotas, " << inst.cenarios.size()
                      << " cenarios, " << inst.demandaTotal << " TEU, alpha=" << inst.meta.alpha << '\n';
        }
    } catch (const std::exception &erro) { std::cerr << "Erro: " << erro.what() << '\n'; return 1; }

    return 0;
}
