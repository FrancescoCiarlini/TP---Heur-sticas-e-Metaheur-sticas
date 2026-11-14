#include "Grafo.hpp"
#include <sstream>
#include <string>
#include <algorithm>

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

Grafo Grafo::ler_instancia(std::istream& entrada)
{
    std::string linha;
    std::vector<Vertice> vertices;
    double limiteCusto = -1.0;
    bool isOPLib = false;

    // Lê a primeira linha não vazia para identificar a assinatura do formato
    while (std::getline(entrada, linha)) {
        if (linha.empty() || linha.find_first_not_of(" \r\n\t") == std::string::npos) continue;
        
        std::string linhaUpper = linha;
        std::transform(linhaUpper.begin(), linhaUpper.end(), linhaUpper.begin(), ::toupper);
        
        if (linhaUpper.find("NAME") != std::string::npos || 
            linhaUpper.find("TYPE") != std::string::npos || 
            linhaUpper.find("DIMENSION") != std::string::npos) 
        {
            isOPLib = true;
        }
        break;
    }

    if (!isOPLib) {
        // --- Formato KU Leuven (Tsiligirides, Chao) ---
        std::istringstream iss(linha);

        int numeroCaminhos = 0;

        if (!(iss >> limiteCusto >> numeroCaminhos)) {
            throw std::invalid_argument("Erro ao ler limite de custo (KU Leuven).");
        }

        if (numeroCaminhos != 1) {
            throw std::invalid_argument("Este projeto trabalha com uma unica rota.");
        }

        // Lê todos os pontos: x, y e benefício.
        while (std::getline(entrada, linha)) {
            if (linha.find_first_not_of(" \r\n\t") == std::string::npos) {
                continue;
            }
        
            std::istringstream ponto(linha);

            double x, y, beneficio;

            if (!(ponto >> x >> y >> beneficio)) {
                continue;
            }
            vertices.push_back({x, y, beneficio});
        }

        if (vertices.size() < 2) {
            throw std::invalid_argument("A instancia precisa ter origem e destino.");
        }

        // O primeiro ponto é a origem.
        // O segundo ponto é o destino.
        // Movemos o destino para o final do vetor.
        Vertice destino = vertices[1];
        vertices.erase(vertices.begin() + 1);
        vertices.push_back(destino);

    } else {
        // --- Formato OPLib (Derivado do TSPLib) ---
        int dimension = 0;
        
        auto extrair_valor = [](const std::string& l) -> std::string {
            auto pos = l.find(':');
            if (pos == std::string::npos) return "";
            std::string val = l.substr(pos + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \r\n\t") + 1);
            return val;
        };

        // Processa cabeçalho
        do {
            std::string lUpper = linha;
            std::transform(lUpper.begin(), lUpper.end(), lUpper.begin(), ::toupper);
            
            if (lUpper.find("COST_LIMIT") != std::string::npos || 
                lUpper.find("TMAX") != std::string::npos) 
            {
                limiteCusto = std::stod(extrair_valor(linha));
            }
            else if (lUpper.find("DIMENSION") != std::string::npos) {
                dimension = std::stoi(extrair_valor(linha));
            }
            else if (lUpper.find("NODE_COORD_SECTION") != std::string::npos) {
                break;
            }
        } while (std::getline(entrada, linha));

        if (dimension < 2) throw std::invalid_argument("Dimensao invalida na OPLib.");
        
        vertices.resize(dimension);
        
        // Coordenadas
        for (int i = 0; i < dimension; ++i) {
            int id; double x, y;
            if (!(entrada >> id >> x >> y)) {
                throw std::invalid_argument("Erro ao ler NODE_COORD_SECTION (OPLib).");
            }
            // Vetor indexado em 0, IDs começam em 1
            vertices[id - 1].x = x;
            vertices[id - 1].y = y;
            vertices[id - 1].beneficio = 0.0;
        }

        // Pontuações
        while (entrada >> linha) {
            std::string lUpper = linha;
            std::transform(lUpper.begin(), lUpper.end(), lUpper.begin(), ::toupper);
            if (lUpper.find("SCORE_SECTION") != std::string::npos) break;
        }
        
        for (int i = 0; i < dimension; ++i) {
            int id; double s;
            if (!(entrada >> id >> s)) break; 
            vertices[id - 1].beneficio = s;
        }
        
        if (limiteCusto < 0) {
            throw std::invalid_argument("Limite de custo nao encontrado na OPLib.");
        }
    }

    return Grafo(vertices, limiteCusto);
}

double Grafo::custo_rota(const std::vector<int>& rota) const
{
    double total = 0.0;
    for (std::size_t p = 1; p < rota.size(); ++p) {
        total += custo(rota[p - 1], rota[p]);
    }
    return total;
}

double Grafo::pontuacao_rota(const std::vector<int>& rota) const
{
    double total = 0.0;
    for (int v : rota) {
        total += beneficio(v);
    }
    return total;
}

bool Grafo::rota_viavel(const std::vector<int>& rota) const
{
    if (rota.size() < 2 || rota.front() != origem() || rota.back() != destino()) {
        return false;
    }
    std::vector<bool> visitado(numero_vertices(), false);
    for (int v : rota) {
        if (v < 0 || v >= numero_vertices() || visitado[v]) {
            return false;
        }
        visitado[v] = true;
    }
    const double total = custo_rota(rota);
    return std::isfinite(total) && total <= limite_custo() + 1e-9;
}
