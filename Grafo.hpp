#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>
#include <iosfwd>

struct Vertice {
    double x;
    double y;
    double beneficio;
};

class Grafo {
public:
    Grafo(const std::vector<Vertice>& vertices, double limiteCusto);

    // Contrato da leitura a ser implementada pelo Fabricio.
    static Grafo ler_instancia(std::istream& entrada);
    double custo_rota(const std::vector<int>& rota) const;
    double pontuacao_rota(const std::vector<int>& rota) const;
    bool rota_viavel(const std::vector<int>& rota) const;

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
