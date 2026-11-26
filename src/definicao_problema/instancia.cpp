#include "definicao_problema/instancia.hpp"
#include "json.hpp"
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>

namespace {
using leitura::Json;
void exigir(bool condicao, const std::string &mensagem) {
    if (!condicao) throw std::runtime_error(mensagem);
}
Inteiro natural(const Json &j, const std::string &campo) {
    Inteiro valor = j.campo(campo).inteiro();
    exigir(valor >= 0, "Valor negativo em " + campo);
    return valor;
}
int indice(const Json &j) {
    Inteiro valor = j.inteiro();
    exigir(valor >= 0 && valor <= std::numeric_limits<int>::max(), "Indice fora do intervalo");
    return static_cast<int>(valor);
}
int indice(const Json &j, const std::string &campo) { return indice(j.campo(campo)); }
std::vector<int> indices(const Json &j) {
    std::vector<int> resultado;
    for (const auto &v : j.itens()) resultado.push_back(indice(v));
    return resultado;
}
Localizacao localizacao(const Json &j) {
    const auto &loc = j.campo("loc");
    double lat = loc.campo("lat").real(), lon = loc.campo("lon").real();
    exigir(std::isfinite(lat) && std::isfinite(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180, "Localizacao invalida");
    return {lat, lon};
}
template <typename T> void validarIds(const std::vector<T> &objetos, int quantidade, const std::string &nome) {
    exigir(objetos.size() == static_cast<std::size_t>(quantidade), "Dimensao incorreta: " + nome);
    for (std::size_t i = 0; i < objetos.size(); ++i) exigir(objetos[i].id == static_cast<int>(i), "IDs nao consecutivos: " + nome);
}
void validarEPreparar(Instancia &inst) {
    const auto &d = inst.meta.dimensoes;
    validarIds(inst.portos, d.portos, "ports"); validarIds(inst.sites, d.sites, "sites");
    validarIds(inst.companhias, d.companhias, "carriers"); validarIds(inst.fluxos, d.fluxos, "flows");
    validarIds(inst.rotas, d.rotas, "routes"); validarIds(inst.cenarios, d.cenarios, "scenarios");
    exigir(d.portos > 0 && d.sites > 0 && d.companhias > 0 && d.fluxos > 0 && d.rotas > 0 && d.cenarios > 0, "Instancia vazia");
    inst.custosRodoviarios.assign(d.sites, std::vector<Inteiro>(d.portos, -1));
    for (const auto &p : inst.portos) if (p.codigo == "SPOT") {
        exigir(inst.portoSpot == -1, "Porto SPOT duplicado");
        inst.portoSpot = p.id;
    }
    for (const auto &m : inst.companhias) if (m.nome == "SPOT") {
        exigir(inst.companhiaSpot == -1, "Companhia SPOT duplicada");
        inst.companhiaSpot = m.id;
    }
    exigir(inst.portoSpot >= 0 && inst.companhiaSpot >= 0, "SPOT ausente");
    for (const auto &t : inst.sites) for (const auto &a : t.acessos) {
        exigir(a.porto < d.portos, "Porto inexistente em site " + std::to_string(t.id));
        exigir(inst.custosRodoviarios[t.id][a.porto] == -1, "Acesso rodoviario duplicado");
        inst.custosRodoviarios[t.id][a.porto] = a.custo;
    }
    std::vector<int> usos(d.rotas, 0);
    for (const auto &f : inst.fluxos) {
        exigir(f.origem < d.sites && f.destino < d.sites && f.demanda > 0 && !f.rotas.empty(), "Fluxo invalido: " + std::to_string(f.id));
        exigir(f.demanda <= std::numeric_limits<Inteiro>::max() - inst.demandaTotal, "Demanda total excede int64");
        inst.demandaTotal += f.demanda;
        for (int r : f.rotas) {
            exigir(r < d.rotas, "Rota inexistente em fluxo " + std::to_string(f.id));
            exigir(++usos[r] == 1 && inst.rotas[r].fluxo == f.id, "Vinculo fluxo-rota inconsistente");
        }
    }
    inst.custosRotas.reserve(d.rotas);
    std::vector<int> rotasSpot(d.fluxos, 0);
    for (const auto &r : inst.rotas) {
        exigir(r.fluxo < d.fluxos && r.companhia < d.companhias && r.portoOrigem < d.portos && r.portoDestino < d.portos && usos[r.id] == 1, "Rota invalida: " + std::to_string(r.id));
        const auto &f = inst.fluxos[r.fluxo];
        Inteiro partida = inst.custosRodoviarios[f.origem][r.portoOrigem], chegada = inst.custosRodoviarios[f.destino][r.portoDestino];
        exigir(partida >= 0 && chegada >= 0, "Rota sem acesso rodoviario: " + std::to_string(r.id));
        Inteiro saldo = (std::numeric_limits<Inteiro>::max() - r.custoMaritimo) / 2;
        exigir(partida <= saldo && chegada <= saldo - partida, "Custo da rota excede int64");
        inst.custosRotas.push_back(r.custoMaritimo + 2 * partida + 2 * chegada);
        if (r.companhia == inst.companhiaSpot) {
            exigir(r.portoOrigem == inst.portoSpot && r.portoDestino == inst.portoSpot, "Rota SPOT fora do porto virtual");
            ++rotasSpot[r.fluxo];
        }
    }
    for (int quantidade : rotasSpot) exigir(quantidade == 1, "Cada fluxo deve ter uma rota SPOT");
    inst.indisponivel.assign(d.cenarios, std::vector<unsigned char>(d.rotas, 0));
    bool nominal = false;
    for (const auto &s : inst.cenarios) {
        if (s.rotasIndisponiveis.empty()) nominal = true;
        for (int r : s.rotasIndisponiveis) {
            exigir(r < d.rotas, "Rota inexistente em cenario " + std::to_string(s.id));
            exigir(!inst.indisponivel[s.id][r] && inst.rotas[r].companhia != inst.companhiaSpot, "Falha SPOT ou rota duplicada em cenario");
            inst.indisponivel[s.id][r] = 1;
        }
    }
    exigir(nominal, "Cenario nominal ausente");
}
}

Instancia lerInstancia(const std::string &caminho) {
    try {
        std::ifstream arquivo(caminho, std::ios::binary);
        exigir(arquivo.is_open(), "Nao foi possivel abrir o arquivo");
        std::string conteudo((std::istreambuf_iterator<char>(arquivo)), std::istreambuf_iterator<char>());
        exigir(!arquivo.bad(), "Erro ao ler o arquivo");
        const Json raiz = leitura::LeitorJson(conteudo).ler();
        const auto &meta = raiz.campo("meta"), &dim = meta.campo("sizes"), &unidades = meta.campo("units");
        Instancia inst;
        inst.meta = {meta.campo("name").texto(), meta.campo("description").texto(), unidades.campo("volume").texto(), unidades.campo("cost").texto(), meta.campo("alpha").real(),
                     {indice(dim, "ports"), indice(dim, "sites"), indice(dim, "carriers"), indice(dim, "flows"), indice(dim, "routes"), indice(dim, "scenarios")}};
        exigir(std::isfinite(inst.meta.alpha) && inst.meta.alpha >= 0 && inst.meta.alpha <= 1, "Alpha fora de [0,1]");
        for (const auto &j : raiz.campo("ports").itens()) inst.portos.push_back({indice(j, "id"), j.campo("code").texto(), j.campo("name").texto(), natural(j, "manut"), localizacao(j)});
        for (const auto &j : raiz.campo("sites").itens()) {
            Site site{indice(j, "id"), j.campo("name").texto(), natural(j, "reutil"), localizacao(j), {}};
            for (const auto &a : j.campo("ports").itens()) site.acessos.push_back({indice(a, "port"), natural(a, "cost")});
            inst.sites.push_back(std::move(site));
        }
        for (const auto &j : raiz.campo("carriers").itens()) inst.companhias.push_back({indice(j, "id"), j.campo("name").texto(), natural(j, "fixed_cost"), natural(j, "trans"), indice(j, "sites")});
        for (const auto &j : raiz.campo("flows").itens()) inst.fluxos.push_back({indice(j, "id"), indice(j, "t_dep"), indice(j, "t_arr"), natural(j, "n"), indices(j.campo("routes"))});
        for (const auto &j : raiz.campo("routes").itens()) inst.rotas.push_back({indice(j, "id"), indice(j, "flow"), indice(j, "carrier"), indice(j, "p_dep"), indice(j, "p_arr"), natural(j, "cost")});
        for (const auto &j : raiz.campo("scenarios").itens()) inst.cenarios.push_back({indice(j, "id"), indices(j.campo("routes"))});
        validarEPreparar(inst);
        return inst;
    } catch (const std::exception &erro) {
        throw std::runtime_error(caminho + ": " + erro.what());
    }
}
