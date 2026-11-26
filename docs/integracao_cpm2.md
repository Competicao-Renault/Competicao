# Integracao cpm2

O ZIP continha 21 blocos de conflito em 11 arquivos. Nos trechos de SA,
movimentos, CLI, testes e documentacao, a alternativa `origin/FuncoesBasicas`
coincidia com o codigo local. Essa versao foi preservada, incluindo oito
familias de movimentos, migracao robusta, RNG isolado e refinamento depois do SA.
Nao foram importados marcadores, metadados Git, binarios ou objetos compilados.

A novidade independente era `busca_tabu`. Foi adaptada para o fluxo atual como
etapa opcional entre SA e refinamento, sem mudar `Solucao`. O arquivo local
`output/solucoes/alliances.json`, que ja estava modificado, foi preservado.

## Busca tabu

Mantem os conceitos do ZIP: vizinhanca amostrada, memoria de rotas/quantidades,
hash canonico, duracao aleatoria e criterio de aspiracao. A busca pode aceitar
pioras na cadeia, mas devolve apenas a melhor incumbente viavel.

As correcoes incluem:

- Validacao de parametros e reavaliacao da entrada antes de buscar.
- Prazo checado antes de cada proposta, com uma tentativa por proposta. O ZIP
  fazia ate 30 tentativas por vizinho sem verificar o prazo dentro da vizinhanca.
- Limites adicionais de iteracoes e propostas, permitindo ensaios reproduziveis.
- Uso efetivo dos hashes recentes para bloquear revisitas; no ZIP a lista era
  consultada apenas na aspiracao. Trocar a ordem dos slots nao conta como visita.
- Restricoes de quantidade associadas ao ID da rota, evitando confundir slots
  apos substituicoes. Reentrada de rotas e proibida ate a expiracao da restricao.
- Aspiracao quando o candidato melhora a melhor solucao global.
- Amostragem uniforme das oito familias existentes, incluindo reutilizacao e
  grupos dirigidos. O operador grupo mantem 30% de escolhas aleatorias.

Sem candidato admissivel, reinicia memorias, mantendo o estado corrente e a
incumbente. O tempo e verificado entre propostas; nao interrompe um movimento
em andamento. Reutilizacoes entram no hash independentemente da ordem do vetor.

## Uso

```powershell
./bin/competicao.exe --semente 7 --sa-segundos 1 --tabu --tabu-segundos 0.5 --refinamento-segundos 0.3 instancia/alliances.json
./bin/competicao.exe --sem-sa --tabu-segundos 1 instancia/reliability.json
```

Opcoes: `--tabu`, `--sem-tabu`, `--tabu-segundos`, `--tabu-lista`,
`--tabu-vizinhos`, `--tabu-iteracoes` e `--tabu-max-propostas`. Uma opcao com
prefixo `--tabu-` ativa a etapa; `--sem-tabu` posterior a desativa.
Padroes: lista 30, vizinhanca 20, 1000 iteracoes, 100.000 propostas e 1 segundo.
Tempo, propostas ou iteracoes iguais a zero retornam a entrada reavaliada.
Lista e vizinhanca precisam ser positivas. A etapa fica desligada por padrao:
adicionar uma heuristica nao demonstra vantagem sob o mesmo tempo total.

```cpp
ParametrosExecucao p;
p.executarTabu = true;
p.tabu.tempoMax = 0.5;
auto resultado = executarFluxo(inst, p);
```

A API `busca_tabu(inst, sol, tamanhoLista, tamanhoVizinhanca, tempoMax)` foi
preservada. A nova `buscaTabu(inst, sol, parametros, &estatisticas)` inclui
contadores. Ambas usam o gerador compartilhado quando chamadas diretamente;
o fluxo controla a semente e restaura o RNG, como ja fazia para o SA.

## Resultados historicos do ZIP

Os tres JSONs em `resultados/melhores` do ZIP foram conferidos independentemente
com o verificador Python atual, em todos os cenarios. Seus custos sao
308.995.986 (alliances), 314.812.464 (reliability) e 361.845.553 (strikes).
Copias e metadados ficam em `output/cpm2/historico`, sem substituir solucoes
locais. Esses sao resultados historicos fornecidos, nao novas execucoes.

Os hashes registrados para SA e movimentos nao coincidiram com os fontes
atuais nem com a resolucao HEAD dos arquivos conflitantes. Por isso, nao se
atribui essa campanha a versao integrada nem se adotam suas conclusoes como
comparacao entre as versoes. As campanhas de 10 mil, 100 mil e 1 milhao de
propostas devem ser refeitas com snapshots e hashes dos fontes efetivamente usados.

## Verificacao

`tests/simulated_annealing.cpp` inclui testes da tabu: hash canonico, limites,
reproducao, caches, preservacao da incumbente e do RNG, interface original e
fluxo combinado. `tests/cli_fluxo.py` testa a etapa pela linha de comando,
incluindo argumentos invalidos e caminhos Unicode. Os modulos existentes de
construcao e refinamento mantem seus testes de regressao.
