#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include <algorithm>
#include <limits>

namespace {
bool rejeitar(std::string *motivo, const std::string &texto) {
    if (motivo) *motivo = texto;
    return false;
}
bool existeFluxo(const Instancia &inst, const Solucao &sol, int fluxo) {
    return fluxo >= 0 && fluxo < static_cast<int>(inst.fluxos.size()) && sol.alocacoes.size() == inst.fluxos.size();
}
bool confirmar(const Instancia &inst, Solucao &sol, Solucao &candidata, std::string *motivo) {
    if (!avaliarSolucao(inst, candidata, motivo)) return false;
    sol = std::move(candidata);
    return true;
}
void ajustarReutilizacoes(const Instancia &inst, Solucao &sol) {
    std::vector<Inteiro> entrada(inst.rotas.size(), 0), saida(inst.rotas.size(), 0);
    for (const auto &a : sol.alocacoes) {
        if (a.rota1 >= 0 && a.rota1 < static_cast<int>(entrada.size())) entrada[a.rota1] = saida[a.rota1] = a.quantidade1;
        if (a.rota2 >= 0 && a.rota2 < static_cast<int>(entrada.size())) entrada[a.rota2] = saida[a.rota2] = a.quantidade2;
    }
    for (auto &y : sol.reutilizacoes) {
        if (y.rotaOrigem < 0 || y.rotaOrigem >= static_cast<int>(saida.size()) || y.rotaDestino < 0 || y.rotaDestino >= static_cast<int>(entrada.size()) || y.quantidade <= 0) continue;
        y.quantidade = static_cast<int>(std::min({Inteiro(y.quantidade), saida[y.rotaOrigem], entrada[y.rotaDestino]}));
        saida[y.rotaOrigem] -= y.quantidade;
        entrada[y.rotaDestino] -= y.quantidade;
    }
    sol.reutilizacoes.erase(std::remove_if(sol.reutilizacoes.begin(), sol.reutilizacoes.end(), [](const Reutilizacao &y) { return y.quantidade == 0; }), sol.reutilizacoes.end());
}
}

bool parRobusto(const Instancia &inst, int rota1, int rota2) {
    const int nr = static_cast<int>(inst.rotas.size());
    if (rota1 < 0 || rota1 >= nr || rota2 < -1 || rota2 >= nr) return false;
    if (rota2 >= 0 && (rota1 == rota2 || inst.rotas[rota1].fluxo != inst.rotas[rota2].fluxo)) return false;
    for (const auto &s : inst.cenarios) if (inst.indisponivel[s.id][rota1] && (rota2 == -1 || inst.indisponivel[s.id][rota2])) return false;

    return !inst.cenarios.empty();
}

bool alterarAlocacoes(const Instancia &inst, Solucao &sol, const std::vector<std::pair<int, AlocacaoFluxo>> &alteracoes, std::string *motivo) {
    if (motivo) motivo->clear();
    if (alteracoes.empty()) return rejeitar(motivo, "Nenhuma alteracao informada");
    Solucao candidata = sol;
    std::vector<unsigned char> alterado(inst.fluxos.size(), 0);
    bool mudou = false;
    for (const auto &item : alteracoes) {
        int f = item.first;
        AlocacaoFluxo a = item.second;
        if (!existeFluxo(inst, sol, f) || alterado[f]) return rejeitar(motivo, "Fluxo invalido ou repetido");
        alterado[f] = 1;
        if (a.quantidade1 < 0 || a.quantidade2 < 0 || Inteiro(a.quantidade1) + a.quantidade2 != inst.fluxos[f].demanda) return rejeitar(motivo, "Divisao de demanda invalida");
        if (a.quantidade1 == 0) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
        if (a.quantidade2 == 0) a.rota2 = -1;
        if (!parRobusto(inst, a.rota1, a.rota2) || inst.rotas[a.rota1].fluxo != f) return rejeitar(motivo, "Rotas invalidas ou sem cobertura robusta");
        const auto &anterior = sol.alocacoes[f];
        if (a.rota1 != anterior.rota1 || a.rota2 != anterior.rota2 || a.quantidade1 != anterior.quantidade1 || a.quantidade2 != anterior.quantidade2) mudou = true;
        candidata.alocacoes[f] = a;
    }
    if (!mudou) return rejeitar(motivo, "Alocacoes sem alteracao");
    ajustarReutilizacoes(inst, candidata);

    return confirmar(inst, sol, candidata, motivo);
}

bool alterarAlocacao(const Instancia &inst, Solucao &sol, int fluxo, AlocacaoFluxo alocacao, std::string *motivo) {
    return alterarAlocacoes(inst, sol, {{fluxo, alocacao}}, motivo);
}

bool transferirQuantidade(const Instancia &inst, Solucao &sol, int fluxo, int delta, std::string *motivo) {
    if (!existeFluxo(inst, sol, fluxo) || delta == 0) return rejeitar(motivo, "Fluxo ou delta invalido");
    AlocacaoFluxo a = sol.alocacoes[fluxo];
    Inteiro q1 = Inteiro(a.quantidade1) + delta, q2 = Inteiro(a.quantidade2) - delta;
    if (a.rota2 < 0 || q1 < 0 || q2 < 0 || q1 > std::numeric_limits<int>::max() || q2 > std::numeric_limits<int>::max()) return rejeitar(motivo, "Transferencia fora dos limites");
    a.quantidade1 = static_cast<int>(q1); a.quantidade2 = static_cast<int>(q2);

    return alterarAlocacao(inst, sol, fluxo, a, motivo);
}

bool substituirRota(const Instancia &inst, Solucao &sol, int fluxo, int posicao, int novaRota, std::string *motivo) {
    if (!existeFluxo(inst, sol, fluxo) || (posicao != 1 && posicao != 2)) return rejeitar(motivo, "Fluxo ou posicao invalida");
    AlocacaoFluxo a = sol.alocacoes[fluxo];
    if (posicao == 1) a.rota1 = novaRota;
    else {
        if (a.quantidade2 == 0) return rejeitar(motivo, "Segunda rota sem alocacao");
        a.rota2 = novaRota;
    }

    return alterarAlocacao(inst, sol, fluxo, a, motivo);
}

bool definirReutilizacao(const Instancia &inst, Solucao &sol, int origem, int destino, int quantidade, std::string *motivo) {
    if (motivo) motivo->clear();
    if (quantidade < 0 || origem < 0 || destino < 0 || origem >= static_cast<int>(inst.rotas.size()) || destino >= static_cast<int>(inst.rotas.size())) return rejeitar(motivo, "Par ou quantidade invalida");
    Solucao candidata = sol;
    auto &ys = candidata.reutilizacoes;
    auto it = std::find_if(ys.begin(), ys.end(), [&](const Reutilizacao &y) { return y.rotaOrigem == origem && y.rotaDestino == destino; });
    if (it == ys.end()) {
        if (quantidade == 0) return rejeitar(motivo, "Reutilizacao inexistente");
        ys.push_back({origem, destino, quantidade});
    } else {
        if (it->quantidade == quantidade) return rejeitar(motivo, "Reutilizacao sem alteracao");
        if (quantidade == 0) ys.erase(it);
        else it->quantidade = quantidade;
    }

    return confirmar(inst, sol, candidata, motivo);
}

bool movimentoAleatorio(const Instancia &inst, Solucao &sol, int tentativas) {
    if (inst.fluxos.empty() || sol.alocacoes.size() != inst.fluxos.size()) return false;
    for (int i = 0; i < tentativas; ++i) {
        int f = inteiroAleatorio(0, static_cast<int>(inst.fluxos.size()) - 1);
        const auto &fluxo = inst.fluxos[f];
        if (fluxo.rotas.empty() || fluxo.demanda > std::numeric_limits<int>::max()) continue;
        int r1 = fluxo.rotas[inteiroAleatorio(0, static_cast<int>(fluxo.rotas.size()) - 1)];
        int r2 = fluxo.rotas[inteiroAleatorio(0, static_cast<int>(fluxo.rotas.size()) - 1)];
        int q1 = static_cast<int>(fluxo.demanda), q2 = 0;
        if (r1 != r2 && q1 > 1) { q1 = inteiroAleatorio(1, q1 - 1); q2 = static_cast<int>(fluxo.demanda) - q1; }
        else r2 = -1;
        if (alterarAlocacao(inst, sol, f, {r1, r2, q1, q2})) return true;
    }

    return false;
}

namespace {
int sorteio(int n) { return inteiroAleatorio(0, n - 1); }
int melhorRota(const Instancia &inst, int fluxo, int companhia, int excluida) {
    int melhor = -1;
    for (int r : inst.fluxos[fluxo].rotas)
        if (r != excluida && inst.rotas[r].companhia == companhia && (melhor < 0 || inst.custosRotas[r] < inst.custosRotas[melhor])) melhor = r;
    return melhor;
}
std::vector<int> fluxosDaCompanhia(const Instancia &inst, const Solucao &sol, int companhia) {
    std::vector<int> lista;
    for (const auto &a : sol.alocacoes) {
        bool usa = inst.rotas[a.rota1].companhia == companhia || (a.rota2 >= 0 && inst.rotas[a.rota2].companhia == companhia);
        if (usa) lista.push_back(static_cast<int>(&a - sol.alocacoes.data()));
    }
    return lista;
}
std::vector<int> fluxosDoSite(const Instancia &inst, int site) {
    std::vector<int> lista;
    for (const auto &f : inst.fluxos) if (f.origem == site || f.destino == site) lista.push_back(f.id);
    return lista;
}
bool prontoParaSortear(const Instancia &inst, const Solucao &sol) { return !inst.fluxos.empty() && sol.alocacoes.size() == inst.fluxos.size(); }
}

bool redividirFluxo(const Instancia &inst, Solucao &sol, int fluxo, int rotaA, int rotaB, int quantidadeA, std::string *motivo) {
    if (!existeFluxo(inst, sol, fluxo) || inst.fluxos[fluxo].demanda > std::numeric_limits<int>::max()) return rejeitar(motivo, "Fluxo invalido");
    int demanda = static_cast<int>(inst.fluxos[fluxo].demanda);
    if (quantidadeA < 0 || quantidadeA > demanda) return rejeitar(motivo, "Quantidade fora da demanda");

    return alterarAlocacao(inst, sol, fluxo, {rotaA, rotaB, quantidadeA, demanda - quantidadeA}, motivo);
}

bool substituirPar(const Instancia &inst, Solucao &sol, int fluxo, int novaRota1, int novaRota2, std::string *motivo) {
    if (!existeFluxo(inst, sol, fluxo)) return rejeitar(motivo, "Fluxo invalido");
    AlocacaoFluxo a = sol.alocacoes[fluxo];
    if (novaRota1 >= 0) a.rota1 = novaRota1;
    if (novaRota2 >= 0 && a.quantidade2 > 0) a.rota2 = novaRota2;

    return alterarAlocacao(inst, sol, fluxo, a, motivo);
}

bool migrarFluxos(const Instancia &inst, Solucao &sol, const std::vector<int> &fluxos, int de, int para, std::string *motivo) {
    if (para < 0 || para >= static_cast<int>(inst.companhias.size()) || de >= static_cast<int>(inst.companhias.size())) return rejeitar(motivo, "Companhia invalida");
    std::vector<std::pair<int, AlocacaoFluxo>> alteracoes;
    for (int f : fluxos) {
        if (!existeFluxo(inst, sol, f)) return rejeitar(motivo, "Fluxo invalido");
        AlocacaoFluxo a = sol.alocacoes[f];
        auto trocar = [&](int &rota, int outra) {
            int m = inst.rotas[rota].companhia, nova = (m == para || (de >= 0 && m != de)) ? -1 : melhorRota(inst, f, para, outra);
            if (nova >= 0) rota = nova;
        };
        trocar(a.rota1, a.rota2);
        if (a.quantidade2 > 0) trocar(a.rota2, a.rota1);
        if (a.rota1 != sol.alocacoes[f].rota1 || a.rota2 != sol.alocacoes[f].rota2) alteracoes.push_back({f, a});
    }
    if (alteracoes.empty()) return rejeitar(motivo, "Nenhum fluxo pode migrar");

    return alterarAlocacoes(inst, sol, alteracoes, motivo);
}

bool trocarCompanhia(const Instancia &inst, Solucao &sol, int de, int para, std::string *motivo) {
    if (de < 0 || de >= static_cast<int>(inst.companhias.size()) || sol.alocacoes.size() != inst.fluxos.size()) return rejeitar(motivo, "Companhia ou solucao invalida");

    return migrarFluxos(inst, sol, fluxosDaCompanhia(inst, sol, de), de, para, motivo);
}

bool unificarSite(const Instancia &inst, Solucao &sol, int site, int companhia, std::string *motivo) {
    if (site < 0 || site >= static_cast<int>(inst.sites.size())) return rejeitar(motivo, "Site invalido");

    return migrarFluxos(inst, sol, fluxosDoSite(inst, site), -1, companhia, motivo);
}

bool movimentoDivisao(const Instancia &inst, Solucao &sol, int tentativas) {
    if (!prontoParaSortear(inst, sol)) return false;
    for (int i = 0; i < tentativas; ++i) {
        int f = sorteio(static_cast<int>(inst.fluxos.size()));
        const auto &fluxo = inst.fluxos[f];
        const auto &a = sol.alocacoes[f];
        if (fluxo.rotas.size() < 2 || fluxo.demanda > std::numeric_limits<int>::max()) continue;
        int rotaB = a.rota2 >= 0 ? a.rota2 : fluxo.rotas[sorteio(static_cast<int>(fluxo.rotas.size()))];
        if (rotaB != a.rota1 && redividirFluxo(inst, sol, f, a.rota1, rotaB, inteiroAleatorio(0, static_cast<int>(fluxo.demanda)))) return true;
    }

    return false;
}

bool movimentoSubstituicao(const Instancia &inst, Solucao &sol, int tentativas) {
    if (!prontoParaSortear(inst, sol)) return false;
    for (int i = 0; i < tentativas; ++i) {
        int f = sorteio(static_cast<int>(inst.fluxos.size())), modo = sorteio(3); // 0: rota 1, 1: rota 2, 2: par.
        const auto &rotas = inst.fluxos[f].rotas;
        if (rotas.empty()) continue;
        int n1 = modo != 1 ? rotas[sorteio(static_cast<int>(rotas.size()))] : -1, n2 = modo != 0 ? rotas[sorteio(static_cast<int>(rotas.size()))] : -1;
        if (substituirPar(inst, sol, f, n1, n2)) return true;
    }

    return false;
}

bool movimentoCompanhia(const Instancia &inst, Solucao &sol, int tentativas) {
    if (!prontoParaSortear(inst, sol)) return false;
    for (int i = 0; i < tentativas; ++i) {
        int f = sorteio(static_cast<int>(inst.fluxos.size())), de = inst.rotas[sol.alocacoes[f].rota1].companhia;
        if (inst.fluxos[f].rotas.empty()) continue;
        int para = inst.rotas[inst.fluxos[f].rotas[sorteio(static_cast<int>(inst.fluxos[f].rotas.size()))]].companhia;
        auto lista = fluxosDaCompanhia(inst, sol, de);
        embaralhaVetor(lista);
        lista.resize(inteiroAleatorio(1, static_cast<int>(lista.size()))); // Subconjunto dos fluxos da companhia.
        if (para != de && migrarFluxos(inst, sol, lista, de, para)) return true;
    }

    return false;
}

bool movimentoSite(const Instancia &inst, Solucao &sol, int tentativas) {
    if (!prontoParaSortear(inst, sol) || inst.sites.empty()) return false;
    for (int i = 0; i < tentativas; ++i) {
        int site = sorteio(static_cast<int>(inst.sites.size()));
        auto lista = fluxosDoSite(inst, site);
        if (lista.empty()) continue;
        const auto &rotas = inst.fluxos[lista[sorteio(static_cast<int>(lista.size()))]].rotas;
        if (!rotas.empty() && unificarSite(inst, sol, site, inst.rotas[rotas[sorteio(static_cast<int>(rotas.size()))]].companhia)) return true;
    }

    return false;
}