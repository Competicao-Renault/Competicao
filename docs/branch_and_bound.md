# Branch and Bound oficial

Ensaios sobre a ordem em relacao a busca tabu estao em
[posicionamento do B&B](posicao_bb.md). O fluxo simples mantem B&B depois da tabu.

`refinarGrupo` usa B&B por padrao. `branchAndBound` permite fornecer listas
explicitas; `enumerarGrupo` continua sendo a referencia exaustiva. As estruturas
da instancia, solucao, SA e tabu foram preservadas. Nao ha solver externo.

## Escopo e garantias

Cada nivel fixa uma alocacao de um fluxo; o grupo pode conter de dois a seis
fluxos. As listas sempre incluem a alocacao atual. Os outros fluxos ficam fixos.
A busca devolve somente melhorias viaveis, com caches recalculados. Atingir um
limite preserva qualquer melhoria ja encontrada e retorna `concluido=false`.
`concluido=true` significa otimo na vizinhanca enumerada, nao na instancia inteira.
As listas amostradas nao representam todas as rotas e quantidades possiveis.

Antes da arvore, precomputa transporte, cargas e contratos por alternativa e
cenario. Retira todos os fluxos livres da base e depois soma somente os ja
atribuidos. Mantem contadores de contratos companhia-site para nao duplica-los.

Podas implementadas:

- Capacidade: carga atribuida mais o minimo inevitavel dos fluxos restantes
  excede a capacidade de porto ou companhia; ou contratos ja excedem sites.
- Minimo comercial: uma companhia ja ativa nao atinge o minimo nem recebendo
  o maior volume possivel dos fluxos ainda livres naquele cenario. Companhias
  inativas nao sao podadas apenas por estarem abaixo do minimo; SPOT e isenta.
- Custo: para algum cenario, transporte fixo e atribuido, mais menores custos
  de transporte restantes e contratos ja obrigatorios, menos a economia
  original de reutilizacao, supera a incumbente com margem numerica.

O ultimo limite e valido porque `alterarAlocacoes` apenas reduz reutilizacoes
existentes. A economia original e um teto otimista. Se uma futura versao criar
novas reutilizacoes nas folhas, esse limite precisara ser revisto.
Folhas promissoras passam pelo avaliador completo e pelo ajuste existente de y.

## Parametros e integracao

`ParametrosRefinamento`: `usarBB=true`, `alternativasComerciais=false`, seis
alternativas por fluxo, 216 folhas e 10.000 nos por chamada. O fluxo oficial
mantem tres fluxos por grupo e seu limite de tempo final existente.
`ResultadoRefinamento` inclui nos, podas de custo, capacidade e minimo.
O console mostra `BB_nos` e `BB_podas` acumulados.

```powershell
./bin/competicao.exe --bb instancia/alliances.json
./bin/competicao.exe --enumeracao instancia/alliances.json
./bin/competicao.exe --bb --alternativas-comerciais instancia/alliances.json
```

O gerador comercial foi compartilhado com a construtiva, preservando seus
pontos de minimos e capacidades e a diversidade de pares. O gerador basico
permanece o padrao por ter sido mais consistente na comparacao global.
Para experimentar grupos maiores pela API, aumente tambem o limite de folhas:

```cpp
ParametrosExecucao p;
p.tamanhoGrupoRefinamento = 4;
p.refinamento.maximoCombinacoes = 1296; // seis alternativas para quatro fluxos
auto resultado = executarFluxo(inst, p);
```

Nao ha poda por simples inviabilidade parcial da solucao original: contratos
e volumes das escolhas ainda nao feitas nao entram como se estivessem fixados.
O tempo inclui preparacao e e checado entre nos; uma operacao nao e interrompida.

## Testes comparativos

Foram construidas 15 entradas fixas: tres instancias e sementes 7, 42, 2026,
11 e 19. Cada entrada foi construida uma vez e recebeu 3.000 propostas de SA.
Sete metodos partiram de copias dessa mesma entrada, cada um com 0,5 segundo,
incluindo selecao de grupos e geracao de alternativas. A ordem foi alternada.
Todos os 105 JSONs e as 15 entradas foram conferidos pelo verificador Python.

Ganho percentual medio sobre a entrada, nas 15 execucoes:

| Metodo | Ganho medio |
|---|---:|
| Enumeracao, 3 fluxos | 5,24% |
| B&B, 3 fluxos | 6,89% |
| B&B, 4 fluxos | 6,90% |
| B&B comercial, 3 fluxos | 5,18% |
| B&B + movimento compensado | 6,66% |
| Movimento compensado | 2,33% |
| Fechamento de contrato | 1,01% |

B&B de tres e quatro fluxos ficaram praticamente empatados; foi mantido o
grupo de tres, com menor arvore e configuracao ja usada no fluxo. O modo
comercial foi melhor em alliances e pior em reliability, sem promocao global.
Os movimentos novos ficam disponiveis para outros ensaios, sem alterar os oito
pesos do SA: `compensarFluxos`, `movimentoCompensado`, `fecharContrato` e
`movimentoContrato`. Todos preservam atomicidade e passam pelo avaliador.

Houve ainda 75 comparacoes com listas identicas e busca concluida: os custos
foram iguais, o tempo total caiu de 0,7147 para 0,2734 segundo (2,61x) e as
combinacoes completas testadas cairam de 16.200 para 298 (98,16%). Esses tempos
incluem a preparacao do B&B e excluem a geracao comum de alternativas.
Os testes sinteticos com 160 listas aleatorias incluem reutilizacao e minimos;
ha tambem verificacao para seis fluxos, limites de nos e compensacao de carga.

Sao ensaios iniciais com orcamento curto, nao uma garantia de superioridade para
todas as instancias ou tempos. O codigo das estrategias e as configuracoes estao
em `tests/comparar_bb.cpp`; os dados completos, resumos, entradas e snapshot dos
fontes usados ficam em `output/bb/`.

```powershell
python tests/executar_bb.py  # recompila e substitui a campanha em output/bb
python tests/analisar_bb.py # somente confere e resume os resultados existentes
```

O executor registra hashes antes de executar e verifica que os fontes nao
mudaram durante a campanha. `fontes.zip` preserva a biblioteca e o driver.
