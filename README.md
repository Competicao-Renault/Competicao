# Competicao

Projeto C++17 para o KIRO 2026.

As estruturas estao em `include/definicao_problema/instancia.hpp`. A funcao
`lerInstancia(caminho)` retorna os dados validados ou lanca `std::runtime_error`
com o caminho do arquivo e o motivo do erro. IDs correspondem aos indices dos
vetores; custos, capacidades e demandas da instancia usam inteiros de 64 bits.
Cada demanda deve caber em `int`, conforme a representacao das alocacoes.

O leitor preserva os metadados e as localizacoes e prepara:

- `companhiaSpot` e `portoSpot`, identificados pelo nome e codigo oficiais;
- `demandaTotal`, em TEU;
- `custosRodoviarios[site][porto]`, com -1 para acessos inexistentes;
- `custosRotas[rota]`, incluindo transporte maritimo e quatro trechos terrestres;
- `indisponivel[cenario][rota]`, com 1 para rotas que falham.

O parser JSON e interno, sem dependencias externas. A leitura verifica os tipos,
as dimensoes, os IDs consecutivos, as referencias, os acessos rodoviarios, a
presenca de SPOT e de cenario nominal. Isso valida a entrada, nao uma solucao.

Compilacao no PowerShell com g++ disponivel:

```powershell
New-Item -ItemType Directory -Force bin | Out-Null
$fontes = @(Get-ChildItem src -Recurse -Filter '*.cpp' | ForEach-Object FullName)
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc @fontes -o bin/competicao.exe
./bin/competicao.exe instancia/alliances.json instancia/reliability.json instancia/strikes.json
./bin/competicao.exe --saida bin/alliances-solucao.json instancia/alliances.json
```

Em ambiente com GNU Make e ferramentas Unix, use `make` e execute
`./bin/competicao instancia/alliances.json`. O executavel cria uma solucao por
blocos, executa o refinamento final e exporta o JSON oficial `{x, y}` em
`output/solucoes/`. `--saida` permite escolher o arquivo para uma unica instancia.
A etapa SA e opcional e ainda nao esta conectada. Consulte
[construcao e fluxo](docs/construcao_fluxo.md) para integrar o callback e ajustar
sementes e orcamentos de tempo.

## Solucoes e movimentos

`solucao.hpp` define `AlocacaoFluxo`, `Reutilizacao` e `Solucao`. A posicao em
`alocacoes` corresponde ao ID do fluxo. A primeira rota tem quantidade positiva;
uma segunda rota ausente e representada por `rota2 = -1, quantidade2 = 0`.

`avaliarSolucao(inst, sol, &motivo)` recalcula custo e viabilidade em todos os
cenarios, incluindo redirecionamento, contratos e perda de reutilizacoes. Em
solucoes viaveis, `piorCenario` e o primeiro cenario de maior custo; nas inviaveis,
indica o primeiro cenario que falhou, ou -1 para erro na estrutura nominal.
O objetivo de uma solucao inviavel e infinito; seus custos parciais nao devem
ser usados como avaliacao completa.

`movimentos.hpp` oferece alteracao de alocacoes individuais ou em bloco,
transferencia de quantidade, substituicao de rota, edicao de reutilizacao e
movimento aleatorio com numero limitado de tentativas. Todos retornam `bool`:
true aplica uma mudanca viavel, false preserva a solucao original. Isso aceita
tanto melhorias quanto pioras de custo; a decisao de aceite cabe a futura busca.
Mudancas de alocacao limitam reutilizacoes existentes aos novos volumes, na
ordem do vetor, e removem pares zerados. Nao ha otimizacao automatica de `y`.

```cpp
Solucao sol = criarSolucaoSpot(inst);
alterarAlocacao(inst, sol, fluxo, {rotaRegular, rotaSpot, 100, 50});
avaliarSolucao(inst, sol);
salvarSolucao(inst, sol, "solucao.json");
```

`movimentoAleatorio` usa o gerador compartilhado de `aleatorio.hpp`; chame
`semente(valor)` para reproduzir experimentos. A avaliacao e completa, ainda
sem atualizacao incremental. O gerador aleatorio altera um fluxo por proposta;
para ativar companhias em conjunto use `alterarAlocacoes`.

## Testes

Use `make test` em ambiente Unix ou execute no PowerShell:

```powershell
$biblioteca = @($fontes | Where-Object { [IO.Path]::GetFileName($_) -ne 'main.cpp' })
g++ -std=c++17 -O0 -g -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/funcoes_basicas.cpp @biblioteca -o bin/testes.exe
./bin/testes.exe
```

Execute a partir da raiz do repositorio. Os testes cobrem custos conhecidos,
falhas, restricoes comerciais, reutilizacao, movimentos atomicos,
reprodutibilidade por semente e as tres instancias oficiais.
