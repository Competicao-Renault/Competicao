#include "refinamento/refinamento.hpp"
#include "refinamento/movimentos.hpp"
#include "utilitarios/cronometro.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
void exigir(bool condicao, const char *mensagem) {
    if (!condicao) throw std::invalid_argument(mensagem);
}
Solucao entradaValidada(const Instancia &inst, const Solucao &sol) {
    Solucao base = sol;
    std::string motivo;
    if (!avaliarSolucao(inst, base, &motivo)) throw std::invalid_argument("Refinamento exige solucao viavel: " + motivo);

    return base;
}
void validarParametros(const ParametrosRefinamento &p) {
    exigir(p.maximoAlternativas >= 2 && p.maximoCombinacoes > 0 && std::isfinite(p.limiteSegundos) && p.limiteSegundos > 0, "Limites de refinamento invalidos");
}
void validarFluxos(const Instancia &inst, const std::vector<int> &fluxos) {
    exigir(fluxos.size() == 2 || fluxos.size() == 3, "O grupo deve ter dois ou tres fluxos");
    std::vector<int> ids = fluxos;
    std::sort(ids.begin(), ids.end());
    exigir(ids.front() >= 0 && ids.back() < static_cast<int>(inst.fluxos.size()) && std::adjacent_find(ids.begin(), ids.end()) == ids.end(), "Fluxos invalidos ou repetidos");
}
bool iguais(const AlocacaoFluxo &a, const AlocacaoFluxo &b) {
    return a.rota1 == b.rota1 && a.rota2 == b.rota2 && a.quantidade1 == b.quantidade1 && a.quantidade2 == b.quantidade2;
}
AlocacaoFluxo normalizar(const Instancia &inst, int f, AlocacaoFluxo a) {
    exigir(a.quantidade1 >= 0 && a.quantidade2 >= 0 && Inteiro(a.quantidade1) + a.quantidade2 == inst.fluxos[f].demanda, "Quantidade invalida na alternativa");
    if (a.quantidade1 == 0) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }
    if (a.quantidade2 == 0) a.rota2 = -1;
    exigir(parRobusto(inst, a.rota1, a.rota2) && inst.rotas[a.rota1].fluxo == f && (a.quantidade2 == 0 || a.rota2 >= 0), "Alternativa sem rotas validas e robustas");
    if (a.rota2 >= 0 && a.rota1 > a.rota2) { std::swap(a.rota1, a.rota2); std::swap(a.quantidade1, a.quantidade2); }

    return a;
}
void incluir(std::vector<AlocacaoFluxo> &lista, const AlocacaoFluxo &a) {
    if (std::none_of(lista.begin(), lista.end(), [&](const AlocacaoFluxo &b) { return iguais(a, b); })) lista.push_back(a);
}
// Intersecta os limites de delta=q-qAtual com minimo <= carga+coef*delta <= maximo.
void limitar(Inteiro carga, int coef, Inteiro minimo, Inteiro maximo, Inteiro &inferior, Inteiro &superior) {
    if (coef == 0) return; // A entrada ja e viavel e esta carga nao muda.
    Inteiro a = minimo - carga, b = maximo - carga;
    if (coef < 0) { coef = -coef; std::swap(a, b); a = -a; b = -b; }
    Inteiro teto = a / coef, piso = b / coef;
    if (a % coef > 0) ++teto;
    if (b % coef < 0) --piso;
    inferior = std::max(inferior, teto); superior = std::min(superior, piso);
}
bool instalar(Solucao &sol, Solucao &melhor, double custoInicial) {
    if (melhor.funcaoObjetivo >= custoInicial) return false;
    sol = std::move(melhor);
    return true;
}
ResultadoRefinamento enumerar(const Instancia &inst, Solucao &sol, const Solucao &base, const std::vector<AlternativasFluxo> &grupo, const ParametrosRefinamento &p, const Cronometro &tempo) {
    ResultadoRefinamento resultado;
    Solucao melhor = base;
    std::vector<std::pair<int, AlocacaoFluxo>> alteracoes(grupo.size());
    auto visitar = [&](auto &&visitar, std::size_t nivel) -> void {
        if (!resultado.concluido) return;
        if (resultado.candidatosTestados >= p.maximoCombinacoes || tempo.segundosDecorridos() >= p.limiteSegundos) { resultado.concluido = false; return; }
        if (nivel == grupo.size()) {
            ++resultado.candidatosTestados;
            Solucao candidata = base; // Nao valida estados parciais nem encadeia candidatos.
            if (alterarAlocacoes(inst, candidata, alteracoes) && candidata.funcaoObjetivo < melhor.funcaoObjetivo) melhor = std::move(candidata);
            return;
        }
        for (const auto &a : grupo[nivel].alocacoes) {
            alteracoes[nivel] = {grupo[nivel].fluxo, a};
            visitar(visitar, nivel + 1);
            if (!resultado.concluido) break;
        }
    };
    visitar(visitar, 0);
    resultado.melhorou = instalar(sol, melhor, base.funcaoObjetivo);

    return resultado;
}
}

ResultadoRefinamento refinarQuantidade(const Instancia &inst, Solucao &sol, int fluxo) {
    exigir(fluxo >= 0 && fluxo < static_cast<int>(inst.fluxos.size()), "Fluxo inexistente");
    const Solucao base = entradaValidada(inst, sol);
    const auto &a = base.alocacoes[fluxo];
    ResultadoRefinamento resultado;
    if (a.rota2 < 0) return resultado;
    Solucao melhor = base;
    const auto &r1 = inst.rotas[a.rota1], &r2 = inst.rotas[a.rota2];
    Inteiro n = inst.fluxos[fluxo].demanda, inferior = 1 - Inteiro(a.quantidade1), superior = n - 1 - a.quantidade1;
    Inteiro entrada1 = 0, saida1 = 0, entrada2 = 0, saida2 = 0;
    for (const auto &y : base.reutilizacoes) {
        if (y.rotaDestino == a.rota1) entrada1 += y.quantidade;
        if (y.rotaOrigem == a.rota1) saida1 += y.quantidade;
        if (y.rotaDestino == a.rota2) entrada2 += y.quantidade;
        if (y.rotaOrigem == a.rota2) saida2 += y.quantidade;
    }
    inferior = std::max(inferior, std::max(entrada1, saida1) - a.quantidade1);
    superior = std::min(superior, n - std::max(entrada2, saida2) - a.quantidade1);
    std::vector<int> coefPortos(inst.portos.size(), 0), coefCompanhias(inst.companhias.size(), 0);
    ++coefPortos[r1.portoOrigem]; ++coefPortos[r1.portoDestino];
    --coefPortos[r2.portoOrigem]; --coefPortos[r2.portoDestino];
    ++coefCompanhias[r1.companhia]; --coefCompanhias[r2.companhia];
    for (const auto &s : inst.cenarios) {
        const auto &falha = inst.indisponivel[s.id];
        if (falha[a.rota1] || falha[a.rota2]) continue; // Com report, q nao altera cargas.
        std::vector<Inteiro> portos(inst.portos.size(), 0), companhias(inst.companhias.size(), 0);
        auto somar = [&](int rota, Inteiro quantidade) {
            const auto &r = inst.rotas[rota];
            portos[r.portoOrigem] += quantidade; portos[r.portoDestino] += quantidade;
            companhias[r.companhia] += quantidade;
        };
        for (const auto &f : inst.fluxos) {
            const auto &v = base.alocacoes[f.id];
            if (falha[v.rota1]) somar(v.rota2, f.demanda);
            else if (v.rota2 >= 0 && falha[v.rota2]) somar(v.rota1, f.demanda);
            else { somar(v.rota1, v.quantidade1); if (v.rota2 >= 0) somar(v.rota2, v.quantidade2); }
        }
        for (const auto &p : inst.portos) limitar(portos[p.id], coefPortos[p.id], 0, p.capacidade, inferior, superior);
        for (const auto &m : inst.companhias) {
            Inteiro minimo = 0;
            if (m.id != inst.companhiaSpot && companhias[m.id] > 0) minimo = static_cast<Inteiro>(std::ceil(static_cast<long double>(inst.meta.alpha) * m.capacidade - 1e-9L));
            limitar(companhias[m.id], coefCompanhias[m.id], minimo, m.capacidade, inferior, superior);
        }
    }
    // No interior, contratos e y sao constantes; o custo por cenario tem
    // inclinacao custo(r1)-custo(r2), ou zero quando uma das rotas falha.
    Inteiro delta = 0;
    if (inst.custosRotas[a.rota1] < inst.custosRotas[a.rota2]) delta = superior;
    else if (inst.custosRotas[a.rota1] > inst.custosRotas[a.rota2]) delta = inferior;
    if (delta != 0) {
        Solucao candidata = base;
        auto &v = candidata.alocacoes[fluxo];
        v.quantidade1 += static_cast<int>(delta); v.quantidade2 -= static_cast<int>(delta);
        ++resultado.candidatosTestados;
        if (avaliarSolucao(inst, candidata) && candidata.funcaoObjetivo < melhor.funcaoObjetivo) melhor = std::move(candidata);
    }
    // Em q=0 ou q=n, os contratos podem desaparecer e reutilizacoes sao ajustadas.
    for (int rota : {a.rota1, a.rota2}) {
        Solucao candidata = base;
        ++resultado.candidatosTestados;
        if (alterarAlocacao(inst, candidata, fluxo, {rota, -1, static_cast<int>(n), 0}) && candidata.funcaoObjetivo < melhor.funcaoObjetivo) melhor = std::move(candidata);
    }
    resultado.melhorou = instalar(sol, melhor, base.funcaoObjetivo);

    return resultado;
}

ResultadoRefinamento enumerarGrupo(const Instancia &inst, Solucao &sol, const std::vector<AlternativasFluxo> &grupo, const ParametrosRefinamento &p) {
    Cronometro tempo;
    validarParametros(p);
    std::vector<int> fluxos;
    for (const auto &g : grupo) fluxos.push_back(g.fluxo);
    validarFluxos(inst, fluxos);
    const Solucao base = entradaValidada(inst, sol);
    std::vector<AlternativasFluxo> candidatos;
    for (const auto &g : grupo) {
        AlternativasFluxo lista{g.fluxo, {normalizar(inst, g.fluxo, base.alocacoes[g.fluxo])}};
        for (const auto &a : g.alocacoes) incluir(lista.alocacoes, normalizar(inst, g.fluxo, a));
        exigir(lista.alocacoes.size() <= static_cast<std::size_t>(p.maximoAlternativas), "Lista excede maximoAlternativas, incluindo a atual");
        candidatos.push_back(std::move(lista));
    }

    return enumerar(inst, sol, base, candidatos, p, tempo);
}

ResultadoRefinamento refinarGrupo(const Instancia &inst, Solucao &sol, const std::vector<int> &fluxos, const ParametrosRefinamento &p) {
    Cronometro tempo;
    validarParametros(p); validarFluxos(inst, fluxos);
    const Solucao base = entradaValidada(inst, sol);
    std::vector<AlternativasFluxo> grupo;
    for (int f : fluxos) {
        const auto &fluxo = inst.fluxos[f];
        int n = static_cast<int>(fluxo.demanda);
        AlternativasFluxo lista{f, {normalizar(inst, f, base.alocacoes[f])}};
        for (int r : fluxo.rotas) if (inst.rotas[r].companhia == inst.companhiaSpot) incluir(lista.alocacoes, {r, -1, n, 0});
        std::vector<std::pair<double, AlocacaoFluxo>> opcoes;
        auto adicionar = [&](AlocacaoFluxo a) {
            a = normalizar(inst, f, a);
            double pior = 0;
            for (const auto &s : inst.cenarios) {
                const auto &falha = inst.indisponivel[s.id];
                double custo = static_cast<double>(inst.custosRotas[a.rota1]) * a.quantidade1;
                if (a.rota2 >= 0) {
                    custo += static_cast<double>(inst.custosRotas[a.rota2]) * a.quantidade2;
                    if (falha[a.rota1]) custo = static_cast<double>(inst.custosRotas[a.rota2]) * n;
                    else if (falha[a.rota2]) custo = static_cast<double>(inst.custosRotas[a.rota1]) * n;
                }
                pior = std::max(pior, custo);
            }
            opcoes.push_back({pior, a});
        };
        for (std::size_t i = 0; i < fluxo.rotas.size(); ++i) {
            if (tempo.segundosDecorridos() >= p.limiteSegundos) { ResultadoRefinamento r; r.concluido = false; return r; }
            int r1 = fluxo.rotas[i];
            if (parRobusto(inst, r1)) adicionar({r1, -1, n, 0});
            for (std::size_t j = i + 1; j < fluxo.rotas.size(); ++j) {
                int r2 = fluxo.rotas[j];
                if (n < 2 || !parRobusto(inst, r1, r2)) continue;
                for (int percentual : {25, 50, 75}) {
                    int q = static_cast<int>(Inteiro(n) * percentual / 100);
                    q = std::max(1, std::min(n - 1, q));
                    adicionar({r1, r2, q, n - q});
                }
            }
        }
        std::stable_sort(opcoes.begin(), opcoes.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &opcao : opcoes) {
            if (lista.alocacoes.size() >= static_cast<std::size_t>(p.maximoAlternativas)) break;
            incluir(lista.alocacoes, opcao.second);
        }
        grupo.push_back(std::move(lista));
    }

    return enumerar(inst, sol, base, grupo, p, tempo);
}
