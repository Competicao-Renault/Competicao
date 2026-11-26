import csv,json,sys,statistics as st
from pathlib import Path
sys.path.insert(0,'tests')
from verificar_simulacao import conferir
p=Path('output/duas_chamadas_bb')
new=list(csv.DictReader((p/'comparacao.csv').open()))
assert len(new)==30
old=[r for r in csv.DictReader(open('output/posicao_bb_longo/comparacao.csv')) if r['metodo'] in ('bb_depois','bb_dividido')]
key=lambda r:(r['instancia'],r['semente'])
base={key(r):r['custo_inicial'] for r in old}
benchmark={key(r):float(r['custo_final']) for r in old if r['metodo']=='bb_depois'}
inst={n:json.loads(Path('instancia',n+'.json').read_text(encoding='utf-8')) for n,k in base}
for r in new:
 n,k=key(r); m=r['metodo']
 assert r['custo_inicial']==base[n,k]
 assert (p/'entradas'/f'{n}-{k}.json').read_bytes()==Path('output/posicao_bb_longo/entradas',f'{n}-{k}.json').read_bytes()
 assert conferir(inst[n],json.loads((p/f'{n}-{k}-{m}.json').read_text()))[0]==float(r['custo_final'])<=float(r['custo_intermediario2'])<=float(r['custo_intermediario1'])<=float(r['custo_inicial'])
res=[]
for m in sorted({r['metodo'] for r in old+new}):
 a=[r for r in old+new if r['metodo']==m]
 res.append(dict(metodo=m,ganho_medio=st.mean(float(r['ganho_percentual']) for r in a),score_medio=sum(float(r['custo_final']) for r in a)/5,vitorias=sum(float(r['custo_final'])<benchmark[key(r)] for r in a),segundos_medios=st.mean(float(r['segundos']) for r in a)))
with (p/'resumo.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(res[0]));w.writeheader();w.writerows(res)
for r in res:print(r)
print('30 JSONs novos validados; 15 entradas identicas.')
for m in ('bb_dividido','bb_final_maior','bb_inicial_maior'):
 a=[r for r in old+new if r['metodo']==m]
 g=[float(r['custo_intermediario2'])-float(r['custo_final']) for r in a]
 print(m,'melhorias na segunda chamada',sum(x>0 for x in g),'ganho medio em USD',round(st.mean(g),2))
