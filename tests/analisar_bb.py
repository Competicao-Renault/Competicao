"""Valida os ensaios de B&B e compara estrategias com entradas pareadas."""
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path
from verificar_simulacao import conferir

pasta = Path('output/bb')
with (pasta / 'comparacao.csv').open(encoding='utf-8') as arquivo:
    linhas = list(csv.DictReader(arquivo))
with (pasta / 'mesmas_alternativas.csv').open(encoding='utf-8') as arquivo:
    micro = list(csv.DictReader(arquivo))
assert len(linhas) == 105 and len(micro) == 75
instancias = {n: json.loads(Path('instancia', n + '.json').read_text(encoding='utf-8'))
              for n in ('alliances', 'reliability', 'strikes')}
originais = {}
for r in linhas:
    nome, seed, metodo = r['instancia'], r['semente'], r['metodo']
    if (nome, seed) not in originais:
        original = json.loads((pasta / 'entradas' / f'{nome}-{seed}.json').read_text())
        originais[nome, seed] = conferir(instancias[nome], original)[0]
    sol = json.loads((pasta / f'{nome}-{seed}-{metodo}.json').read_text())
    custo, _ = conferir(instancias[nome], sol)
    assert custo == float(r['custo_final']) <= originais[nome, seed] == float(r['custo_inicial'])
assert all(float(r['custo_enum']) == float(r['custo_bb']) for r in micro)
grupos = defaultdict(list)
for r in linhas:
    grupos[r['instancia'], r['metodo']].append(r)
resumo = []
for (nome, metodo), casos in sorted(grupos.items()):
    referencia = {r['semente']: float(r['custo_final']) for r in grupos[nome, 'enumeracao3']}
    assert set(referencia) == {r['semente'] for r in casos}
    resumo.append(dict(instancia=nome, metodo=metodo,
                       ganho_medio_percentual=statistics.mean(float(r['ganho_percentual']) for r in casos),
                       custo_medio=statistics.mean(float(r['custo_final']) for r in casos),
                       melhor_custo=min(float(r['custo_final']) for r in casos),
                       tempo_medio=statistics.mean(float(r['segundos']) for r in casos),
                       vitorias_vs_enum=sum(float(r['custo_final']) < referencia[r['semente']] for r in casos),
                       empates_vs_enum=sum(float(r['custo_final']) == referencia[r['semente']] for r in casos)))
with (pasta / 'resumo.csv').open('w', encoding='utf-8', newline='') as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=list(resumo[0])); escritor.writeheader(); escritor.writerows(resumo)
for r in resumo:
    print(f"{r['instancia']:12} {r['metodo']:17} ganho={r['ganho_medio_percentual']:.3f}% "
          f"vitorias={r['vitorias_vs_enum']} empates={r['empates_vs_enum']}")
for metodo in sorted({r['metodo'] for r in linhas}):
    casos = [r for r in linhas if r['metodo'] == metodo]
    print('TOTAL', metodo, 'ganho_medio', statistics.mean(float(r['ganho_percentual']) for r in casos))
te = sum(float(r['segundos_enum']) for r in micro)
tb = sum(float(r['segundos_bb']) for r in micro)
fe = sum(int(r['folhas_enum']) for r in micro)
fb = sum(int(r['folhas_bb']) for r in micro)
print('MESMAS LISTAS', len(micro), 'tempo_enum', te, 'tempo_bb', tb, 'razao', te / tb,
      'folhas_enum', fe, 'folhas_bb', fb, 'reducao_folhas', 100 * (1 - fb / fe))
print('105 resultados, 15 entradas e 75 comparacoes nas mesmas listas conferidos.')
