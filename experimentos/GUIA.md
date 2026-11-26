# Bancada experimental

No WSL, na raiz do repositório, execute:

```bash
bash experimentos/testar.sh
```

É preciso ter Bash, Git, g++ com C++17 e Python 3. O menu compila o
executável experimental em um diretório temporário. A compilação usa os
fontes disponíveis na cópia de trabalho.

1. Escolha um dos oito fluxos e selecione uma instância ou todas.
2. Digite as sementes separadas por vírgula ou espaço. ENTER mantém
   `7,42,2026,11,19`.
3. Configure o construtivo uma vez e informe quantas configurações
   deseja comparar. ENTER aceita o valor mostrado entre colchetes.
   Use ponto como separador decimal.
4. Para o SA, selecione um perfil de pesos ou informe os oito valores.
   Peso zero desativa um operador. Pelo menos um peso deve ser positivo.
   A previsão dos níveis usa o modelo de resfriamento geométrico.
5. Para desativar o refinamento em um fluxo que o oferece, informe
   tempo de refinamento zero. Zero grupos mantém apenas os ajustes
   de quantidade, se houver tempo positivo.

O modo 7 executa SA e Tabu a partir da mesma solução inicial, com a mesma
semente e o mesmo orçamento de tempo, sem refinamento. Os limites de cada
algoritmo são configurados separadamente e podem encerrar a busca antes do
prazo. O CSV registra o motivo do SA e o limite inferido para a Tabu, cuja
API não expõe o motivo. O modo 8 executa SA, depois Tabu e refinamento.
A construção é compartilhada por todas as configurações de um mesmo par
instância–semente. Os geradores são reinicializados antes de cada etapa.

O modo 9 aplica E4 nas três instâncias com o construtivo e o SA.
O tempo do SA é informado no menu. Os níveis contam candidatos avaliados,
inclusive inviáveis. O custo registrado como melhor vem de soluções viáveis.

Cada campanha cria um diretório novo em `experimentos/resultados/` com
parâmetros, versão, hashes, log, CSVs e soluções. `validado.txt` indica que
as soluções passaram pelo avaliador completo e pelo verificador
independente. Consulte `resumo.csv` para as estatísticas agregadas e
`pares.csv` para as diferenças pareadas por semente. O tempo médio inclui
a construção e as etapas habilitadas. Com uma única semente, o desvio
padrão amostral fica em branco. Quando as três instâncias são executadas,
`scores.csv` soma os três melhores custos robustos por configuração. O
terminal imprime os três custos individuais e o `SCORE TOTAL`.

O verificador independente está em `tests/verificar_simulacao.py`.
A campanha histórica do E4 obteve Alliances 285411123, Reliability 292384403
e Strikes 290238690, com total de 868034216. A versão atual usa a
função penalizada durante a busca e pode produzir resultados diferentes.
