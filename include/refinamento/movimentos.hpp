#ifndef MOVIMENTOS_HPP
#define MOVIMENTOS_HPP

#include "definicao_problema/solucao.hpp"
#include <utility>

bool parRobusto(const Instancia &inst, int rota1, int rota2 = -1);
// Movimentos so alteram a solucao se o resultado for viavel; custos sao atualizados.
// Reutilizacoes existentes sao reduzidas quando necessario, sem criar novos pares.
bool alterarAlocacoes(const Instancia &inst, Solucao &solucao, const std::vector<std::pair<int, AlocacaoFluxo>> &alteracoes, std::string *motivo = nullptr);
bool alterarAlocacao(const Instancia &inst, Solucao &solucao, int fluxo, AlocacaoFluxo alocacao, std::string *motivo = nullptr);
// Delta positivo transfere da rota2 para a rota1; negativo faz o inverso.
bool transferirQuantidade(const Instancia &inst, Solucao &solucao, int fluxo, int delta, std::string *motivo = nullptr);
bool substituirRota(const Instancia &inst, Solucao &solucao, int fluxo, int posicao, int novaRota, std::string *motivo = nullptr);
// Quantidade absoluta do par; zero remove. Nao reduz outros pares para abrir espaco.
bool definirReutilizacao(const Instancia &inst, Solucao &solucao, int origem, int destino, int quantidade, std::string *motivo = nullptr);
bool movimentoAleatorio(const Instancia &inst, Solucao &solucao, int tentativas = 30);

#endif
