# Construcao inicial e fluxo de execucao

`construcao.hpp` define a construtiva; `fluxo.hpp` coordena as etapas. A
representacao `Solucao`, o avaliador e os movimentos existentes foram preservados.
O SA de `_com1.0.zip` foi integrado em `meta_heuristicas/simulated_annealing`.
Os `.cpp` entram automaticamente na compilacao do Makefile. Detalhes dos
movimentos e opcoes estao em [integracao do SA](integracao_sa.md).

## Construtiva

```cpp
ParametrosConstrucao p;
p.limiteSegundos = 1.0;
EstatisticasConstrucao dados;
Solucao sol = construirSolucao(inst, p, 42, &dados);
```

Comeca em SPOT e nunca aumenta o custo robusto. Candidatos viaveis sao
comparados lexicograficamente por custo robusto, soma dos custos dos cenarios
e volume nominal na SPOT. O segundo criterio equivale a comparar medias, pois
o numero de cenarios nao muda. Empates uteis permitem sair de plateaus da SPOT.

Cada bloco comeca com ate tres fluxos. A selecao usa 30% de sorteios totalmente
aleatorios e divide o restante entre companhia e custo de transporte no pior
cenario. Cada estrategia dirigida sorteia entre ate 20 prioritarios. Sem
companhia elegivel, usa a prioridade pelo pior cenario. Ao falhar, amplia o
bloco ate `maximoFluxosBloco` (padrao 6); ao aceitar, retorna ao tamanho inicial.

As alternativas incluem a atual, SPOT, rotas individuais imunes e pares
robustos. Quantidades usam extremos positivos, 25/50/75%, necessidades minimas
e folgas das companhias nos cenarios em que ambas as rotas sobrevivem. Tambem
consideram parcelas proporcionais a demanda do bloco. Esses pontos sao pistas
de construcao, nao uma prova de viabilidade: a avaliacao completa decide.

Mantem ate oito alternativas por fluxo e amostra ate 64 propostas completas por
bloco, incluindo uma combinacao das primeiras ofertas novas. Guarda as cinco
melhores propostas que superam a solucao atual e sorteia uma delas (RCL).
Tempos e contagens sao configuraveis. Nao enumera todas as combinacoes nem
otimiza globalmente as quantidades. Nao cria reutilizacoes nessa versao.

O RNG e local e nao altera `semente()` nem sorteios do SA. Sementes reproduzem
os sorteios; a parada por tempo pode mudar os resultados. Para reproduzir um
teste exatamente, limite blocos e propostas e forneca tempo suficiente.
Orcamento zero retorna SPOT. Parametros invalidos lancam `invalid_argument`.

## Ponto de integracao do SA

```text
lerInstancia -> construirSolucao -> SA integrado ou callback
            -> refinarQuantidades/grupos -> avaliarSolucao -> salvarSolucao
```

`executarFluxo(inst, parametros)` executa o SA integrado por padrao. Para pular
essa etapa, use `parametros.executarSA = false`. Um callback explicito tem
prioridade sobre essa opcao e sobre o SA integrado. Seu contrato permanece:

```cpp
using EtapaSA = std::function<void(const Instancia &, Solucao &, unsigned)>;
```

Para substituir o SA integrado por uma implementacao alternativa:

```cpp
EtapaSA etapa = [&](const Instancia &i, Solucao &s, unsigned seed) {
    // Chame a implementacao real do SA aqui, atualizando s.
    // Exemplo, ajustando a assinatura: s = executarSA(i, s, parametrosSA, seed);
};
auto resultado = executarFluxo(inst, parametros, etapa);
```

O exemplo acima e somente o ponto de adaptacao; nao e uma implementacao de SA.
O fluxo passa uma copia da construcao e a semente. Reavalia a saida do callback,
rejeita resultados inviaveis com erro e preserva a incumbente se o custo piorar.
O callback deve guardar internamente a melhor solucao do SA, nao apenas o ultimo
estado da cadeia. Seu tempo deve ser controlado pelos parametros proprios do SA.

Depois ha uma passagem de quantidades e grupos de ate tres fluxos, usando
a mesma selecao dirigida com parcela aleatoria. Padroes: 0,5 segundo e ate
100 grupos. O refinamento so aceita melhorias estritas. Os limites de tempo
sao verificados entre operacoes, sem interromper uma avaliacao em andamento.
O resultado traz custos por etapa, tempos, contagens e a solucao final validada.

## Executavel

O programa agora construi e refina antes de exportar. Sem `--saida`, grava em
`output/solucoes/<nome-do-arquivo>.json`, substituindo esse destino ao reexecutar.
`--saida` aceita uma unica instancia. As opcoes podem vir antes ou depois dela:

```powershell
./bin/competicao.exe --semente 42 --construcao-segundos 1 --refinamento-segundos 0.5 instancia/alliances.json
./bin/competicao.exe --saida output/minha-solucao.json instancia/reliability.json
./bin/competicao.exe instancia/alliances.json instancia/reliability.json instancia/strikes.json
```

Os custos de construcao, apos SA e finais sao distintos no console. `SA=false`
informa que a etapa foi desativada. Os JSONs continuam no formato oficial
`{x, y}`, sem campos de diagnostico. Leitura, validacao final e exportacao estao
fora dos orcamentos de busca; a construtiva inclui a criacao da sua SPOT inicial.

## Testes

```powershell
$biblioteca = @(Get-ChildItem src -Recurse -Filter '*.cpp' | Where-Object Name -ne 'main.cpp' | ForEach-Object FullName)
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude -Isrc tests/construcao_fluxo.cpp @biblioteca -o bin/teste_construcao.exe
./bin/teste_construcao.exe
python tests/cli_fluxo.py
```

Cobrem desempates, saida de plateau, RNG, selecao sem repeticao, reproducao
por blocos, fallback SPOT, callback ausente, melhoria/piora/inviabilidade do
callback, refinamento final e as tres instancias oficiais. O callback de teste
somente injeta solucoes conhecidas; nao simula a qualidade de um SA real.
O teste de CLI cobre opcoes invalidas e leitura/exportacao em caminhos absolutos
com acentos. As APIs de arquivos recebem caminhos UTF-8 e usam filesystem para
abrir os arquivos no Windows.
