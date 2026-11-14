#pragma once

#include "Grafo.hpp"
#include <vector>

// Define qual heuristica sera usada para gerar a solucao inicial.
enum class TipoConstrucao {
    InsercaoGulosa,
    MST
};

// Constroi uma rota inicial usando a insercao gulosa.
double insercao_gulosa(
    const Grafo& g,
    std::vector<int>& rota
);

// Constroi uma rota inicial usando MST com reparacao e insercao gulosa
double mst_com_reparacao(
    const Grafo& g,
    std::vector<int>& rota
);

// Executa a construcao inicial escolhida.
double construir_solucao_inicial(
    const Grafo& g,
    std::vector<int>& rota,
    TipoConstrucao tipo
);
