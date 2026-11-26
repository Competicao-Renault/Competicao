# Integracao de _com1.0.zip

Foram aproveitados o SA e as operacoes adicionais de movimentos do ZIP. Os
modelos de instancia e solucao eram os mesmos. A importacao foi seletiva: nao
incluiu `.git`, executaveis, objetos compilados ou resultados antigos do ZIP.
Quebras CRCRLF foram normalizadas para nao duplicar linhas.

## Incompatibilidades resolvidas

- O SA do ZIP chamava somente `movimentoAleatorio`, sem usar as novas familias.
  Agora sorteia operadores por pesos, com uma tentativa por proposta contabilizada.
- O `main` do ZIP proibia refinamento depois de SA. O fluxo integrado preserva
  construcao -> SA -> refinamento -> validacao -> exportacao.
- A migracao escolhia a rota mais barata sem verificar cobertura robusta e
  podia migrar apenas parte dos fluxos pedidos. Agora considera cobertura e
  compatibilidade com a segunda rota, rejeita falta de ofertas atomica e pode
  consolidar dois volumes numa unica rota imune.
- Foram mantidos a API de Solucao, os movimentos basicos e o callback existente.
  O SA original continua disponivel usando somente o primeiro peso.

## Execucao

SA e habilitado por padrao, com temperatura inicial 1.000.000, final 10.000,
alfa 0,95, 200 candidatos viaveis por nivel, 100.000 propostas e 1 segundo.
Esses valores sao configuracoes iniciais, nao parametros calibrados. O limite
de tempo padrao foi reduzido dos 60 segundos do `main` do ZIP para permitir
execucoes curtas do fluxo; pode ser alterado pela linha de comando.

```powershell
./bin/competicao.exe --semente 7 --sa-segundos 5 instancia/alliances.json
./bin/competicao.exe --sem-sa instancia/alliances.json
./bin/competicao.exe --sa --sa-segundos 2 --refinamento-segundos 0.5 instancia/reliability.json
```

Opcoes aceitas: `--sa-temperatura-inicial`, `--sa-temperatura-final`, `--sa-alfa`,
`--sa-max`, `--sa-max-propostas`, `--sa-segundos`, `--sa-pesos` e
`--sa-aleatorio-grupos`. A semente e a mesma informada em `--semente`, sem
substituicao silenciosa pelo valor 7. Use `--sem-sa` para comparar sem essa etapa.

Os oito pesos, na ordem, sao: aleatorio original, divisao, substituicao,
companhia, site, reutilizacao, grupo e quantidade. Padrao: `1,2,2,1,1,1,1,2`.
Peso zero desabilita uma familia; pesos negativos e soma zero sao rejeitados.
No operador grupo, 30% dos trios sao aleatorios e o restante divide-se entre
companhia e pior cenario. Essa proporcao refere-se apenas ao operador grupo.

```powershell
./bin/competicao.exe --sa-pesos 1,0,0,0,0,0,0,0 --sa-segundos 1 instancia/strikes.json
```

## Contratos e estatisticas

`simulatedAnnealing` preserva a assinatura do ZIP e retorna a melhor solucao
viavel, sem modificar a entrada. Aceita pioras na cadeia por Metropolis e mantem
uma incumbente separada. Nunca devolve o ultimo estado se ele for pior que a
incumbente. Tempo, propostas e temperatura podem encerrar a busca; o prazo e
checado entre propostas, sem interromper movimentos em andamento.

Uma proposta invalida consome o limite global, mas nao os candidatos viaveis
por temperatura. Sao registrados propostas, viaveis e aceitos por familia,
alem dos contadores de melhorias, empates, pioras, recordes e niveis do ZIP.
O console mostra o custo apos SA separadamente do custo apos refinamento.

O SA direto usa o gerador compartilhado, preservando a interface do ZIP:
chame `semente(valor)` antes de invoca-lo. `executarFluxo` faz isso em escopo
temporario e restaura o estado anterior, inclusive em excecoes. Construcao e
refinamento continuam com seus geradores locais. O RNG compartilhado exige
sincronizacao externa se o SA direto for usado concorrentemente.

`ParametrosExecucao.sa` controla o SA integrado; `ResultadoExecucao.estatisticasSA`
traz seus contadores. Um callback externo continua tendo prioridade e nao
preenche automaticamente as estatisticas do SA integrado.

## Testes

`make test` inclui os testes basicos e `tests/simulated_annealing.cpp`. No Windows:

```powershell
$biblioteca = @(Get-ChildItem src -Recurse -Filter '*.cpp' | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName)
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/simulated_annealing.cpp @biblioteca -o bin/testes_sa.exe
./bin/testes_sa.exe
python tests/cli_fluxo.py
```

Os testes originais do ZIP usam somente o peso aleatorio para preservar seus
sorteios exatos. Os adicionais exercitam cada familia isoladamente e a mistura,
incluindo reproducao, atomicidade, reutilizacao, migracao robusta, contadores,
aceite de pioras, retorno da incumbente e integracao com refinamento final.
