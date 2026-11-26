"""Compara a posicao do B&B usando entradas identicas e um segundo por metodo."""
import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from verificar_simulacao import conferir

pasta = Path('output/posicao_bb')
if len(sys.argv) > 1:
    pasta = Path(sys.argv[1])
with (pasta / 'comparacao.csv').open(encoding='utf-8') as arquivo:
    linhas = list(csv.DictReader(arquivo))
metodos = {r['metodo'] for r in linhas}
assert len(linhas) == 15 * len(metodos) and metodos in ({'bb_so', 'tabu_so', 'bb_antes', 'bb_depois', 'bb_dividido'}, {'bb_antes', 'bb_depois', 'bb_dividido'})
instancias = {n: json.loads(Path('instancia', n + '.json').read_text(encoding='utf-8'))
              for n in ('alliances', 'reliability', 'strikes')}
originais, casos, grupos = {}, {}, defaultdict(list)
for r in linhas:
    nome, seed, metodo = r['instancia'], r['semente'], r['metodo']
    if (nome, seed) not in originais:
        origem = json.loads((pasta / 'entradas' / f'{nome}-{seed}.json').read_text())
        originais[nome, seed] = conferir(instancias[nome], origem)[0]
    custo, _ = conferir(instancias[nome], json.loads((pasta / f'{nome}-{seed}-{metodo}.json').read_text()))
    assert custo == float(r['custo_final']) <= float(r['custo_intermediario2']) <= float(r['custo_intermediario1']) <= originais[nome, seed] == float(r['custo_inicial'])
    assert abs(float(r['bb_segundos']) + float(r['tabu_segundos']) - float(r['segundos'])) < 0.02
    assert (nome, seed, metodo) not in casos
    casos[nome, seed, metodo] = r
    grupos[nome, metodo].append(r)
resumo = []
for (nome, metodo), rs in sorted(grupos.items()):
    assert len(rs) == 5
    resumo.append(dict(instancia=nome, metodo=metodo,
                       ganho_medio_percentual=statistics.mean(float(r['ganho_percentual']) for r in rs),
                       custo_medio=statistics.mean(float(r['custo_final']) for r in rs),
                       melhor_custo=min(float(r['custo_final']) for r in rs),
                       segundos_medios=statistics.mean(float(r['segundos']) for r in rs)))
with (pasta / 'resumo.csv').open('w', encoding='utf-8', newline='') as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=list(resumo[0])); escritor.writeheader(); escritor.writerows(resumo)
for r in resumo:
    print(f"{r['instancia']:12} {r['metodo']:12} ganho={r['ganho_medio_percentual']:.3f}% tempo={r['segundos_medios']:.4f}s")
totais = []
for metodo in sorted({r['metodo'] for r in linhas}):
    rs = [r for r in linhas if r['metodo'] == metodo]
    escores = [sum(float(casos[nome, seed, metodo]['custo_final']) for nome in instancias) for seed in sorted({r['semente'] for r in rs})]
    totais.append(dict(metodo=metodo, ganho_medio_percentual=statistics.mean(float(r['ganho_percentual']) for r in rs),
                       score_medio=statistics.mean(escores), melhor_score=min(escores),
                       desvio_padrao_score=statistics.stdev(escores),
                       segundos_medios=statistics.mean(float(r['segundos']) for r in rs)))
    print('TOTAL', totais[-1])
with (pasta / 'resumo_total.csv').open('w', encoding='utf-8', newline='') as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=list(totais[0])); escritor.writeheader(); escritor.writerows(totais)
comparacoes = []
for a, b in [('bb_antes', 'bb_depois'), ('bb_depois', 'bb_dividido'), ('bb_so', 'bb_depois')]:
    if a not in metodos or b not in metodos:
        continue
    ganhos = [float(casos[n, s, a]['custo_final']) - float(casos[n, s, b]['custo_final']) for n, s in originais]
    comparacoes.append(dict(anterior=a, posterior=b, ganho_medio_absoluto=statistics.mean(ganhos),
                            posterior_melhor=sum(g > 0 for g in ganhos), empates=ganhos.count(0),
                            posterior_pior=sum(g < 0 for g in ganhos)))
with (pasta / 'pares.csv').open('w', encoding='utf-8', newline='') as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=list(comparacoes[0])); escritor.writeheader(); escritor.writerows(comparacoes)
for r in comparacoes:
    print('PARES', r)
print(f'{len(linhas)} resultados e 15 entradas verificados independentemente em todos os cenarios.')
