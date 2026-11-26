# Refinamento local

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
