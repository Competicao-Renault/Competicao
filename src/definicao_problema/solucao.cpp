#include "definicao_problema/solucao.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

bool avaliarSolucao(const Instancia &inst, Solucao &sol, std::string *motivo) {
    sol.viavel = false;
    sol.funcaoObjetivo = std::numeric_limits<double>::infinity();
    sol.piorCenario = -1;
    sol.custosCenarios.assign(inst.cenarios.size(), sol.funcaoObjetivo);
    if (motivo) motivo->clear();
    auto falha = [&](const std::string &texto) {
        if (motivo) *motivo = texto;
        return false;
    };
    const int nr = static_cast<int>(inst.rotas.size()), nt = static_cast<int>(inst.sites.size());
    const int nm = static_cast<int>(inst.companhias.size());
    if (sol.alocacoes.size() != inst.fluxos.size() || inst.cenarios.empty()) return falha("Numero de alocacoes ou cenarios invalido");
    std::vector<Inteiro> nominal(nr, 0), recebidos(nr, 0), fornecidos(nr, 0);
    std::set<std::pair<int, int>> pares;
    for (const auto &f : inst.fluxos) {
        const auto &a = sol.alocacoes[f.id];
        if (a.quantidade1 <= 0 || a.quantidade2 < 0 || Inteiro(a.quantidade1) + a.quantidade2 != f.demanda) return falha("Demanda incorreta no fluxo " + std::to_string(f.id));
        if (a.rota1 < 0 || a.rota1 >= nr || inst.rotas[a.rota1].fluxo != f.id) return falha("Primeira rota invalida");
        if ((a.quantidade2 == 0 && a.rota2 != -1) || (a.quantidade2 > 0 && (a.rota2 < 0 || a.rota2 >= nr || a.rota2 == a.rota1 || inst.rotas[a.rota2].fluxo != f.id))) return falha("Segunda rota invalida");
        nominal[a.rota1] = a.quantidade1;
        if (a.quantidade2 > 0) nominal[a.rota2] = a.quantidade2;
    }
    // Nas falhas, x de uma rota sobrevivente nao diminui e y so pode desaparecer.
    // Portanto, os limites por rota de y bastam ser verificados no nominal.
    for (const auto &y : sol.reutilizacoes) {
        if (y.rotaOrigem < 0 || y.rotaOrigem >= nr || y.rotaDestino < 0 || y.rotaDestino >= nr || y.quantidade <= 0) return falha("Reutilizacao invalida");
        const auto &r1 = inst.rotas[y.rotaOrigem], &r2 = inst.rotas[y.rotaDestino];
        if (r1.companhia != r2.companhia || inst.fluxos[r1.fluxo].destino != inst.fluxos[r2.fluxo].origem) return falha("Reutilizacao entre sites ou companhias diferentes");
        if (!pares.emplace(y.rotaOrigem, y.rotaDestino).second) return falha("Reutilizacao duplicada");
        fornecidos[y.rotaOrigem] += y.quantidade;
        recebidos[y.rotaDestino] += y.quantidade;
        if (fornecidos[y.rotaOrigem] > nominal[y.rotaOrigem] || recebidos[y.rotaDestino] > nominal[y.rotaDestino]) return falha("Reutilizacao excede o volume nominal");
    }
    double objetivo = -std::numeric_limits<double>::infinity();
    for (const auto &s : inst.cenarios) {
        sol.piorCenario = s.id; // Em caso de falha, informa o primeiro cenario inviavel.
        std::vector<Inteiro> portos(inst.portos.size(), 0), companhias(nm, 0), reutilizacao(nt, 0);
        std::vector<unsigned char> contratos(nm * nt, 0);
        const auto &indisponivel = inst.indisponivel[s.id];
        double custo = 0;
        auto adicionar = [&](int rota, Inteiro quantidade) {
            const auto &r = inst.rotas[rota];
            const auto &f = inst.fluxos[r.fluxo];
            portos[r.portoOrigem] += quantidade;
            portos[r.portoDestino] += quantidade; // SPOT tambem conta chegada e partida.
            companhias[r.companhia] += quantidade;
            custo += static_cast<double>(inst.custosRotas[rota]) * quantidade;
            for (int site : {f.origem, f.destino}) if (!contratos[r.companhia * nt + site]) {
                contratos[r.companhia * nt + site] = 1;
                custo += inst.companhias[r.companhia].custoFixo;
            }
        };
        for (const auto &f : inst.fluxos) {
            const auto &a = sol.alocacoes[f.id];
            bool falha1 = indisponivel[a.rota1], falha2 = true;
            if (a.rota2 >= 0) falha2 = indisponivel[a.rota2];
            if (falha1 && falha2) return falha("Fluxo sem rota disponivel no cenario " + std::to_string(s.id));
            if (falha1) adicionar(a.rota2, f.demanda);
            else if (a.rota2 >= 0 && falha2) adicionar(a.rota1, f.demanda);
            else {
                adicionar(a.rota1, a.quantidade1);
                if (a.rota2 >= 0) adicionar(a.rota2, a.quantidade2);
            }
        }
        for (const auto &y : sol.reutilizacoes) {
            if (indisponivel[y.rotaOrigem] || indisponivel[y.rotaDestino]) continue;
            const auto &r1 = inst.rotas[y.rotaOrigem], &r2 = inst.rotas[y.rotaDestino];
            int site = inst.fluxos[r1.fluxo].destino;
            reutilizacao[site] += y.quantidade;
            custo -= (static_cast<double>(inst.custosRodoviarios[site][r1.portoDestino]) + inst.custosRodoviarios[site][r2.portoOrigem]) * y.quantidade;
        }
        for (const auto &p : inst.portos) if (portos[p.id] > p.capacidade) return falha("Capacidade do porto excedida: " + std::to_string(p.id));
        for (const auto &t : inst.sites) if (reutilizacao[t.id] > t.capacidadeReutilizacao) return falha("Capacidade de reutilizacao excedida");
        for (const auto &m : inst.companhias) {
            int sites = std::count(contratos.begin() + m.id * nt, contratos.begin() + (m.id + 1) * nt, 1);
            if (companhias[m.id] > m.capacidade || sites > m.maximoSites) return falha("Capacidade ou limite de sites da companhia excedido: " + std::to_string(m.id));
            if (m.id != inst.companhiaSpot && companhias[m.id] > 0 && companhias[m.id] + 1e-9L < static_cast<long double>(inst.meta.alpha) * m.capacidade) return falha("Utilizacao minima da companhia nao atingida: " + std::to_string(m.id));
        }
        if (!std::isfinite(custo)) return falha("Custo nao finito");
        sol.custosCenarios[s.id] = custo;
        objetivo = std::max(objetivo, custo);
    }
    sol.funcaoObjetivo = objetivo;
    sol.piorCenario = static_cast<int>(std::max_element(sol.custosCenarios.begin(), sol.custosCenarios.end()) - sol.custosCenarios.begin());
    sol.viavel = true;

    return true;
}

Solucao criarSolucaoSpot(const Instancia &inst) {
    Solucao sol;
    sol.alocacoes.resize(inst.fluxos.size());
    for (const auto &f : inst.fluxos) {
        if (f.demanda > std::numeric_limits<int>::max()) throw std::runtime_error("Demanda excede o limite de AlocacaoFluxo");
        for (int r : f.rotas) if (inst.rotas[r].companhia == inst.companhiaSpot) sol.alocacoes[f.id] = {r, -1, static_cast<int>(f.demanda), 0};
    }
    std::string motivo;
    if (!avaliarSolucao(inst, sol, &motivo)) throw std::runtime_error("Solucao SPOT inviavel: " + motivo);

    return sol;
}

void salvarSolucao(const Instancia &inst, const Solucao &solucao, const std::string &caminho) {
    Solucao sol = solucao;
    std::string motivo;
    if (!avaliarSolucao(inst, sol, &motivo)) throw std::runtime_error("Exportacao de solucao inviavel: " + motivo);
    std::ofstream arquivo(caminho);
    if (!arquivo) throw std::runtime_error("Nao foi possivel abrir a saida: " + caminho);
    arquivo << "{\n  \"x\": [";
    bool primeiro = true;
    for (const auto &a : sol.alocacoes) for (const auto &item : {std::make_pair(a.rota1, a.quantidade1), std::make_pair(a.rota2, a.quantidade2)}) {
        if (item.second == 0) continue;
        if (!primeiro) arquivo << ',';
        arquivo << "\n    {\"route\": " << item.first << ", \"n\": " << item.second << '}';
        primeiro = false;
    }
    arquivo << "\n  ],\n  \"y\": [";
    primeiro = true;
    for (const auto &y : sol.reutilizacoes) {
        if (!primeiro) arquivo << ',';
        arquivo << "\n    {\"from\": " << y.rotaOrigem << ", \"to\": " << y.rotaDestino << ", \"n\": " << y.quantidade << '}';
        primeiro = false;
    }
    arquivo << "\n  ]\n}\n";
    arquivo.close();
    if (!arquivo) throw std::runtime_error("Erro ao gravar a saida: " + caminho);
}
