#ifndef BUSCA_TABU_HPP
#define BUSCA_TABU_HPP

#include "definicao_problema/solucao.hpp"

// Busca tabu com vizinhança amostrada aleatoriamente. 'sol' deve ser viável e recebe a melhor solução
// encontrada. tamanhoLista: duração média das entradas tabu e tamanho da memória de hashes;
// tamanhoVizinhanca: vizinhos sorteados por iteração. Retorna o objetivo da solução final.
double busca_tabu(const Instancia &inst, Solucao &sol, int tamanhoLista, int tamanhoVizinhanca, double tempoMax);

#endif