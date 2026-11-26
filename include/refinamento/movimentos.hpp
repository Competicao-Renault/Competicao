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

// Divide o fluxo entre rotaA (quantidadeA) e rotaB (resto); quantidadeA = 0 ou demanda deixa uma unica rota.
bool redividirFluxo(const Instancia &inst, Solucao &solucao, int fluxo, int rotaA, int rotaB, int quantidadeA, std::string *motivo = nullptr);
// Troca a rota 1 e/ou a rota 2 de um fluxo (-1 mantem a atual), preservando as quantidades.
bool substituirPar(const Instancia &inst, Solucao &solucao, int fluxo, int novaRota1, int novaRota2 = -1, std::string *motivo = nullptr);
// Move os fluxos listados da companhia "de" (-1 = qualquer) para "para", usando a rota mais barata de "para".
bool migrarFluxos(const Instancia &inst, Solucao &solucao, const std::vector<int> &fluxos, int de, int para, std::string *motivo = nullptr);
bool trocarCompanhia(const Instancia &inst, Solucao &solucao, int de, int para, std::string *motivo = nullptr); // Todos os fluxos de "de".
bool unificarSite(const Instancia &inst, Solucao &solucao, int site, int companhia, std::string *motivo = nullptr); // Todos os fluxos do site.
// Versoes sorteadas: aplicam a primeira proposta viavel em ate "tentativas" sorteios.
bool movimentoDivisao(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool movimentoSubstituicao(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool movimentoCompanhia(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool movimentoSite(const Instancia &inst, Solucao &solucao, int tentativas = 30);
<<<<<<< HEAD
bool movimentoQuantidade(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool movimentoReutilizacao(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool movimentoGrupo(const Instancia &inst, Solucao &solucao, int tentativas = 30, int percentualAleatorio = 30);
// Compensa o volume nominal de uma companhia entre dois fluxos, atomicamente.
bool compensarFluxos(const Instancia &inst, Solucao &solucao, int fluxo1, int fluxo2, int companhia, int delta, std::string *motivo = nullptr);
bool movimentoCompensado(const Instancia &inst, Solucao &solucao, int tentativas = 30);
bool fecharContrato(const Instancia &inst, Solucao &solucao, int companhia, int site, int destino, std::string *motivo = nullptr);
bool movimentoContrato(const Instancia &inst, Solucao &solucao, int tentativas = 30);
=======
>>>>>>> 9ae27bf60f46b7eb264913aaca7cbf8aa5fc414c

#endif
