"""Confere os JSONs da simulacao pelas equacoes do enunciado, sem chamar C++."""
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path


def conferir(instancia, solucao):
    rotas, fluxos = instancia['routes'], instancia['flows']
    companhias, portos, sites = instancia['carriers'], instancia['ports'], instancia['sites']
    rodovia = [{a['port']: a['cost'] for a in t['ports']} for t in sites]
    x, y, por_fluxo = {}, {}, defaultdict(list)
    for a in solucao['x']:
        r, n = a['route'], a['n']
        assert type(r) is int and 0 <= r < len(rotas) and r not in x
        assert type(n) is int and n > 0
        x[r] = n
        por_fluxo[rotas[r]['flow']].append(r)
    for f in fluxos:
        assert 1 <= len(por_fluxo[f['id']]) <= 2
        assert sum(x[r] for r in por_fluxo[f['id']]) == f['n']
    for a in solucao['y']:
        origem, destino, n = a['from'], a['to'], a['n']
        assert type(origem) is int and type(destino) is int
        assert 0 <= origem < len(rotas) and 0 <= destino < len(rotas)
        assert type(n) is int and n > 0 and (origem, destino) not in y
        r1, r2 = rotas[origem], rotas[destino]
        assert r1['carrier'] == r2['carrier']
        assert fluxos[r1['flow']]['t_arr'] == fluxos[r2['flow']]['t_dep']
        y[origem, destino] = n
    custos = []
    for s in instancia['scenarios']:
        falhas, xs = set(s['routes']), {}
        for f in fluxos:
            sobreviventes = [r for r in por_fluxo[f['id']] if r not in falhas]
            assert sobreviventes, f"Fluxo {f['id']} sem rota no cenario {s['id']}"
            if len(sobreviventes) == 1:
                xs[sobreviventes[0]] = f['n']
            else:
                xs.update((r, x[r]) for r in sobreviventes)
        carga_portos, carga_companhias = defaultdict(int), defaultdict(int)
        contratos, custo = set(), 0
        for r, n in xs.items():
            rota, fluxo = rotas[r], fluxos[rotas[r]['flow']]
            m, p, q = rota['carrier'], rota['p_dep'], rota['p_arr']
            carga_portos[p] += n
            carga_portos[q] += n
            carga_companhias[m] += n
            contratos.update(((m, fluxo['t_dep']), (m, fluxo['t_arr'])))
            custo += n * (rota['cost'] + 2*rodovia[fluxo['t_dep']][p] + 2*rodovia[fluxo['t_arr']][q])
        entrada, saida, reutilizacao = defaultdict(int), defaultdict(int), defaultdict(int)
        for (origem, destino), n in y.items():
            if origem in falhas or destino in falhas:
                continue
            r1, r2 = rotas[origem], rotas[destino]
            t = fluxos[r1['flow']]['t_arr']
            entrada[destino] += n
            saida[origem] += n
            reutilizacao[t] += n
            custo -= n * (rodovia[t][r1['p_arr']] + rodovia[t][r2['p_dep']])
        assert all(n <= xs.get(r, 0) for r, n in entrada.items())
        assert all(n <= xs.get(r, 0) for r, n in saida.items())
        assert all(carga_portos[p['id']] <= p['manut'] for p in portos)
        assert all(reutilizacao[t['id']] <= t['reutil'] for t in sites)
        for m in companhias:
            carga = carga_companhias[m['id']]
            assert carga <= m['trans']
            assert sum(c == m['id'] for c, _ in contratos) <= m['sites']
            if carga > 0 and m['name'] != 'SPOT':
                assert carga + 1e-9 >= instancia['meta']['alpha'] * m['trans']
        custos.append(custo + sum(companhias[m]['fixed_cost'] for m, _ in contratos))
    return max(custos), custos.index(max(custos))


if __name__ == '__main__':
    pasta = Path('output/simulacao')
    if len(sys.argv) > 1:
        pasta = Path(sys.argv[1])
    linhas = list(csv.DictReader((pasta / 'resultados.csv').open(encoding='utf-8')))
    for linha in linhas:
        nome, semente = linha['instancia'], linha['semente']
        instancia = json.loads(Path('instancia', nome + '.json').read_text(encoding='utf-8'))
        prefixo = nome
        if 'estrategia' in linha:
            prefixo += '-' + linha['estrategia']
        solucao = json.loads((pasta / f'{prefixo}-semente-{semente}.json').read_text())
        custo, pior = conferir(instancia, solucao)
        assert custo == float(linha['custo_final']) and pior == int(linha['pior_cenario'])
        assert custo <= float(linha['custo_inicial'])
        assert float(linha['ganho_grupos']) + float(linha['ganho_quantidades']) == float(linha['custo_inicial']) - custo
    print(f'{len(linhas)} solucoes conferidas independentemente em todos os cenarios.')
