#include "meta_heuristicas/busca_tabu.hpp"
#include "definicao_problema/solucao.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/aleatorio.hpp"
#include "utilitarios/cronometro.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

// Tipo de movimento que originou a restrição tabu.
enum class TipoMovimento { Nenhum, Substituicao, Divisao, Companhia, Site };

// Uma entrada da lista tabu: "no fluxo 'alvo', não volte a usar 'proibido' até a iteração duracaoTabu".
//   Divisao:                         proibido = quantidade1 anterior (não pode voltar à divisão antiga)
//   Substituicao / Companhia / Site: proibido = rota que saiu do fluxo (não pode reentrar)
typedef struct tipoTabu{
    TipoMovimento tipo = TipoMovimento::Nenhum;
    int alvo = -1;
    int proibido = -1;
    int duracaoTabu = 0;
}tipoTabu;

static uint64_t misturaHash(uint64_t hash, uint64_t valor){
    hash ^= valor;
    return hash * 1099511628211ULL; // primo do FNV-1a
}

// Hash canônico: par de rotas ordenado (mesma convenção de refinamento.cpp) e
// reutilizações combinadas por soma, para não depender da ordem do vetor.
uint64_t hashTabu(const Solucao &s){
    uint64_t hash = 14695981039346656037ULL; // offset do FNV-1a

    for(AlocacaoFluxo a : s.alocacoes){
        if(a.rota2 >= 0 && a.rota1 > a.rota2){
            std::swap(a.rota1, a.rota2);
            std::swap(a.quantidade1, a.quantidade2);
        }
        for(int v : {a.rota1, a.rota2, a.quantidade1, a.quantidade2})
            hash = misturaHash(hash, static_cast<uint64_t>(v));
    }

    uint64_t somaReuso = 0;
    for(const Reutilizacao &y : s.reutilizacoes){
        uint64_t h = 14695981039346656037ULL;
        for(int v : {y.rotaOrigem, y.rotaDestino, y.quantidade})
            h = misturaHash(h, static_cast<uint64_t>(v));
        somaReuso += h;
    }

    return misturaHash(hash, somaReuso);
}

// Um movimento tabu só é liberado se melhora o melhor global E leva a uma
// solução ainda não visitada (hash fora da lista), evitando ciclos de mesmo custo.
bool criterioAspiracao(const Solucao &sLinha, double foEstrela, const std::deque<uint64_t> &listaHash){
    if(sLinha.funcaoObjetivo >= foEstrela - 1e-9) // 1e-9 evita aceitar só por erro de arredondamento
        return false;
    return std::find(listaHash.begin(), listaHash.end(), hashTabu(sLinha)) == listaHash.end();
}

static bool igual(const AlocacaoFluxo &a, const AlocacaoFluxo &b){
    return a.rota1 == b.rota1 && a.rota2 == b.rota2 && a.quantidade1 == b.quantidade1 && a.quantidade2 == b.quantidade2;
}

static bool usaRota(const AlocacaoFluxo &a, int r){
    return a.rota1 == r || a.rota2 == r;
}

// Entradas tabu geradas ao aplicar o movimento s -> v: proíbem desfazer o que mudou.
static std::vector<tipoTabu> gerarTabus(const Solucao &s, const Solucao &v, TipoMovimento tipo, int validade){
    std::vector<tipoTabu> novos;
    for(size_t f = 0; f < s.alocacoes.size(); f++){
        const AlocacaoFluxo &a = s.alocacoes[f], &b = v.alocacoes[f];
        if(igual(a, b))
            continue;
        if(tipo == TipoMovimento::Divisao)
            novos.push_back({tipo, (int)f, a.quantidade1, validade});
        else for(int r : {a.rota1, a.rota2})
            if(r >= 0 && !usaRota(b, r))
                novos.push_back({tipo, (int)f, r, validade});
    }
    return novos;
}

// O movimento s -> v é tabu se reintroduz, em algum fluxo alterado, algo ainda proibido.
static bool movimentoTabu(const Solucao &s, const Solucao &v, int iter, const std::vector<tipoTabu> &listaTabu){
    for(const tipoTabu &t : listaTabu){
        if(t.duracaoTabu < iter)
            continue; // entrada expirada
        const AlocacaoFluxo &a = s.alocacoes[t.alvo], &b = v.alocacoes[t.alvo];
        if(igual(a, b))
            continue;
        bool volta = (t.tipo == TipoMovimento::Divisao)
                   ? (b.quantidade1 == t.proibido && a.quantidade1 != t.proibido)
                   : (usaRota(b, t.proibido) && !usaRota(a, t.proibido));
        if(volta)
            return true;
    }
    return false;
}

// Amostra 'tamanhoVizinhanca' vizinhos com movimentos aleatórios e devolve em 'melhor' o de menor
// objetivo entre os admissíveis (não tabu, ou tabu liberado pela aspiração). False se nenhum servir.
bool vizinhoMelhorTabu(const Instancia &inst, const Solucao &s, int tamanhoVizinhanca, double foEstrela, int iter,
                       const std::vector<tipoTabu> &listaTabu, const std::deque<uint64_t> &listaHash,
                       Solucao &melhor, TipoMovimento &tipoMelhor)
{
    typedef bool (*Movimento)(const Instancia &, Solucao &, int);
    static const Movimento movimentos[] = {movimentoSubstituicao, movimentoDivisao, movimentoCompanhia, movimentoSite};
    static const TipoMovimento tipos[] = {TipoMovimento::Substituicao, TipoMovimento::Divisao,
                                          TipoMovimento::Companhia, TipoMovimento::Site};
    bool achou = false;

    for(int k = 0; k < tamanhoVizinhanca; k++){
        int m = inteiroAleatorio(0, 3);
        Solucao v = s;
        if(!movimentos[m](inst, v, 30))
            continue; // nenhuma proposta viável sorteada
        if(achou && v.funcaoObjetivo >= melhor.funcaoObjetivo)
            continue; // já existe um vizinho admissível melhor
        if(movimentoTabu(s, v, iter, listaTabu) && !criterioAspiracao(v, foEstrela, listaHash))
            continue;
        melhor = std::move(v);
        tipoMelhor = tipos[m];
        achou = true;
    }
    return achou;
}

// 's' deve ser viável. Retorna o objetivo da melhor solução encontrada, que fica em 's'.
double busca_tabu(const Instancia &inst, Solucao &s, int tamanhoLista, int tamanhoVizinhanca, double tempoMax){
    Cronometro cron;
    Solucao sEstrela = s, vizinho;
    TipoMovimento tipo = TipoMovimento::Nenhum;
    std::vector<tipoTabu> listaTabu;
    std::deque<uint64_t> hashsVisitados{hashTabu(s)};

    for(int iter = 0; cron.segundosDecorridos() < tempoMax; iter++){
        if(!vizinhoMelhorTabu(inst, s, tamanhoVizinhanca, sEstrela.funcaoObjetivo, iter, listaTabu, hashsVisitados, vizinho, tipo)){
            listaTabu.clear(); // reset quando não há vizinho admissível
            continue;
        }
        int duracaoTabu = inteiroAleatorio((int)(0.8 * tamanhoLista), (int)(1.2 * tamanhoLista));
        auto novos = gerarTabus(s, vizinho, tipo, iter + duracaoTabu);
        listaTabu.erase(std::remove_if(listaTabu.begin(), listaTabu.end(),
                        [iter](const tipoTabu &t){ return t.duracaoTabu < iter; }), listaTabu.end());
        listaTabu.insert(listaTabu.end(), novos.begin(), novos.end());

        if(hashsVisitados.size() >= (size_t)tamanhoLista)
            hashsVisitados.pop_front();
        hashsVisitados.push_back(hashTabu(vizinho));

        s = std::move(vizinho);
        if(s.funcaoObjetivo < sEstrela.funcaoObjetivo - 1e-9)
            sEstrela = s;
    }

    s = sEstrela;
    return s.funcaoObjetivo;
}