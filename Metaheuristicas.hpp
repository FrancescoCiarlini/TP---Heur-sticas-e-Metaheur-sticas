#pragma once

#include "Grafo.hpp"
#include "HeuristicasConstrutivas.hpp"
#include <algorithm>
#include <chrono>
#include <random>

struct RegistroEvolucao {
    double tempo;
    double pontuacao;
    double custo;
};

// Cada funcao atualiza rota e retorna a pontuacao dessa rota.
double ILS(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double rho = 0.2,
    int L = 1000,
    unsigned int semente = 42,
    std::vector<RegistroEvolucao>* historico = nullptr
);

double GRASP(const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double alpha = 0.25,
    unsigned int semente = 42,
    TipoConstrucao construcaoInicial = TipoConstrucao::InsercaoGulosa,
    std::vector<RegistroEvolucao>* historico = nullptr
);

double SA(const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    unsigned int semente = 42,
    int L = 1000,
    std::vector<RegistroEvolucao>* historico = nullptr);

// Seleciona e executa a metaheuristica.
double busca(const Grafo& g,
    std::vector<int>& rota,
    int meta,
    double tempoCorrido,
    unsigned int semente = 42,
    TipoConstrucao construcaoInicial = TipoConstrucao::InsercaoGulosa,
    double rho = 0.2,
    int L = 1000,
    double alpha = 0.25,
    std::vector<RegistroEvolucao>* historico = nullptr);

//busca local utilizada pelo ILS e GRASP
double busca_local(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    const std::vector<int>& bloqueados = {},
    std::vector<RegistroEvolucao>* historico = nullptr
);
