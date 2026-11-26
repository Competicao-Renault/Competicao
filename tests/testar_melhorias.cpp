#define main main_posicao_bb
#include "comparar_posicao_bb.cpp"
#undef main
#include "refinamento/movimentos.hpp"

namespace {
Solucao reconstruir(const Instancia &inst, const Solucao &base) {
    std::vector<int> ativos, volumes(inst.rotas.size());
    for (const auto &a : base.alocacoes) {
        ativos.push_back(a.rota1); volumes[a.rota1] = a.quantidade1;
        if (a.rota2 >= 0) { ativos.push_back(a.rota2); volumes[a.rota2] = a.quantidade2; }
    }
    auto melhor = base;
    std::vector<int> ordem(inst.cenarios.size());
    std::iota(ordem.begin(), ordem.end(), 0);
    std::stable_sort(ordem.begin(), ordem.end(), [&](int a, int b) { return base.custosCenarios[a] > base.custosCenarios[b]; });
    for (std::size_t janela : {1u, 3u, 10u}) {
        struct Par { int de, para, site; long double valor; };
        std::vector<Par> pares;
        for (int de : ativos) for (int para : ativos) {
            const auto &a = inst.rotas[de], &b = inst.rotas[para];
            int t = inst.fluxos[a.fluxo].destino;
            if (a.companhia != b.companhia || t != inst.fluxos[b.fluxo].origem || !inst.sites[t].capacidadeReutilizacao) continue;
            int sobrevive = 0;
            for (std::size_t k = 0; k < std::min(janela, ordem.size()); ++k)
                sobrevive += !inst.indisponivel[ordem[k]][de] && !inst.indisponivel[ordem[k]][para];
            long double valor = static_cast<long double>(inst.custosRodoviarios[t][a.portoDestino]) + inst.custosRodoviarios[t][b.portoOrigem];
            if (valor > 0 && sobrevive) pares.push_back({de, para, t, valor * sobrevive});
        }
        std::stable_sort(pares.begin(), pares.end(), [](const Par &a, const Par &b) { return a.valor > b.valor; });
        auto candidata = base;
        candidata.reutilizacoes.clear();
        auto entrada = volumes, saida = volumes;
        std::vector<Inteiro> folga;
        for (const auto &t : inst.sites) folga.push_back(t.capacidadeReutilizacao);
        for (const auto &p : pares) {
            int q = static_cast<int>(std::min({Inteiro(saida[p.de]), Inteiro(entrada[p.para]), folga[p.site]}));
            if (!q) continue;
            candidata.reutilizacoes.push_back({p.de, p.para, q});
            saida[p.de] -= q; entrada[p.para] -= q; folga[p.site] -= q;
        }
        if (!avaliarSolucao(inst, candidata)) throw std::runtime_error("Reconstrucao inviavel");
        if (candidata.funcaoObjetivo < melhor.funcaoObjetivo) melhor = std::move(candidata);
    }
    return melhor;
}
}

int main() {
    try {
        const std::string pasta = "output/melhorias";
        std::filesystem::create_directories(pasta);
        std::ofstream csv(pasta + "/resultados.csv");
        csv << "instancia,semente,modo,base,final,segundos,propostas,viaveis,segundos_avaliador\n" << std::setprecision(17);
        for (std::string nome : {"alliances", "reliability", "strikes"}) {
            auto inst = lerInstancia("instancia/" + nome + ".json");
            for (unsigned seed : {7u, 42u, 2026u, 11u, 19u}) {
                std::string prefixo = nome + '-' + std::to_string(seed);
                auto base = lerEntrada(inst, "output/posicao_bb_longo/" + prefixo + "-bb_depois.json");
                Cronometro reutilizacao;
                auto reuso = reconstruir(inst, base);
                csv << nome << ',' << seed << ",reuso," << base.funcaoObjetivo << ',' << reuso.funcaoObjetivo << ',' << reutilizacao.segundosDecorridos() << ",3,3,0\n";
                for (int modo : {0, 1}) {
                    semente(seed);
                    auto sol = base;
                    int propostas = 0, viaveis = 0;
                    double avaliador = 0;
                    Cronometro tempo;
                    while (tempo.segundosDecorridos() - avaliador < 0.25) {
                        auto cand = sol;
                        bool ok;
                        if (modo == 0) ok = movimentoGrupo(inst, cand, 1);
                        else {
                            auto ids = selecionarGrupo(inst, sol, gerador);
                            // Mantem um fluxo do trio e coordena as alternativas dos outros dois.
                            ids.resize(2);
                            ParametrosRefinamento p;
                            p.maximoCombinacoes = 36;
                            p.limiteSegundos = 0.02;
                            // Uma chamada pode testar varias combinacoes, todas robustas por fluxo.
                            ok = refinarGrupo(inst, cand, ids, p).melhorou;
                        }
                        ++propostas;
                        if (!ok) continue;
                        ++viaveis;
                        Cronometro afericao;
                        double custo = cand.funcaoObjetivo;
                        if (!avaliarSolucao(inst, cand) || cand.funcaoObjetivo != custo) throw std::runtime_error("Cache incorreto");
                        avaliador += afericao.segundosDecorridos();
                        if (custo < sol.funcaoObjetivo) sol = std::move(cand);
                    }
                    std::string tipo = modo == 0 ? "aleatorio" : "grupo_bb";
                    csv << nome << ',' << seed << ',' << tipo << ',' << base.funcaoObjetivo << ',' << sol.funcaoObjetivo << ',' << tempo.segundosDecorridos() - avaliador << ',' << propostas << ',' << viaveis << ',' << avaliador << '\n';
                    salvarSolucao(inst, sol, pasta + '/' + prefixo + '-' + tipo + ".json");
                }
                salvarSolucao(inst, reuso, pasta + '/' + prefixo + "-reuso.json");
            }
        }
        csv.close();
        if (!csv) throw std::runtime_error("Falha ao gravar resultados");
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }

    return 0;
}
