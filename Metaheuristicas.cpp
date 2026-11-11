#include "Metaheuristicas.hpp"

double busca(const Grafo& g,
    std::vector<int>& rota,
    int meta,
    double tempoCorrido)
{
    switch (meta) {
    case 0:
        return ILS(g, rota, tempoCorrido);

    case 1:
        return GRASP(g, rota, tempoCorrido);

    case 2:
        return SA(g, rota, tempoCorrido);

    default:
        throw std::invalid_argument(
            "Meta invalida: use 0 para ILS, 1 para GRASP ou 2 para SA."
        );
    }
}

#include "busca_local.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

double busca_local(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    const std::vector<int>& bloqueados)
{
    constexpr double EPS = 1e-9;
    constexpr double LIMITE_TEMPO = 5.0;

    const auto inicio = std::chrono::steady_clock::now();

    // Funcao auxiliar definida dentro da busca local.
    // Soma o tempo desta chamada ao tempo anteriormente gasto.
    auto tempo_esgotado = [&]() {
        const double tempoLocal = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - inicio
        ).count();

        return tempoCorrido + tempoLocal >= LIMITE_TEMPO;
        };

    const int n = g.numero_vertices();

    if (rota.size() < 2 ||
        rota.front() != g.origem() ||
        rota.back() != g.destino())
    {
        throw std::invalid_argument(
            "A rota deve comecar na origem e terminar no destino."
        );
    }

    std::vector<bool> visitado(n, false);
    std::vector<bool> bloqueado(n, false);

    double pontuacaoAtual = 0.0;

    for (int v : rota) {
        if (v < 0 || v >= n || visitado[v]) {
            throw std::invalid_argument(
                "A rota possui vertice invalido ou repetido."
            );
        }

        visitado[v] = true;
        pontuacaoAtual += g.beneficio(v);
    }

    for (int v : bloqueados) {
        if (v < 0 || v >= n) {
            throw std::invalid_argument("Vertice bloqueado invalido.");
        }

        bloqueado[v] = true;
    }

    // Recalcula o custo depois de cada movimento para evitar
    // acumular erros numericos ao somar e subtrair variacoes.
    auto calcular_custo = [&]() {
        double total = 0.0;

        for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
            total += g.custo(rota[p - 1], rota[p]);
        }

        return total;
        };

    double custoAtual = calcular_custo();

    if (custoAtual > g.limite_custo() + EPS) {
        throw std::invalid_argument(
            "A busca local precisa receber uma rota viavel."
        );
    }

    // ---------------------------------------------------------
    // 1. 2-OPT
    // Escolhe a inversao de segmento que mais reduz o custo.
    // Origem e destino nunca sao movidos.
    // ---------------------------------------------------------

    while (!tempo_esgotado()) {
        const int m = static_cast<int>(rota.size());

        int melhorInicio = -1;
        int melhorFim = -1;
        double melhorDelta = 0.0;

        for (int i = 1; i < m - 2; ++i) {
            for (int j = i + 1; j < m - 1; ++j) {
                if (tempo_esgotado()) {
                    return pontuacaoAtual;
                }

                const int a = rota[i - 1];
                const int b = rota[i];
                const int c = rota[j];
                const int d = rota[j + 1];

                // Remove (a,b) e (c,d).
                // Adiciona (a,c) e (b,d).
                const double delta =
                    g.custo(a, c) + g.custo(b, d)
                    - g.custo(a, b) - g.custo(c, d);

                if (delta < melhorDelta) {
                    melhorDelta = delta;
                    melhorInicio = i;
                    melhorFim = j;
                }
            }
        }

        if (melhorInicio == -1 || melhorDelta >= -EPS) {
            break;
        }

        std::reverse(
            rota.begin() + melhorInicio,
            rota.begin() + melhorFim + 1
        );

        custoAtual = calcular_custo();
    }

    // ---------------------------------------------------------
    // 2. INSERCAO
    // Avalia todos os vertices e todas as posicoes.
    // Prioriza maior ganho de pontuacao.
    // Em empate, escolhe o menor custo adicional.
    // ---------------------------------------------------------

    while (!tempo_esgotado()) {
        const int m = static_cast<int>(rota.size());

        int melhorVertice = -1;
        int melhorPosicao = -1;
        double melhorGanho = -1.0;
        double melhorDelta = 0.0;

        for (int v = 0; v < n; ++v) {
            if (visitado[v] || bloqueado[v]) {
                continue;
            }

            const double ganho = g.beneficio(v);

            // Em uma metrica, inserir um vertice de beneficio
            // zero nao melhora a pontuacao nem reduz o custo.
            if (ganho <= 0.0) {
                continue;
            }

            for (int p = 1; p < m; ++p) {
                if (tempo_esgotado()) {
                    return pontuacaoAtual;
                }

                const int a = rota[p - 1];
                const int b = rota[p];

                const double delta =
                    g.custo(a, v) + g.custo(v, b)
                    - g.custo(a, b);

                if (custoAtual + delta > g.limite_custo() + EPS) {
                    continue;
                }

                if (melhorVertice == -1 ||
                    ganho > melhorGanho ||
                    (ganho == melhorGanho && delta < melhorDelta))
                {
                    melhorVertice = v;
                    melhorPosicao = p;
                    melhorGanho = ganho;
                    melhorDelta = delta;
                }
            }
        }

        if (melhorVertice == -1) {
            break;
        }

        rota.insert(
            rota.begin() + melhorPosicao,
            melhorVertice
        );

        visitado[melhorVertice] = true;
        pontuacaoAtual += melhorGanho;
        custoAtual = calcular_custo();
    }

    // ---------------------------------------------------------
    // 3. TROCA / REPLACE
    // Substitui um vertice interno por um nao visitado,
    // mantendo a mesma posicao na rota.
    //
    // Aceita maior pontuacao ou, com pontuacao igual,
    // menor custo. Bloqueados podem entrar por troca.
    // ---------------------------------------------------------

    while (!tempo_esgotado()) {
        const int m = static_cast<int>(rota.size());

        int melhorVertice = -1;
        int melhorPosicao = -1;
        double melhorGanho = -1.0;
        double melhorDelta = 0.0;

        for (int p = 1; p < m - 1; ++p) {
            const int a = rota[p - 1];
            const int atual = rota[p];
            const int b = rota[p + 1];

            for (int v = 0; v < n; ++v) {
                if (tempo_esgotado()) {
                    return pontuacaoAtual;
                }

                if (visitado[v]) {
                    continue;
                }

                const double ganho =
                    g.beneficio(v) - g.beneficio(atual);

                const double delta =
                    g.custo(a, v) + g.custo(v, b)
                    - g.custo(a, atual) - g.custo(atual, b);

                const bool melhora =
                    ganho > 0.0 ||
                    (ganho == 0.0 && delta < -EPS);

                if (!melhora ||
                    custoAtual + delta > g.limite_custo() + EPS)
                {
                    continue;
                }

                if (melhorVertice == -1 ||
                    ganho > melhorGanho ||
                    (ganho == melhorGanho && delta < melhorDelta))
                {
                    melhorVertice = v;
                    melhorPosicao = p;
                    melhorGanho = ganho;
                    melhorDelta = delta;
                }
            }
        }

        if (melhorVertice == -1) {
            break;
        }

        visitado[rota[melhorPosicao]] = false;

        rota[melhorPosicao] = melhorVertice;
        visitado[melhorVertice] = true;

        pontuacaoAtual += melhorGanho;
        custoAtual = calcular_custo();
    }

    return pontuacaoAtual;
}

double ILS(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double rho,
    int L,
    unsigned int semente)
{
    constexpr double EPS = 1e-9;
    constexpr double LIMITE_TEMPO = 5.0;

    const auto inicio = std::chrono::steady_clock::now();

    // Tempo total: construcao anterior + tempo gasto dentro do ILS.
    auto tempo_atual = [&]() {
        const double tempoILS = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - inicio
        ).count();

        return tempoCorrido + tempoILS;
        };

    auto calcular_custo = [&g](const std::vector<int>& r) {
        double total = 0.0;

        for (int p = 1; p < static_cast<int>(r.size()); ++p) {
            total += g.custo(r[p - 1], r[p]);
        }

        return total;
        };

    if (!std::isfinite(rho) || rho < 0.0 || rho > 1.0) {
        throw std::invalid_argument("rho deve estar entre 0 e 1.");
    }

    if (L <= 0) {
        throw std::invalid_argument("L deve ser positivo.");
    }

    std::mt19937 gerador(semente);

    // Cada sorteio retorna true com probabilidade rho.
    std::bernoulli_distribution remover(rho);

    // Melhora a solucao inicial, sem bloquear vertices.
    double pontuacaoAtual = busca_local(
        g,
        rota,
        tempo_atual()
    );

    double custoAtual = calcular_custo(rota);

    int semMelhoria = 0;

    while (semMelhoria < L &&
        rota.size() > 2 &&
        tempo_atual() < LIMITE_TEMPO)
    {
        // 1. PERTURBACAO
        // Constroi uma candidata removendo vertices internos.
        std::vector<int> candidata;
        std::vector<int> removidos;

        candidata.reserve(rota.size());
        candidata.push_back(rota.front());

        for (int p = 1; p < static_cast<int>(rota.size()) - 1; ++p) {
            if (remover(gerador)) {
                removidos.push_back(rota[p]);
            }
            else {
                candidata.push_back(rota[p]);
            }
        }

        candidata.push_back(rota.back());

        // Se nenhum vertice foi removido, remove um aleatoriamente.
        if (removidos.empty()) {
            std::uniform_int_distribution<int> sortearPosicao(
                1,
                static_cast<int>(rota.size()) - 2
            );

            const int posicao = sortearPosicao(gerador);

            removidos.push_back(rota[posicao]);

            // Como nenhuma remocao ocorreu antes, candidata e rota
            // ainda possuem os mesmos vertices nas mesmas posicoes.
            candidata.erase(candidata.begin() + posicao);
        }

        if (tempo_atual() >= LIMITE_TEMPO) {
            break;
        }

        // 2. BUSCA LOCAL
        // Os removidos ficam bloqueados para insercao,
        // mas podem retornar por troca.
        const double pontuacaoCandidata = busca_local(
            g,
            candidata,
            tempo_atual(),
            removidos
        );

        const double custoCandidata = calcular_custo(candidata);

        // 3. ACEITACAO
        const bool melhora =
            pontuacaoCandidata > pontuacaoAtual ||
            (pontuacaoCandidata == pontuacaoAtual &&
                custoCandidata < custoAtual - EPS);

        if (melhora) {
            rota.swap(candidata);

            pontuacaoAtual = pontuacaoCandidata;
            custoAtual = custoCandidata;

            semMelhoria = 0;
        }
        else {
            ++semMelhoria;
        }
    }

    return pontuacaoAtual;
}

