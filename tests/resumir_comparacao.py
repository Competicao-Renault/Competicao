"""Resume as execucoes pareadas das tres estrategias, sem bibliotecas externas."""
import csv
import statistics
from collections import defaultdict
from pathlib import Path

pasta = Path('output/comparacao_grupos')
with (pasta / 'resultados.csv').open(encoding='utf-8') as arquivo:
    linhas = list(csv.DictReader(arquivo))
grupos = defaultdict(list)
for linha in linhas:
    grupos[linha['instancia'], linha['estrategia']].append(linha)
resumo = []
for (instancia, estrategia), casos in sorted(grupos.items()):
    reducoes = [float(c['reducao_percentual']) for c in casos]
    custos = [float(c['custo_final']) for c in casos]
    aleatoria = {c['semente']: float(c['custo_final']) for c in grupos[instancia, 'aleatoria']}
    assert set(aleatoria) == {c['semente'] for c in casos}
    resumo.append({
        'instancia': instancia, 'estrategia': estrategia, 'execucoes': len(casos),
        'reducao_media_percentual': statistics.mean(reducoes),
        'reducao_mediana_percentual': statistics.median(reducoes),
        'desvio_padrao_reducao': statistics.stdev(reducoes),
        'custo_medio': statistics.mean(custos), 'melhor_custo': min(custos),
        'execucoes_sem_melhoria': sum(r == 0 for r in reducoes),
        'tempo_medio_segundos': statistics.mean(float(c['segundos']) for c in casos),
        'grupos_medios': statistics.mean(int(c['grupos']) for c in casos),
        'vitorias_sobre_aleatoria': sum(float(c['custo_final']) < aleatoria[c['semente']] for c in casos),
        'empates_com_aleatoria': sum(float(c['custo_final']) == aleatoria[c['semente']] for c in casos),
    })
with (pasta / 'resumo.csv').open('w', encoding='utf-8', newline='') as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=list(resumo[0]))
    escritor.writeheader()
    escritor.writerows(resumo)
for r in resumo:
    print(f"{r['instancia']:12} {r['estrategia']:13} media={r['reducao_media_percentual']:.2f}% "
          f"sd={r['desvio_padrao_reducao']:.2f} melhor={r['melhor_custo']:.0f} "
          f"sem_melhoria={r['execucoes_sem_melhoria']} vitorias={r['vitorias_sobre_aleatoria']} "
          f"tempo={r['tempo_medio_segundos']:.3f}s")
