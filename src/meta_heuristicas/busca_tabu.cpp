#include "meta_heuristicas/busca_tabu.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <stdexcept>

namespace {
constexpr std::uint64_t inicioHash = 14695981039346656037ULL;
std::uint64_t misturar(std::uint64_t h, std::uint64_t v) { return (h ^ v) * 1099511628211ULL; }
int quantidade(const AlocacaoFluxo &a, int rota) {
    if (a.rota1 == rota) return a.quantidade1;
    if (a.rota2 == rota) return a.quantidade2;
    return 0;
}
struct Restricao { int fluxo, rota, quantidade; Inteiro ate; }; // quantidade=-1 proibe reentrada da rota.
struct RestricaoReuso { int origem, destino, quantidade; Inteiro ate; }; // quantidade=-1 proibe recriar o par.
int quantidadeReuso(const Solucao &sol, int origem, int destino) {
    for (const auto &y : sol.reutilizacoes) if (y.rotaOrigem == origem && y.rotaDestino == destino) return y.quantidade;
    return 0;
}
bool mesmaAlocacao(const Solucao &a, const Solucao &b) {
    for (std::size_t f = 0; f < a.alocacoes.size(); ++f) {
        const auto &x = a.alocacoes[f], &y = b.alocacoes[f];
        if (x.rota1 != y.rota1 || x.rota2 != y.rota2 || x.quantidade1 != y.quantidade1 || x.quantidade2 != y.quantidade2) return false;
    }
    return true;
}
// Movimentos de reutilizacao nao alteram x, entao nao geravam atributo tabu: so o hash recente os contia.
// So valem para movimentos puros de y; quando x muda, ajustarReutilizacoes reduz y como efeito colateral.
bool proibidoReuso(const Solucao &atual, const Solucao &vizinho, int iter, const std::vector<RestricaoReuso> &lista) {
    if (lista.empty() || !mesmaAlocacao(atual, vizinho)) return false;
    for (const auto &t : lista) {
        if (t.ate < iter) continue;
        int antes = quantidadeReuso(atual, t.origem, t.destino), depois = quantidadeReuso(vizinho, t.origem, t.destino);
        if (antes == depois) continue;
        if ((t.quantidade == -1 && antes == 0 && depois > 0) || depois == t.quantidade) return true;
    }
    return false;
}
bool proibido(const Solucao &atual, const Solucao &vizinho, int iter, const std::vector<Restricao> &lista) {
    for (const auto &t : lista) {
        int antes = quantidade(atual.alocacoes[t.fluxo], t.rota), depois = quantidade(vizinho.alocacoes[t.fluxo], t.rota);
        if (t.ate < iter || antes == depois) continue;
        if ((t.quantidade == -1 && antes == 0 && depois > 0) || depois == t.quantidade) return true;
    }
    return false;
}
bool propor(const Instancia &inst, Solucao &sol) {
    int tipo = inteiroAleatorio(0, 7);
    if (tipo == 6) return movimentoGrupo(inst, sol, 1);
    using Operador = bool (*)(const Instancia &, Solucao &, int);
    static const Operador ops[] = {movimentoAleatorio, movimentoDivisao, movimentoSubstituicao, movimentoCompanhia, movimentoSite, movimentoReutilizacao, nullptr, movimentoQuantidade};
    return ops[tipo](inst, sol, 1);
}
}

std::uint64_t hashTabu(const Solucao &sol) {
    std::uint64_t h = inicioHash, reuso = 0;
    for (auto a : sol.alocacoes) {
        if (a.rota2 >= 0 && a.rota1 > a.rota2) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
        for (int v : {a.rota1, a.rota2, a.quantidade1, a.quantidade2}) h = misturar(h, static_cast<std::uint64_t>(v));
    }
    for (const auto &y : sol.reutilizacoes) {
        auto par = inicioHash;
        for (int v : {y.rotaOrigem, y.rotaDestino, y.quantidade}) par = misturar(par, static_cast<std::uint64_t>(v));
        reuso += par;
    }
    return misturar(h, reuso);
}

void validarParametrosTabu(const ParametrosTabu &p) {
    if (p.tamanhoLista < 1 || p.tamanhoVizinhanca < 1 || p.maxIteracoes < 0 || p.maxPropostas < 0 || !std::isfinite(p.tempoMax) || p.tempoMax < 0) throw std::invalid_argument("Parametros da busca tabu invalidos");
}

double buscaTabu(const Instancia &inst, Solucao &sol, const ParametrosTabu &p, EstatisticasTabu *estatisticas) {
    Cronometro tempo;
    EstatisticasTabu dados;
    if (estatisticas) *estatisticas = {};
    validarParametrosTabu(p);
    Solucao atual = sol;
    atual.permitirInviavel = false; // So vizinhos viaveis sao explorados; a incumbente devolvida e sempre viavel.
    std::string motivo;
    if (!sol.viavel || !std::isfinite(sol.funcaoObjetivo) || !avaliarSolucao(inst, atual, &motivo)) throw std::invalid_argument("Tabu exige solucao viavel: " + motivo);
    Solucao melhor = atual;
    dados.custoInicial = atual.funcaoObjetivo;
    std::vector<Restricao> lista;
    std::vector<RestricaoReuso> listaReuso;
    std::deque<std::uint64_t> recentes{hashTabu(atual)};
    for (int iter = 0; iter < p.maxIteracoes && dados.propostas < p.maxPropostas && tempo.segundosDecorridos() < p.tempoMax; ++iter) {
        ++dados.iteracoes;
        Solucao escolhido;
        bool encontrou = false, aspirou = false;
        auto hashAtual = hashTabu(atual);
        lista.erase(std::remove_if(lista.begin(), lista.end(), [&](const Restricao &t) { return t.ate < iter; }), lista.end());
        listaReuso.erase(std::remove_if(listaReuso.begin(), listaReuso.end(), [&](const RestricaoReuso &t) { return t.ate < iter; }), listaReuso.end());
        int bloqueadosIter = 0;
        for (int k = 0; k < p.tamanhoVizinhanca && dados.propostas < p.maxPropostas && tempo.segundosDecorridos() < p.tempoMax; ++k) {
            ++dados.propostas;
            Solucao v = atual;
            if (!propor(inst, v)) continue;
            if (v.viavel) ++dados.viaveis;
            auto hash = hashTabu(v);
            if (hash == hashAtual) continue; // Troca da ordem de slots nao e um novo estado.
            bool tabu = proibido(atual, v, iter, lista) || proibidoReuso(atual, v, iter, listaReuso) || std::find(recentes.begin(), recentes.end(), hash) != recentes.end();
            if (tabu && v.funcaoObjetivo >= melhor.funcaoObjetivo) { ++dados.bloqueados; ++bloqueadosIter; continue; }
            if (!encontrou || v.funcaoObjetivo < escolhido.funcaoObjetivo) { escolhido = std::move(v); encontrou = true; aspirou = tabu; }
        }
        if (!encontrou) {
            // Sem vizinho viavel (amostra ruim) a memoria e mantida; so um bloqueio tabu real justifica limpa-la.
            if (bloqueadosIter > 0) { lista.clear(); listaReuso.clear(); recentes = {hashAtual}; ++dados.reinicios; }
            continue;
        }
        Inteiro duracao = std::uniform_int_distribution<Inteiro>(std::max(1, p.tamanhoLista - p.tamanhoLista / 5), Inteiro(p.tamanhoLista) + p.tamanhoLista / 5)(gerador);
        for (std::size_t f = 0; f < atual.alocacoes.size(); ++f) for (int r : {atual.alocacoes[f].rota1, atual.alocacoes[f].rota2}) {
            if (r < 0) continue;
            int antes = quantidade(atual.alocacoes[f], r), depois = quantidade(escolhido.alocacoes[f], r);
            if (antes == depois) continue;
            int valor = antes;
            if (depois == 0) valor = -1;
            lista.push_back({static_cast<int>(f), r, valor, Inteiro(iter) + duracao});
        }
        if (mesmaAlocacao(atual, escolhido)) {
            for (const auto &y : atual.reutilizacoes) {
                int depois = quantidadeReuso(escolhido, y.rotaOrigem, y.rotaDestino);
                if (depois != y.quantidade) listaReuso.push_back({y.rotaOrigem, y.rotaDestino, depois == 0 ? -1 : y.quantidade, Inteiro(iter) + duracao});
            }
        }
        if (recentes.size() >= static_cast<std::size_t>(p.tamanhoLista)) recentes.pop_front();
        recentes.push_back(hashTabu(escolhido));
        atual = std::move(escolhido); ++dados.aceitos; dados.aspiracoes += aspirou;
        if (atual.viavel && atual.funcaoObjetivo < melhor.funcaoObjetivo) melhor = atual;
    }
    dados.custoCorrente = atual.funcaoObjetivo; dados.melhorCusto = melhor.funcaoObjetivo;
    dados.tempoExecucao = tempo.segundosDecorridos();
    sol = std::move(melhor);
    if (estatisticas) *estatisticas = dados;

    return sol.funcaoObjetivo;
}

double busca_tabu(const Instancia &inst, Solucao &sol, int tamanhoLista, int tamanhoVizinhanca, double tempoMax) {
    ParametrosTabu p;
    p.tamanhoLista = tamanhoLista; p.tamanhoVizinhanca = tamanhoVizinhanca; p.tempoMax = tempoMax;

    return buscaTabu(inst, sol, p);
}
