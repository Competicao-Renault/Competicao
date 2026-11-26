# Competicao

Projeto C++17 para o KIRO 2026.

As estruturas estao em `include/definicao_problema/instancia.hpp`. A funcao
`lerInstancia(caminho)` retorna os dados validados ou lanca `std::runtime_error`
com o caminho do arquivo e o motivo do erro. IDs correspondem aos indices dos
vetores; custos, capacidades e demandas usam inteiros de 64 bits.

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
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc src/main.cpp src/definicao_problema/instancia.cpp src/utilitarios/aleatorio.cpp -o bin/competicao.exe
./bin/competicao.exe instancia/alliances.json instancia/reliability.json instancia/strikes.json
```

Em ambiente com GNU Make e ferramentas Unix, use `make` e execute
`./bin/competicao instancia/alliances.json`. Por enquanto, o executavel apenas
le e apresenta o resumo das instancias; ainda nao gera solucoes.
