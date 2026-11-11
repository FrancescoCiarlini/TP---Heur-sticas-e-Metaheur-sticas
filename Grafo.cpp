#include "Grafo.hpp"

Grafo::Grafo(const std::vector<Vertice>& vertices, double limiteCusto)
    : vertices_(vertices),
    limiteCusto_(limiteCusto)
{
    if (vertices_.size() < 2) {
        throw std::invalid_argument(
            "O grafo precisa ter pelo menos origem e destino."
        );
    }

    if (!std::isfinite(limiteCusto_) || limiteCusto_ < 0.0) {
        throw std::invalid_argument(
            "O limite de custo deve ser finito e nao negativo."
        );
    }

    for (const Vertice& v : vertices_) {
        if (!std::isfinite(v.x) ||
            !std::isfinite(v.y) ||
            !std::isfinite(v.beneficio) ||
            v.beneficio < 0.0)
        {
            throw std::invalid_argument(
                "Coordenadas devem ser finitas e beneficios "
                "devem ser finitos e nao negativos."
            );
        }
    }

    // Origem e destino nao contribuem para a pontuacao.
    vertices_.front().beneficio = 0.0;
    vertices_.back().beneficio = 0.0;

    const int n = numero_vertices();

    // A diagonal permanece com custo zero.
    custos_.assign(n, std::vector<double>(n, 0.0));

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const double dx = vertices_[i].x - vertices_[j].x;
            const double dy = vertices_[i].y - vertices_[j].y;

            const double distancia = std::hypot(dx, dy);

            if (!std::isfinite(distancia)) {
                throw std::invalid_argument(
                    "As coordenadas produziram uma distancia nao finita."
                );
            }

            // As distancias euclidianas sao simetricas.
            custos_[i][j] = distancia;
            custos_[j][i] = distancia;
        }
    }
}

int Grafo::numero_vertices() const
{
    return static_cast<int>(vertices_.size());
}

int Grafo::origem() const
{
    return 0;
}

int Grafo::destino() const
{
    return numero_vertices() - 1;
}

double Grafo::custo(int i, int j) const
{
    return custos_.at(i).at(j);
}

double Grafo::beneficio(int i) const
{
    return vertices_.at(i).beneficio;
}

double Grafo::limite_custo() const
{
    return limiteCusto_;
}