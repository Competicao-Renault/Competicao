# Refinamento local

Atualizacao: `refinarGrupo` agora usa [B&B](branch_and_bound.md) por padrao,
com podas e limites de nos. `enumerarGrupo` permanece como referencia exaustiva.
As descricoes abaixo da enumeracao continuam se referindo a essa referencia.

Interface: `include/refinamento/refinamento.hpp`. O modulo depende apenas das
estruturas, do avaliador, dos movimentos e do cronometro ja existentes. Nao
altera o programa principal, a geracao inicial, o SA nem o gerador aleatorio.
O Makefile existente descobre o novo `.cpp` automaticamente.

## Integracao

```cpp
#include "refinamento/refinamento.hpp"

// sol e uma solucao viavel produzida pela construtiva ou pelo SA.
ResultadoRefinamento quantidade = refinarQuantidade(inst, sol, fluxo);

ParametrosRefinamento parametros;
parametros.maximoAlternativas = 6;
parametros.maximoCombinacoes = 216;
parametros.limiteSegundos = 1.0;
ResultadoRefinamento grupo = refinarGrupo(inst, sol, {fluxoA, fluxoB}, parametros);
```

Os fluxos do grupo devem ser distintos, em quantidade de dois ou tres. O
chamador escolhe os grupos e a frequencia das chamadas. `sol` so e substituida
quando existe melhoria estrita; a melhoria inclui custos por cenario, objetivo,
pior cenario e viabilidade atualizados. Sem melhoria, a entrada permanece igual.
Entrada inviavel, IDs ou parametros invalidos lancam `std::invalid_argument`.

## Quantidades

`refinarQuantidade` trabalha com o par de rotas atual. No interior, ambas
permanecem positivas e as reutilizacoes ficam fixas. Capacidades, utilizacao
minima em todos os cenarios e volumes de reutilizacao definem um intervalo
inteiro viavel. Contratos e sites atendidos nao mudam nesse intervalo.

O custo tem inclinacao `custosRotas[rota1] - custosRotas[rota2]` nos cenarios
em que ambas sobrevivem, e zero nos demais. Logo, o extremo viavel que favorece
a rota mais barata minimiza o custo robusto nesse interior. A preparacao custa
O(cenarios * (fluxos + portos + companhias) + reutilizacoes).

Os dois extremos com uma unica rota sao testados separadamente, permitindo
remover contratos e limitar reutilizacoes conforme o movimento existente.
Sao testados no maximo tres candidatos completos, alem da validacao da entrada.
Nao ha busca de outros pares nem otimizacao conjunta de reutilizacoes.

## Grupos

`refinarGrupo` preserva a alocacao atual e a alternativa SPOT em cada lista.
As outras alternativas sao rotas individuais imunes ou pares robustos com
divisoes proximas de 25/50/75%, ordenadas pelo custo de transporte do pior
cenario. Essa ordenacao e heuristica: nao inclui contratos ou economias de y.
A geracao percorre pares de rotas, respeitando o tempo entre etapas.

Quem gera a solucao ou executa o SA pode fornecer alternativas especificas:

```cpp
std::vector<AlternativasFluxo> alternativas{
    {fluxoA, {{rotaA, spotA, 100, 50}}},
    {fluxoB, {{rotaB, spotB, 80, 20}}}
};
ResultadoRefinamento resultado = enumerarGrupo(inst, sol, alternativas, parametros);
```

As quantidades devem somar a demanda real de cada fluxo. Rotas devem pertencer
ao fluxo e garantir cobertura de todos os cenarios. A lista atual e inserida
automaticamente, duplicatas equivalentes sao removidas e o limite de alternativas
inclui a atual. Listas explicitas acima desse limite sao rejeitadas.

A enumeracao testa combinacoes completas, sempre partindo da mesma entrada.
Estados parciais nao sao rejeitados pelo minimo comercial. Cada combinacao
ajusta y pelas mesmas regras de `alterarAlocacoes`; nao cria novas reutilizacoes.
O custo cresce com o produto dos tamanhos das listas; com seis alternativas
sao ate 36 combinacoes para dois fluxos e 216 para tres.

`candidatosTestados` conta combinacoes visitadas, inclusive a atual e propostas
rejeitadas antes da avaliacao completa. `concluido` indica o fim dessas listas,
nao otimalidade global. Se tempo ou numero de combinacoes encerrar a busca,
retorna `concluido = false` e instala qualquer melhoria ja encontrada. O limite
de tempo inclui preparacao e validacao e e checado entre etapas; uma avaliacao
em andamento nao e interrompida. A copia para validacao da entrada e obrigatoria.

## Verificacao

Testes diferenciais com enumeracao de todas as quantidades inteiras, grupos,
reutilizacao, minimos comerciais, manutencao, limites de busca, preservacao do
RNG e as tres instancias oficiais estao em `tests/refinamento.cpp`.
Execute a partir da raiz do repositorio no PowerShell:

```powershell
New-Item -ItemType Directory -Force bin | Out-Null
$biblioteca = @(Get-ChildItem src -Recurse -Filter '*.cpp' | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName)
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/refinamento.cpp @biblioteca -o bin/teste_refinamento.exe
./bin/teste_refinamento.exe
```

O teste novo tem seu proprio `main` e e compilado separadamente do teste
`funcoes_basicas.cpp`, para preservar o alvo `make test` existente.

## Simulacao sem construtiva ou SA

`tests/simulacao_refinamento.cpp` inicia em SPOT e visita ate 100 grupos
aleatorios de tres fluxos por execucao. Apos cada grupo, refina as quantidades
dos seus fluxos. Usa um RNG local; nao altera o gerador compartilhado.
O primeiro argumento limita segundos por execucao e o segundo define quantas
sementes testar em cada uma das tres instancias (padroes: 5 segundos e 3 sementes).

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/simulacao_refinamento.cpp @biblioteca -o bin/simulacao.exe
./bin/simulacao.exe 5 3
python tests/verificar_simulacao.py
```

O ensaio grava `output/simulacao/resultados.csv` e um JSON oficial por execucao.
Reexecutar substitui esses arquivos. Os tempos medem apenas a busca, incluindo
validacoes dos refinadores; excluem leitura, SPOT inicial, conferencia final e
exportacao. O CSV separa ganhos e tempos dos grupos e dos ajustes de quantidade.
O verificador Python calcula custos e checa restricoes sem chamar o codigo C++.

Esta simulacao nao mede a qualidade da futura construtiva ou do SA. Parte de
uma baseline cara, usa grupos aleatorios e nao cria reutilizacoes. Melhorias
estritas podem estagnar quando faltam mudancas conjuntas maiores ou diversificacao.

## Comparacao da selecao dos grupos

`tests/comparar_grupos.cpp` compara tres estrategias com as mesmas sementes,
mesma SPOT inicial e mesmo orcamento de busca (padrao: 1 segundo e 5 sementes
por estrategia e instancia, totalizando 45 execucoes). Alterna a ordem das
estrategias entre sementes. Nao altera os modulos da construtiva, SA ou refinamento.

- `aleatoria`: preserva a selecao do primeiro ensaio, embaralhando todos os
  fluxos e percorrendo trios sem repeticao antes de embaralhar novamente.
- `companhia`: em 70% das chamadas sorteia uma companhia regular, restringe
  a fluxos com ofertas dessa companhia e prioriza a diferenca entre seu custo
  de transporte no pior cenario atual e o custo da melhor oferta da companhia.
  Sorteia tres fluxos entre os 20 prioritarios. Agrupa por companhia sem obrigar
  o refinador a escolher suas rotas.
- `pior_cenario`: em 70% das chamadas sorteia tres dos 20 fluxos de maior custo
  de transporte no pior cenario atual. Contratos e reutilizacoes nao entram
  nesse ranking; todas as restricoes e custos entram na avaliacao do candidato.

As duas dirigidas usam grupos totalmente aleatorios nos outros 30% das chamadas
(probabilidade por chamada, nao cota exata). O tempo inclui selecao, preparacao
de ofertas, refinamento de grupos e quantidades. Leitura, SPOT inicial e escrita
ficam fora. A quantidade de grupos varia para respeitar o mesmo orcamento de
tempo. Sementes controlam os sorteios, mas a parada por tempo pode variar entre
execucoes. Nenhuma estrategia cria reutilizacoes nesse ensaio.

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/comparar_grupos.cpp @biblioteca -o bin/comparar_grupos.exe
./bin/comparar_grupos.exe 1 5
python tests/verificar_simulacao.py output/comparacao_grupos
python tests/resumir_comparacao.py
```

Resultados brutos, resumo e JSONs por execucao ficam em
`output/comparacao_grupos`. Reexecutar substitui esses resultados, preservando
os arquivos do ensaio anterior em `output/simulacao`. Cinco sementes e um segundo
constituem um ensaio inicial, sem garantir o mesmo desempenho apos integracao ao SA.
