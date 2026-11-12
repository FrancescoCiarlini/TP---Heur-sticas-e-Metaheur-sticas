#pragma once

#include "Grafo.hpp"  // Ajuste para o cabecalho do grupo.
#include <algorithm>
#include <chrono>
#include <random>

// Cada funcao atualiza rota e retorna a pontuacao dessa rota.
double ILS(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double rho = 0.2,
    int L = 1000,
    unsigned int semente = 42
);

double GRASP(const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double alpha = 0.25,
    unsigned int semente = 42,
    TipoConstrucao construcaoInicial = TipoConstrucao::InsercaoGulosa
);

double SA(const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido);

// Seleciona e executa a metaheuristica.
double busca(const Grafo& g,
    std::vector<int>& rota,
    int meta,
    double tempoCorrido);

//busca local utilizada pelo ILS e GRASP
double busca_local(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    const std::vector<int>& bloqueados = {}
);
