#include "Metaheuristicas.hpp"
#include <utility>

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

//----------------------------------------------------------------------------------------------------------------------
namespace {
constexpr double EPS = 1e-9;
constexpr double LIMITE_TEMPO = 5.0;

// rota + valores ja calculados
struct Solucao {
    std::vector<int> rota;
    double pontuacao = 0.0;
    double custo = 0.0;
};

// soma dos custos das arestas consecutivas da rota
double calcular_custo(const Grafo& g, const std::vector<int>& rota)
{
    double total = 0.0;

    for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
        total += g.custo(rota[p - 1], rota[p]);
    }

    return total;
}

// soma dos beneficios dos vertices da rota, com origem e destino = 0
double calcular_pontuacao(const Grafo& g, const std::vector<int>& rota)
{
    double total = 0.0;

    for (int v : rota) {
        total += g.beneficio(v);
    }

    return total;
}

// recalcula custo e pontuacao do 0
Solucao montar_solucao(const Grafo& g, std::vector<int> rota)
{
    Solucao s;
    s.pontuacao = calcular_pontuacao(g, rota);
    s.custo = calcular_custo(g, rota);
    s.rota = std::move(rota);
    return s;
}

// marcar quais vertices estao na rota
std::vector<bool> marcar_visitados(const Grafo& g, const std::vector<int>& rota)
{
    std::vector<bool> visitado(g.numero_vertices(), false);

    for (int v : rota) {
        visitado[v] = true;
    }

    return visitado;
}

// omeca na origem e termina no destino
void validar_rota_inicial(const Grafo& g, const std::vector<int>& rota)
{
    const int n = g.numero_vertices();

    if (rota.size() < 2 ||
        rota.front() != g.origem() ||
        rota.back() != g.destino())
    {
        throw std::invalid_argument(
            "A rota deve comecar na origem e terminar no destino."
        );
    }

    std::vector<bool> visto(n, false);

    for (int v : rota) {
        if (v < 0 || v >= n || visto[v]) {
            throw std::invalid_argument(
                "A rota possui vertice invalido ou repetido."
            );
        }

        visto[v] = true;
    }

    if (calcular_custo(g, rota) > g.limite_custo() + EPS) {
        throw std::invalid_argument(
            "O SA precisa receber uma rota viavel."
        );
    }
}

// a "melhor que" b: maior pontuacao ou, empatando, menor custo.
bool melhor_que(const Solucao& a, const Solucao& b)
{
    return a.pontuacao > b.pontuacao + EPS ||
        (std::abs(a.pontuacao - b.pontuacao) <= EPS &&
            a.custo < b.custo - EPS);
}

// theta0 = -media pontos turisticos / ln(0.8)
double temperatura_inicial(const Grafo& g)
{
    const int n = g.numero_vertices();

    if (n <= 2) {
        return 1.0;
    }

    double soma = 0.0;

    for (int v = 1; v < n - 1; ++v) {
        soma += g.beneficio(v);
    }

    const double media = soma / static_cast<double>(n - 2);

    // Todos os beneficios nulos: nenhum Delta < 0 ocorre
    if (media <= 0.0) {
        return 1.0;
    }

    return -media / std::log(0.8);
}

// Parametros do SA. Como o SA tem a assinatura fixa do .hpp,
// os valores ficam aqui ate a etapa 7 (definicao dos parametros).
constexpr int    L_SA = 1000;              // tentativas seguidas sem melhorar a melhor
constexpr double FATOR_RESFRIAMENTO = 0.95; // alpha do TP2
constexpr unsigned int SEMENTE_SA = 42;     // mesma semente padrao do ILS

bool aceitar_movimento(double delta,
    double custoAtual,
    double custoVizinha,
    double theta,
    std::mt19937& gerador)
{
    if (delta > EPS) {
        return true;
    }

    if (delta >= -EPS) {
        return custoVizinha < custoAtual - EPS;
    }

    std::uniform_real_distribution<double> uniforme(0.0, 1.0);

    return uniforme(gerador) < std::exp(delta / theta);
}

// ---------------------------------------------------------------
// OPERADORES DE VIZINHANCA DO SA
// Cada operador recebe a solucao atual (que nao e alterada) e
// preenche 'saida' com a candidata. Retorna false se nao houver
// movimento aplicavel ou se o movimento ficar inviavel.
// ---------------------------------------------------------------

enum class Operador {
    Insercao = 0,   // (i)   insercao gulosa
    Remocao = 1,    // (ii)  remocao aleatoria
    Troca = 2,      // (iii) troca aleatoria
    DoisOpt = 3     // (iv)  2-opt guloso
};

constexpr int NUM_OPERADORES = 4;

// (i) Insercao gulosa: entre as insercoes viaveis, escolhe a de maior
// ganho de pontuacao; empatando, a de menor custo adicional.
// Vertices de beneficio zero nao sao considerados, como na busca_local.
bool op_insercao_gulosa(const Grafo& g,
    const Solucao& atual,
    Solucao& saida)
{
    const int n = g.numero_vertices();
    const int m = static_cast<int>(atual.rota.size());
    const std::vector<bool> visitado = marcar_visitados(g, atual.rota);

    int melhorVertice = -1;
    int melhorPosicao = -1;
    double melhorGanho = -1.0;
    double melhorDelta = 0.0;

    for (int v = 0; v < n; ++v) {
        if (visitado[v]) {
            continue;
        }

        const double ganho = g.beneficio(v);

        if (ganho <= 0.0) {
            continue;
        }

        for (int p = 1; p < m; ++p) {
            const int a = atual.rota[p - 1];
            const int b = atual.rota[p];

            const double delta =
                g.custo(a, v) + g.custo(v, b) - g.custo(a, b);

            if (atual.custo + delta > g.limite_custo() + EPS) {
                continue;
            }

            if (melhorVertice == -1 ||
                ganho > melhorGanho + EPS ||
                (std::abs(ganho - melhorGanho) <= EPS &&
                    delta < melhorDelta))
            {
                melhorVertice = v;
                melhorPosicao = p;
                melhorGanho = ganho;
                melhorDelta = delta;
            }
        }
    }

    if (melhorVertice == -1) {
        return false;
    }

    std::vector<int> nova = atual.rota;
    nova.insert(nova.begin() + melhorPosicao, melhorVertice);

    saida = montar_solucao(g, std::move(nova));
    return true;
}

// (ii) Remocao aleatoria: remove um vertice interno escolhido
// uniformemente. Origem e destino nunca saem.
bool op_remocao_aleatoria(const Grafo& g,
    const Solucao& atual,
    std::mt19937& gerador,
    Solucao& saida)
{
    const int m = static_cast<int>(atual.rota.size());

    // Rota (origem, destino): nao ha vertice interno para remover.
    if (m <= 2) {
        return false;
    }

    std::uniform_int_distribution<int> sortearPosicao(1, m - 2);
    const int posicao = sortearPosicao(gerador);

    std::vector<int> nova = atual.rota;
    nova.erase(nova.begin() + posicao);

    Solucao candidata = montar_solucao(g, std::move(nova));

    // Pela desigualdade triangular o custo nao aumenta, mas a
    // checagem barata protege contra erro numerico.
    if (candidata.custo > g.limite_custo() + EPS) {
        return false;
    }

    saida = std::move(candidata);
    return true;
}

// (iii) Troca aleatoria: sorteia um vertice visitado (interno) e um nao
// visitado. Remove o visitado e insere o novo na posicao de menor custo
// adicional da rota ja sem o removido.
bool op_troca_aleatoria(const Grafo& g,
    const Solucao& atual,
    std::mt19937& gerador,
    Solucao& saida)
{
    const int n = g.numero_vertices();
    const int m = static_cast<int>(atual.rota.size());

    if (m <= 2) {
        return false;
    }

    const std::vector<bool> visitado = marcar_visitados(g, atual.rota);

    std::vector<int> naoVisitados;

    for (int v = 0; v < n; ++v) {
        if (!visitado[v]) {
            naoVisitados.push_back(v);
        }
    }

    if (naoVisitados.empty()) {
        return false;
    }

    std::uniform_int_distribution<int> sortearPosicao(1, m - 2);
    std::uniform_int_distribution<int> sortearNovo(
        0, static_cast<int>(naoVisitados.size()) - 1
    );

    const int posicaoRemovida = sortearPosicao(gerador);
    const int novo = naoVisitados[sortearNovo(gerador)];

    // Rota sem o vertice removido.
    std::vector<int> base = atual.rota;
    base.erase(base.begin() + posicaoRemovida);

    const double custoBase = calcular_custo(g, base);

    // Melhor posicao para o novo vertice na rota base.
    int melhorPosicao = -1;
    double melhorDelta = 0.0;

    for (int p = 1; p < static_cast<int>(base.size()); ++p) {
        const int a = base[p - 1];
        const int b = base[p];

        const double delta =
            g.custo(a, novo) + g.custo(novo, b) - g.custo(a, b);

        if (melhorPosicao == -1 || delta < melhorDelta) {
            melhorPosicao = p;
            melhorDelta = delta;
        }
    }

    // Movimento inviavel: descartado.
    if (custoBase + melhorDelta > g.limite_custo() + EPS) {
        return false;
    }

    base.insert(base.begin() + melhorPosicao, novo);

    saida = montar_solucao(g, std::move(base));
    return true;
}

// (iv) 2-opt guloso: aplica a inversao de segmento que mais reduz o
// custo. Retorna false se nenhuma inversao reduzir o custo.
bool op_dois_opt_guloso(const Grafo& g,
    const Solucao& atual,
    Solucao& saida)
{
    const int m = static_cast<int>(atual.rota.size());

    int melhorInicio = -1;
    int melhorFim = -1;
    double melhorDelta = 0.0;

    for (int i = 1; i < m - 2; ++i) {
        for (int j = i + 1; j < m - 1; ++j) {
            const int a = atual.rota[i - 1];
            const int b = atual.rota[i];
            const int c = atual.rota[j];
            const int d = atual.rota[j + 1];

            // Remove (a,b) e (c,d); adiciona (a,c) e (b,d).
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
        return false;
    }

    std::vector<int> nova = atual.rota;

    std::reverse(
        nova.begin() + melhorInicio,
        nova.begin() + melhorFim + 1
    );

    saida = montar_solucao(g, std::move(nova));
    return true;
}

// Despacha o operador sorteado.
bool gerar_vizinho(const Grafo& g,
    const Solucao& atual,
    Operador op,
    std::mt19937& gerador,
    Solucao& saida)
{
    switch (op) {
    case Operador::Insercao:
        return op_insercao_gulosa(g, atual, saida);

    case Operador::Remocao:
        return op_remocao_aleatoria(g, atual, gerador, saida);

    case Operador::Troca:
        return op_troca_aleatoria(g, atual, gerador, saida);

    case Operador::DoisOpt:
        return op_dois_opt_guloso(g, atual, saida);
    }

    return false;
}

} // namespace

double SA(const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido)
{
    const auto inicio = std::chrono::steady_clock::now();

    // Tempo total: construcao anterior + tempo gasto dentro do SA.
    auto tempo_atual = [&]() {
        const double tempoSA = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - inicio
            ).count();

        return tempoCorrido + tempoSA;
        };

    validar_rota_inicial(g, rota);

    Solucao atual = montar_solucao(g, rota);
    Solucao melhor = atual;
    Solucao vizinha;

    double theta = temperatura_inicial(g);

    // "n" do resfriamento: numero de pontos candidatos (sem origem e destino).
    const int tentativasPorResfriamento =
        std::max(1, g.numero_vertices() - 2);

    std::mt19937 gerador(SEMENTE_SA);
    std::uniform_int_distribution<int> sortearOperador(0, NUM_OPERADORES - 1);

    int semMelhoria = 0;   // "sem" do pseudocodigo
    int tentativas = 0;    // "j" do pseudocodigo

    while (semMelhoria < L_SA && tempo_atual() < LIMITE_TEMPO) {

        // 1. VIZINHO: sorteia o operador (25% cada) e gera a candidata.
        const Operador op =
            static_cast<Operador>(sortearOperador(gerador));

        const bool existe = gerar_vizinho(g, atual, op, gerador, vizinha);

        // 2. ACEITACAO (so para movimentos viaveis).
        if (existe) {
            const double delta = vizinha.pontuacao - atual.pontuacao;

            if (aceitar_movimento(delta, atual.custo, vizinha.custo, theta, gerador)) {
                atual = std::move(vizinha);
            }
        }

        // 3. MELHOR SOLUCAO E CONTADOR DE ESTAGNACAO.
        // Toda tentativa conta, inclusive as descartadas.
        if (melhor_que(atual, melhor)) {
            melhor = atual;
            semMelhoria = 0;
        }
        else {
            ++semMelhoria;
        }

        // 4. RESFRIAMENTO a cada n tentativas.
        ++tentativas;

        if (tentativas == tentativasPorResfriamento) {
            theta *= FATOR_RESFRIAMENTO;
            tentativas = 0;
        }
    }

    // Devolve a melhor solucao encontrada, nunca pior que a inicial.
    rota = melhor.rota;
    return melhor.pontuacao;
}

// -------------------------------------------------------------
// CONSTRUCAO RANDOMIZADA DO GRASP
// -------------------------------------------------------------
namespace {
    constexpr double EPS_GRASP = 1e-9;

    struct CandidatoGRASP {
        int vertice;
        int posicao;
        double delta;
        double ganho;
    };

    double custo_rota_grasp(const Grafo& g, const std::vector<int>& rota)
    {
        double total = 0.0;
        for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
            total += g.custo(rota[p - 1], rota[p]);
        }
        return total;
    }

    double pontuacao_rota_grasp(const Grafo& g, const std::vector<int>& rota)
    {
        double total = 0.0;
        for (int v : rota) {
            total += g.beneficio(v);
        }
        return total;
    }

    // Implementa o procedimento ConstrucaoRandomizada.
    double construcao_randomizada(
        const Grafo& g,
        std::vector<int>& rota,
        double alpha,
        std::mt19937& gerador)
    {
        const int n = g.numero_vertices();
        rota.clear();

        // A rota inicial contem somente origem e destino.
        if (g.custo(g.origem(), g.destino()) > g.limite_custo() + EPS_GRASP) {
            return 0.0;
        }

        rota = { g.origem(), g.destino() };
        std::vector<bool> visitado(n, false);
        visitado[g.origem()] = true;
        visitado[g.destino()] = true;

        double custoAtual = g.custo(g.origem(), g.destino());

        while (true) {
            std::vector<CandidatoGRASP> candidatos;
            double ganhoMaximo = -std::numeric_limits<double>::infinity();
            double ganhoMinimo = std::numeric_limits<double>::infinity();

            // Guarda, para cada vertice nao visitado, sua melhor posicao viavel.
            for (int v = 1; v < n - 1; ++v) {
                if (visitado[v]) continue;

                int melhorPosicao = -1;
                double menorDelta = std::numeric_limits<double>::infinity();

                for (int p = 1; p < static_cast<int>(rota.size()); ++p) {
                    const int a = rota[p - 1];
                    const int b = rota[p];
                    const double delta =
                        g.custo(a, v) + g.custo(v, b) - g.custo(a, b);

                    if (custoAtual + delta <= g.limite_custo() + EPS_GRASP &&
                        delta < menorDelta)
                    {
                        menorDelta = delta;
                        melhorPosicao = p;
                    }
                }

                if (melhorPosicao != -1) {
                    const double ganho = g.beneficio(v) / (menorDelta + EPS_GRASP);
                    candidatos.push_back({ v, melhorPosicao, menorDelta, ganho });
                    ganhoMaximo = std::max(ganhoMaximo, ganho);
                    ganhoMinimo = std::min(ganhoMinimo, ganho);
                }
            }

            if (candidatos.empty()) break;

            // LRC = { i | g_i >= g_max - alpha * (g_max - g_min) }.
            const double limiteLRC =
                ganhoMaximo - alpha * (ganhoMaximo - ganhoMinimo);

            std::vector<CandidatoGRASP> LRC;
            for (const CandidatoGRASP& candidato : candidatos) {
                if (candidato.ganho + EPS_GRASP >= limiteLRC) {
                    LRC.push_back(candidato);
                }
            }

            // Escolhe uniformemente um candidato da LRC.
            std::uniform_int_distribution<int> sortear(
                0, static_cast<int>(LRC.size()) - 1
            );
            const CandidatoGRASP escolhido = LRC[sortear(gerador)];

            rota.insert(rota.begin() + escolhido.posicao, escolhido.vertice);
            visitado[escolhido.vertice] = true;
            custoAtual += escolhido.delta;
        }

        return pontuacao_rota_grasp(g, rota);
    }
}

// -------------------------------------------------------------
// GREEDY RANDOMIZED ADAPTIVE SEARCH PROCEDURE (GRASP)
// -------------------------------------------------------------
double GRASP(
    const Grafo& g,
    std::vector<int>& rota,
    double tempoCorrido,
    double alpha,
    unsigned int semente,
    TipoConstrucao construcaoInicial)
{
    constexpr double EPS = 1e-9;
    constexpr double LIMITE_TEMPO = 5.0;

    if (!std::isfinite(alpha) || alpha < 0.0 || alpha > 1.0) {
        throw std::invalid_argument("alpha deve estar entre 0 e 1.");
    }
    if (!std::isfinite(tempoCorrido) || tempoCorrido < 0.0) {
        throw std::invalid_argument("O tempo corrido deve ser finito e nao negativo.");
    }

    const auto inicio = std::chrono::steady_clock::now();
    auto tempo_atual = [&]() {
        const double tempoGRASP = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - inicio
        ).count();
        return tempoCorrido + tempoGRASP;
    };

    std::mt19937 gerador(semente);

    // 1. Gera a solucao inicial com uma das heuristicas de controle.
    construir_solucao_inicial(g, rota, construcaoInicial);
    if (rota.empty()) return 0.0;

    // 2. Aplica a busca local compartilhada a solucao inicial.
    double pontuacaoMelhor = busca_local(g, rota, tempo_atual());
    std::vector<int> melhorRota = rota;
    double custoMelhor = custo_rota_grasp(g, melhorRota);

    // 3. Constroi solucoes do zero, aplica busca local e guarda a melhor.
    while (tempo_atual() < LIMITE_TEMPO) {
        std::vector<int> candidata;
        construcao_randomizada(g, candidata, alpha, gerador);
        if (candidata.empty()) break;

        const double pontuacaoCandidata = busca_local(
            g, candidata, tempo_atual()
        );
        const double custoCandidato = custo_rota_grasp(g, candidata);

        const bool melhorou =
            pontuacaoCandidata > pontuacaoMelhor + EPS ||
            (std::abs(pontuacaoCandidata - pontuacaoMelhor) <= EPS &&
                custoCandidato < custoMelhor - EPS);

        if (melhorou) {
            melhorRota = candidata;
            pontuacaoMelhor = pontuacaoCandidata;
            custoMelhor = custoCandidato;
        }
    }

    rota = melhorRota;
    return pontuacaoMelhor;
}
