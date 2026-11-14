#include "HeuristicasConstrutivas.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
    constexpr double EPS = 1e-9;

    double calcular_custo(const Grafo& g, const std::vector<int>& rota)
    {
        double total = 0.0;
        for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
            total += g.custo(rota[p - 1], rota[p]);
        }
        return total;
    }

    double calcular_pontuacao(const Grafo& g, const std::vector<int>& rota)
    {
        double total = 0.0;
        for (int v : rota) {
            total += g.beneficio(v);
        }
        return total;
    }

    // Completa uma rota usando a regra de insercao gulosa
    void completar_por_insercao_gulosa(
        const Grafo& g,
        std::vector<int>& rota,
        std::vector<bool>& visitado)
    {
        const int n = g.numero_vertices();
        double custoAtual = calcular_custo(g, rota);

        while (true) {
            int melhorVertice = -1;
            int melhorPosicao = -1;
            double melhorBeneficio = -1.0;
            double melhorDelta = 0.0;
            double melhorRazao = -1.0;
            bool existeReducao = false;

            // Primeiro verifica se existe alguma insercao com delta <= 0.
            for (int v = 1; v < n - 1; ++v) {
                if (visitado[v]) continue;

                for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
                    const int a = rota[p - 1];
                    const int b = rota[p];
                    const double delta =
                        g.custo(a, v) + g.custo(v, b) - g.custo(a, b);

                    if (custoAtual + delta > g.limite_custo() + EPS) {
                        continue;
                    }

                    if (delta <= EPS) {
                        if (!existeReducao ||
                            g.beneficio(v) > melhorBeneficio ||
                            (g.beneficio(v) == melhorBeneficio && delta < melhorDelta - EPS) ||
                            (g.beneficio(v) == melhorBeneficio && std::abs(delta - melhorDelta) <= EPS &&
                                (v < melhorVertice || (v == melhorVertice && p < melhorPosicao))))
                        {
                            existeReducao = true;
                            melhorVertice = v;
                            melhorPosicao = p;
                            melhorBeneficio = g.beneficio(v);
                            melhorDelta = delta;
                        }
                    }
                }
            }

            // Se nao ha insercao sem aumento de custo, maximiza beneficio/delta.
            if (!existeReducao) {
                for (int v = 1; v < n - 1; ++v) {
                    if (visitado[v]) continue;

                    for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
                        const int a = rota[p - 1];
                        const int b = rota[p];
                        const double delta =
                            g.custo(a, v) + g.custo(v, b) - g.custo(a, b);

                        if (custoAtual + delta > g.limite_custo() + EPS || delta <= EPS) {
                            continue;
                        }

                        const double razao = g.beneficio(v) / delta;
                        if (melhorVertice == -1 ||
                            razao > melhorRazao + EPS ||
                            (std::abs(razao - melhorRazao) <= EPS &&
                                (v < melhorVertice || (v == melhorVertice && p < melhorPosicao))))
                        {
                            melhorVertice = v;
                            melhorPosicao = p;
                            melhorRazao = razao;
                            melhorDelta = delta;
                        }
                    }
                }
            }

            if (melhorVertice == -1) {
                break;
            }

            rota.insert(rota.begin() + melhorPosicao, melhorVertice);
            visitado[melhorVertice] = true;
            custoAtual += melhorDelta;
        }
    }
}

double insercao_gulosa(const Grafo& g, std::vector<int>& rota)
{
    const int n = g.numero_vertices();
    rota.clear();

    // Se nem o deslocamento direto entre origem e destino cabe no orcamento, nao existe rota viavel.
    if (g.custo(g.origem(), g.destino()) > g.limite_custo() + EPS) {
        return 0.0;
    }

    rota = { g.origem(), g.destino() };
    std::vector<bool> visitado(n, false);
    visitado[g.origem()] = true;
    visitado[g.destino()] = true;

    completar_por_insercao_gulosa(g, rota, visitado);
    return calcular_pontuacao(g, rota);
}

double mst_com_reparacao(const Grafo& g, std::vector<int>& rota)
{
    const int n = g.numero_vertices();
    rota.clear();

    if (g.custo(g.origem(), g.destino()) > g.limite_custo() + EPS) {
        return 0.0;
    }

    // 1. Constroi uma arvore geradora minima usando Prim.
    std::vector<double> chave(n, std::numeric_limits<double>::infinity());
    std::vector<int> pai(n, -1);
    std::vector<bool> incluido(n, false);
    chave[0] = 0.0;

    for (int it = 0; it < n; ++it) {
        int u = -1;
        for (int v = 0; v < n; ++v) {
            if (!incluido[v] && (u == -1 || chave[v] < chave[u])) {
                u = v;
            }
        }

        if (u == -1) break;
        incluido[u] = true;

        for (int v = 0; v < n; ++v) {
            if (!incluido[v] && g.custo(u, v) < chave[v]) {
                chave[v] = g.custo(u, v);
                pai[v] = u;
            }
        }
    }

    std::vector<std::vector<int>> arvore(n);
    for (int v = 1; v < n; ++v) {
        if (pai[v] >= 0) {
            arvore[v].push_back(pai[v]);
            arvore[pai[v]].push_back(v);
        }
    }

    // 2. Encontra por DFS o caminho entre origem e destino na MST.
    std::vector<int> predecessor(n, -1);
    std::vector<int> pilha;
    pilha.push_back(g.origem());
    predecessor[g.origem()] = g.origem();

    while (!pilha.empty() && predecessor[g.destino()] == -1) {
        const int u = pilha.back();
        pilha.pop_back();

        for (int v : arvore[u]) {
            if (predecessor[v] == -1) {
                predecessor[v] = u;
                pilha.push_back(v);
            }
        }
    }

    if (predecessor[g.destino()] == -1) {
        return 0.0;
    }

    for (int v = g.destino();; v = predecessor[v]) {
        rota.push_back(v);
        if (v == g.origem()) break;
    }
    std::reverse(rota.begin(), rota.end());

    // 3. Repara a rota removendo o vertice interno com maior soma dos custos das arestas incidentes, ate respeitar o orcamento.
    double custoAtual = calcular_custo(g, rota);
    while (custoAtual > g.limite_custo() + EPS && rota.size() > 2) {
        int melhorPosicao = -1;
        double maiorSoma = -1.0;
        int menorIndice = std::numeric_limits<int>::max();

        for (int p = 1; p < static_cast<int>(rota.size()) - 1; ++p) {
            const int a = rota[p - 1];
            const int v = rota[p];
            const int b = rota[p + 1];
            const double soma = g.custo(a, v) + g.custo(v, b);

            if (soma > maiorSoma + EPS ||
                (std::abs(soma - maiorSoma) <= EPS && v < menorIndice))
            {
                maiorSoma = soma;
                menorIndice = v;
                melhorPosicao = p;
            }
        }

        if (melhorPosicao == -1) break;
        rota.erase(rota.begin() + melhorPosicao);
        custoAtual = calcular_custo(g, rota);
    }

    // 4. Completa a rota por insercao gulosa. Os vertices removidos pela reparacao tambem permanecem disponiveis como candidatos.
    std::vector<bool> visitado(n, false);
    for (int v : rota) visitado[v] = true;
    completar_por_insercao_gulosa(g, rota, visitado);

    return calcular_pontuacao(g, rota);
}

double construir_solucao_inicial(
    const Grafo& g,
    std::vector<int>& rota,
    TipoConstrucao tipo)
{
    switch (tipo) {
    case TipoConstrucao::InsercaoGulosa:
        return insercao_gulosa(g, rota);
    case TipoConstrucao::MST:
        return mst_com_reparacao(g, rota);
    default:
        throw std::invalid_argument("Tipo de construcao inicial invalido.");
    }
}
