# Quando aplicar o B&B

Foram feitos dois ensaios pareados sobre as mesmas 15 solucoes pos-SA da
campanha anterior: tres instancias e cinco sementes (7, 42, 2026, 11 e 19).
Cada metodo parte de uma copia da mesma entrada; nao se refaz a construcao ou
o SA entre metodos. A entrada de cada execucao foi salva e reavaliada.

## Metodologia

O bloco chamado B&B inclui a passagem analitica de quantidades e o B&B oficial
de tres fluxos, com alternativas basicas, seis alternativas por fluxo, 216
folhas e 10.000 nos por chamada. A selecao mantem 30% de grupos aleatorios.
Nao se limita a quantidade total de grupos: usa o tempo reservado ao bloco.

Tabu usa lista 30, vizinhanca 20 e as oito familias de movimentos. Seus limites
de propostas e iteracoes sao altos para que a parada seja pelo tempo. Cada
metodo reinicia o RNG com a mesma semente; o RNG local do B&B continua entre as
duas chamadas na variante dividida. A ordem dos metodos alterna entre sementes.

Primeira campanha: 75 execucoes com um segundo total cada:

- Somente B&B: 1 segundo.
- Somente Tabu: 1 segundo.
- B&B antes: 0,5 segundo B&B -> 0,5 segundo Tabu.
- B&B depois: 0,5 segundo Tabu -> 0,5 segundo B&B.
- Dividido: 0,25 segundo B&B -> 0,5 segundo Tabu -> 0,25 segundo B&B.

Segunda campanha: 45 execucoes com dois segundos totais, comparando as tres
ordens combinadas e dobrando cada parcela acima. Os limites sao verificados
entre operacoes; pequenos excessos constam nos tempos efetivamente registrados.
Validacao final, leitura e exportacao ficam fora do tempo de busca.

## Resultados

Ganho percentual medio adicional sobre as entradas pos-SA, nas 15 execucoes:

| Ordem | Orcamento total 1 s | Orcamento total 2 s |
|---|---:|---:|
| B&B -> Tabu | 10,60% | 14,07% |
| Tabu -> B&B | 10,59% | 14,52% |
| B&B -> Tabu -> B&B | 10,39% | 15,32% |
| Somente B&B | 8,45% | Nao testado |
| Somente Tabu | 9,38% | Nao testado |

Com um segundo, depois venceu antes em 12 dos 15 pares, mas algumas vitorias
maiores de antes deixaram sua media ligeiramente melhor. A soma media dos
custos das tres instancias por semente foi 1.192.885.047,60 antes contra
1.195.124.230,40 depois: diferenca de aproximadamente 0,19%.

Com dois segundos, depois venceu antes em 10 de 15 pares. O dividido venceu
depois em 10 de 15. A soma media dos custos das tres instancias foi:

| Ordem | Score medio em 2 s (menor e melhor) |
|---|---:|
| B&B -> Tabu | 1.145.223.342,60 |
| Tabu -> B&B | 1.139.509.807,80 |
| B&B -> Tabu -> B&B | 1.130.451.621,80 |

O dividido teve maior dispersao entre sementes: desvio padrao do score de
57,94 milhoes, contra 26,36 milhoes de depois. Cinco sementes nao permitem
tratar essas diferencas como uma garantia estatistica para qualquer orcamento.
Em strikes, antes foi melhor em media no ensaio de um segundo, enquanto depois
foi melhor em media no de dois segundos. A preferencia nao e universal.

## Recomendacao

Manter B&B depois da Tabu como ordem simples de polimento: foi mais consistente
na maioria das comparacoes pareadas. Com dois segundos disponiveis para essas etapas,
experimentar 25% B&B antes, 50% Tabu e 25% B&B depois mostrou a melhor media.
Isso nao implica que a mesma divisao seja ideal para varios minutos ou para
solucoes iniciais melhores; recomenda-se comparar novamente nesses cenarios.

O fluxo de producao agora divide o refinamento igualmente antes e depois
da Tabu quando ambas as etapas estao ativas. Consulte [os parametros](branch_and_bound.md).

Todos os 120 JSONs finais foram conferidos pelo verificador Python independente,
incluindo custos, capacidades, contratos, minimos, reutilizacao e todos os
cenarios. Os intermediarios tambem preservaram a incumbente de cada etapa.
Nao houve alteracao do SA, da Tabu ou do B&B de producao durante estes testes.

## Reproducao

```powershell
python tests/executar_bb.py posicao       # 75 execucoes, total de 1 s por metodo
python tests/executar_bb.py posicao_longa # 45 execucoes, total de 2 s por metodo
```

Os ensaios exigem as 15 entradas existentes em `output/bb/entradas`.
Fontes usados sao congelados antes de compilar e conferidos ao terminar.
Resultados, configuracoes, fontes, entradas e resumos ficam em
`output/posicao_bb` e `output/posicao_bb_longo`. Para analisar sem repetir:

```powershell
python tests/analisar_posicao_bb.py
python tests/analisar_posicao_bb.py output/posicao_bb_longo
```
