#include "aleatorio.hpp"

// O gerador inline do header e compartilhado pelas funcoes e pelo embaralhamento.


void semente(unsigned valor)
{
    gerador.seed(valor);
}

void sementeAleatoria()
{
    std::random_device fonteDeEntropia;
    gerador.seed(fonteDeEntropia());
}

int inteiroAleatorio(int minimo, int maximo)
{
    std::uniform_int_distribution<int> distribuicao(minimo, maximo);
    return distribuicao(gerador);
}

double realAleatorio(double minimo, double maximo)
{
    std::uniform_real_distribution<double> distribuicao(minimo, maximo);
    return distribuicao(gerador);
}



