#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>

struct Vertice {
    double x;
    double y;
    double beneficio;
};

class Grafo {
public:
    Grafo(const std::vector<Vertice>& vertices, double limiteCusto);

    int numero_vertices() const;
    int origem() const;
    int destino() const;

    double custo(int i, int j) const;
    double beneficio(int i) const;
    double limite_custo() const;

private:
    std::vector<Vertice> vertices_;
    std::vector<std::vector<double>> custos_;

    double limiteCusto_;
};