// Testes de corretude do projeto (Open Orienteering Problem).
// Compilar junto com o projeto, SEM o main.cpp:
//   g++ -std=c++17 -O2 testes_corretude.cpp Grafo.cpp HeuristicasConstrutivas.cpp Metaheuristicas.cpp -o testes
// Executar: ./testes

#include "Grafo.hpp"
#include "HeuristicasConstrutivas.hpp"
#include "Metaheuristicas.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
int total = 0, falhas = 0;
std::string secaoAtual;

void secao(const std::string& nome) { secaoAtual = nome; std::cout << "\n== " << nome << " ==\n"; }

void check(bool cond, const std::string& msg)
{
    ++total;
    if (!cond) { ++falhas; std::cout << "  [FALHA] " << secaoAtual << ": " << msg << '\n'; }
}

bool lanca(const std::function<void()>& f)
{
    try { f(); } catch (const std::exception&) { return true; }
    return false;
}

// Instancia aleatoria euclidiana. orcFrac: fracao do "caminho total" usada como Tmax.
Grafo instancia_aleatoria(int n, unsigned semente, double orcFrac, bool beneficioZero = false)
{
    std::mt19937 rng(semente);
    std::uniform_real_distribution<double> coord(0, 100);
    std::uniform_int_distribution<int> ben(1, 20);
    std::vector<Vertice> v(n);
    for (auto& p : v) p = { coord(rng), coord(rng), beneficioZero ? 0.0 : double(ben(rng)) };
    // Tmax relativo a distancia origem-destino e ao tamanho da caixa.
    const double direto = std::hypot(v.front().x - v.back().x, v.front().y - v.back().y);
    return Grafo(v, direto + orcFrac * 100.0 * std::sqrt(double(n)));
}

// Verificacoes comuns a qualquer rota devolvida por heuristica.
void checar_rota(const Grafo& g, const std::vector<int>& rota, double pont, const std::string& quem)
{
    check(g.rota_viavel(rota), quem + ": rota inviavel (extremos, repeticao ou Tmax)");
    check(std::abs(pont - g.pontuacao_rota(rota)) < 1e-7, quem + ": pontuacao devolvida != recalculada");
    check(g.custo_rota(rota) <= g.limite_custo() + 1e-9, quem + ": custo acima de Tmax");
}

// Otimo exato por forca bruta (n pequeno): todas as permutacoes de todos os subconjuntos.
double otimo_bruto(const Grafo& g)
{
    const int n = g.numero_vertices();
    std::vector<int> internos;
    for (int i = 1; i < n - 1; ++i) internos.push_back(i);
    double melhor = 0.0;
    const int k = static_cast<int>(internos.size());
    for (int mask = 0; mask < (1 << k); ++mask) {
        std::vector<int> sel;
        for (int i = 0; i < k; ++i) if (mask >> i & 1) sel.push_back(internos[i]);
        std::sort(sel.begin(), sel.end());
        do {
            std::vector<int> rota{ 0 };
            rota.insert(rota.end(), sel.begin(), sel.end());
            rota.push_back(n - 1);
            if (g.rota_viavel(rota)) melhor = std::max(melhor, g.pontuacao_rota(rota));
        } while (std::next_permutation(sel.begin(), sel.end()));
    }
    return melhor;
}

const char* NOME_META[] = { "ILS", "GRASP", "SA" };
const char* NOME_CONS[] = { "gulosa", "mst" };
}

// ---------------------------------------------------------------------------
void teste_grafo()
{
    secao("Grafo: construtor e metodos basicos");
    Grafo g({ {0,0,5}, {3,0,10}, {3,4,7}, {6,4,9} }, 20.0);
    check(g.numero_vertices() == 4, "numero_vertices");
    check(g.origem() == 0 && g.destino() == 3, "origem/destino");
    check(g.beneficio(0) == 0.0 && g.beneficio(3) == 0.0, "origem e destino devem ter beneficio zero");
    check(g.beneficio(1) == 10.0 && g.beneficio(2) == 7.0, "beneficios internos preservados");
    check(std::abs(g.custo(0, 1) - 3.0) < 1e-12 && std::abs(g.custo(1, 2) - 4.0) < 1e-12, "distancias euclidianas");
    check(g.custo(0, 0) == 0.0, "diagonal zero");
    bool simetrica = true;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) simetrica &= g.custo(i, j) == g.custo(j, i);
    check(simetrica, "custos simetricos");

    // Desigualdade triangular em instancia aleatoria.
    Grafo r = instancia_aleatoria(30, 1, 0.5);
    bool tri = true;
    for (int i = 0; i < 30; ++i) for (int j = 0; j < 30; ++j) for (int k = 0; k < 30; ++k)
        tri &= r.custo(i, j) <= r.custo(i, k) + r.custo(k, j) + 1e-9;
    check(tri, "desigualdade triangular");

    check(lanca([] { Grafo({ {0,0,0} }, 10); }), "menos de 2 vertices deve lancar");
    check(lanca([] { Grafo({ {0,0,0},{1,1,0} }, -1); }), "Tmax negativo deve lancar");
    check(lanca([] { Grafo({ {0,0,0},{1,1,0} }, NAN); }), "Tmax NaN deve lancar");
    check(lanca([] { Grafo({ {0,0,0},{1,1,-3},{2,2,0} }, 10); }), "beneficio negativo deve lancar");
    check(lanca([] { Grafo({ {NAN,0,0},{1,1,1} }, 10); }), "coordenada NaN deve lancar");

}

void teste_grafo2()
{
    secao("Grafo: rota_viavel / custo_rota / pontuacao_rota");
    // custo 0-1 = 3, 1-2 = 4, 2-3 = 3  => 10
    Grafo g({ {0,0,5}, {3,0,10}, {3,4,7}, {6,4,9} }, 10.0);
    check(std::abs(g.custo_rota({ 0,1,2,3 }) - 10.0) < 1e-12, "custo_rota = 10");
    check(g.pontuacao_rota({ 0,1,2,3 }) == 17.0, "pontuacao_rota = 17 (origem/destino valem 0)");
    check(g.rota_viavel({ 0,1,2,3 }), "rota no limite exato de Tmax e viavel");
    check(g.rota_viavel({ 0,3 }), "rota direta viavel");
    check(!g.rota_viavel({ 0,2,1,3 }), "rota acima de Tmax e inviavel");
    check(!g.rota_viavel({ 1,2,3 }), "nao comeca na origem");
    check(!g.rota_viavel({ 0,1,2 }), "nao termina no destino");
    check(!g.rota_viavel({ 0,1,1,3 }), "vertice repetido");
    check(!g.rota_viavel({ 0,9,3 }), "vertice fora do intervalo");
    check(!g.rota_viavel({ 0,-1,3 }), "vertice negativo");
    check(!g.rota_viavel({ 0 }), "rota com 1 vertice");
    check(!g.rota_viavel({}), "rota vazia");
}

void teste_parser()
{
    secao("Parser: formato KU Leuven (conforme o codigo espera: 'Tmax 1' + linhas 'x y s')");
    {
        std::istringstream in("10 1\n0 0 0\n9 0 0\n3 0 4\n6 0 8\n");
        Grafo g = Grafo::ler_instancia(in);
        check(g.numero_vertices() == 4, "KU: 4 vertices");
        check(g.limite_custo() == 10.0, "KU: Tmax");
        check(g.destino() == 3 && g.custo(g.origem(), g.destino()) == 9.0, "KU: destino (2o ponto) movido para o fim");
        check(g.beneficio(1) == 4.0 && g.beneficio(2) == 8.0, "KU: beneficios dos internos na ordem original");
    }
    {
        std::istringstream in("10 2\n0 0 0\n9 0 0\n3 0 4\n");
        check(lanca([&] { Grafo::ler_instancia(in); }), "KU: mais de 1 caminho deve lancar");
    }
    {
        std::istringstream in("10 1\n0 0 0\n");
        check(lanca([&] { Grafo::ler_instancia(in); }), "KU: menos de 2 pontos deve lancar");
    }

    secao("Parser: formato OPLib");
    {
        std::istringstream in(
            "NAME : teste\nTYPE : OP\nDIMENSION : 4\nCOST_LIMIT : 25\nEDGE_WEIGHT_TYPE : EUC_2D\n"
            "NODE_COORD_SECTION\n1 0 0\n2 3 0\n3 6 0\n4 9 0\nSCORE_SECTION\n1 0\n2 4\n3 8\n4 5\nEOF\n");
        Grafo g = Grafo::ler_instancia(in);
        check(g.numero_vertices() == 4, "OPLib: 4 vertices");
        check(g.limite_custo() == 25.0, "OPLib: COST_LIMIT");
        check(g.beneficio(1) == 4.0 && g.beneficio(2) == 8.0, "OPLib: scores internos");
        check(g.beneficio(0) == 0.0 && g.beneficio(3) == 0.0, "OPLib: primeiro e ultimo zerados");
        check(std::abs(g.custo(0, 3) - 9.0) < 1e-12, "OPLib: coordenadas");
    }
    {
        std::istringstream in("NAME : x\nDIMENSION : 3\nNODE_COORD_SECTION\n1 0 0\n2 1 1\n3 2 2\nSCORE_SECTION\n1 0\n2 1\n3 0\n");
        check(lanca([&] { Grafo::ler_instancia(in); }), "OPLib: sem COST_LIMIT deve lancar");
    }
}

void teste_construtivas()
{
    secao("Construtivas: viabilidade e pontuacao (200 instancias aleatorias x 2 heuristicas)");
    for (int t = 0; t < 2; ++t) {
        const auto tipo = t == 0 ? TipoConstrucao::InsercaoGulosa : TipoConstrucao::MST;
        for (unsigned s = 0; s < 200; ++s) {
            const int n = 4 + s % 40;
            const double frac = 0.05 + 0.1 * (s % 8);
            Grafo g = instancia_aleatoria(n, s * 7 + t, frac);
            std::vector<int> rota;
            const double p = construir_solucao_inicial(g, rota, tipo);
            if (rota.empty()) { check(false, std::string(NOME_CONS[t]) + ": rota vazia em instancia viavel"); continue; }
            checar_rota(g, rota, p, NOME_CONS[t]);
        }
    }

    secao("Construtivas: casos de borda");
    for (int t = 0; t < 2; ++t) {
        const auto tipo = t == 0 ? TipoConstrucao::InsercaoGulosa : TipoConstrucao::MST;
        const std::string nome = NOME_CONS[t];
        { // origem-destino nao cabe
            Grafo g({ {0,0,0},{10,0,5},{20,0,0} }, 5.0);
            std::vector<int> rota{ 99 };
            double p = construir_solucao_inicial(g, rota, tipo);
            check(rota.empty() && p == 0.0, nome + ": Tmax < d(0,n+1) deve devolver rota vazia e 0");
        }
        { // so origem e destino
            Grafo g({ {0,0,0},{3,4,0} }, 5.0);
            std::vector<int> rota;
            double p = construir_solucao_inicial(g, rota, tipo);
            check(rota == std::vector<int>({ 0,1 }) && p == 0.0, nome + ": n=2 devolve (0,1)");
        }
        { // Tmax exatamente igual a d(0,n+1): nao cabe nenhum interno fora da reta
            Grafo g({ {0,0,0},{5,50,9},{10,0,0} }, 10.0);
            std::vector<int> rota;
            construir_solucao_inicial(g, rota, tipo);
            check(g.rota_viavel(rota) && rota.size() == 2, nome + ": orcamento justo nao admite desvio");
        }
        { // ponto colinear no caminho: custo adicional 0, deve ser incluido
            Grafo g({ {0,0,0},{5,0,9},{10,0,0} }, 10.0);
            std::vector<int> rota;
            double p = construir_solucao_inicial(g, rota, tipo);
            check(p == 9.0 && rota == std::vector<int>({ 0,1,2 }), nome + ": vertice colinear (delta=0) deve entrar");
        }
        { // orcamento folgado: deve visitar todos os pontos de beneficio positivo
            Grafo g = instancia_aleatoria(12, 3, 100.0);
            std::vector<int> rota;
            double p = construir_solucao_inicial(g, rota, tipo);
            double soma = 0; for (int i = 1; i < 11; ++i) soma += g.beneficio(i);
            check(std::abs(p - soma) < 1e-9, nome + ": Tmax enorme deve coletar tudo");
        }
        { // beneficios todos zero
            Grafo g = instancia_aleatoria(10, 4, 0.3, true);
            std::vector<int> rota;
            double p = construir_solucao_inicial(g, rota, tipo);
            check(g.rota_viavel(rota) && p == 0.0, nome + ": beneficios zero -> pontuacao 0 e rota viavel");
        }
    }
    {
        Grafo g = instancia_aleatoria(8, 1, 0.3);
        std::vector<int> r;
        check(lanca([&] { construir_solucao_inicial(g, r, static_cast<TipoConstrucao>(7)); }), "tipo de construcao invalido deve lancar");
    }
    {
        // Determinismo das construtivas.
        Grafo g = instancia_aleatoria(40, 9, 0.4);
        for (int t = 0; t < 2; ++t) {
            const auto tipo = t == 0 ? TipoConstrucao::InsercaoGulosa : TipoConstrucao::MST;
            std::vector<int> a, b;
            construir_solucao_inicial(g, a, tipo); construir_solucao_inicial(g, b, tipo);
            check(a == b, std::string(NOME_CONS[t]) + ": construtiva deve ser deterministica");
        }
    }
}

void teste_metaheuristicas()
{
    secao("Metaheuristicas: viabilidade, nao piorar e limite do otimo (forca bruta, n<=9)");
    // tempoCorrido = 4.9 deixa so 0.1 s para cada metaheuristica -> suite rapida.
    for (unsigned s = 0; s < 12; ++s) {
        Grafo g = instancia_aleatoria(5 + s % 5, 100 + s, 0.2 + 0.15 * (s % 4));
        const double otimo = otimo_bruto(g);
        for (int t = 0; t < 2; ++t) {
            const auto tipo = t == 0 ? TipoConstrucao::InsercaoGulosa : TipoConstrucao::MST;
            for (int meta = 0; meta < 3; ++meta) {
                std::vector<int> rota;
                const double ini = construir_solucao_inicial(g, rota, tipo);
                std::vector<RegistroEvolucao> hist{ {0.0, ini, g.custo_rota(rota)} };
                const double fim = busca(g, rota, meta, 4.9, 42 + s, tipo, 0.2, 1000, 0.25, &hist);
                const std::string quem = std::string(NOME_META[meta]) + "+" + NOME_CONS[t];
                checar_rota(g, rota, fim, quem);
                check(fim >= ini - 1e-9, quem + ": resultado pior que a solucao inicial");
                check(fim <= otimo + 1e-9, quem + ": pontuacao acima do otimo exato (bug de viabilidade)");
                bool mono = true;
                for (size_t i = 1; i < hist.size(); ++i)
                    mono &= hist[i].pontuacao >= hist[i - 1].pontuacao - 1e-9 && hist[i].tempo >= hist[i - 1].tempo - 1e-9;
                check(mono, quem + ": historico de evolucao nao monotono");
            }
        }
    }

    secao("Metaheuristicas: instancias medias (n=30..100)");
    for (unsigned s = 0; s < 6; ++s) {
        Grafo g = instancia_aleatoria(30 + 14 * s, 500 + s, 0.25);
        for (int meta = 0; meta < 3; ++meta) {
            std::vector<int> rota;
            const double ini = construir_solucao_inicial(g, rota, TipoConstrucao::InsercaoGulosa);
            const double fim = busca(g, rota, meta, 4.7, 7, TipoConstrucao::InsercaoGulosa, 0.2, 1000, 0.25, nullptr);
            checar_rota(g, rota, fim, NOME_META[meta]);
            check(fim >= ini - 1e-9, std::string(NOME_META[meta]) + ": piorou");
        }
    }

    secao("Metaheuristicas: respeito ao limite de 5 s (n=400, 0.2 s restantes)");
    {
        Grafo g = instancia_aleatoria(400, 77, 0.3);
        for (int meta = 0; meta < 3; ++meta) {
            std::vector<int> rota;
            const double ini = construir_solucao_inicial(g, rota, TipoConstrucao::InsercaoGulosa);
            const auto t0 = std::chrono::steady_clock::now();
            const double fim = busca(g, rota, meta, 4.8, 1, TipoConstrucao::InsercaoGulosa, 0.2, 1000, 0.25, nullptr);
            const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            checar_rota(g, rota, fim, NOME_META[meta]);
            check(fim >= ini - 1e-9, std::string(NOME_META[meta]) + ": piorou (n=400)");
            check(dt < 0.2 + 0.25, std::string(NOME_META[meta]) + ": estourou o limite de tempo (levou " + std::to_string(dt) + " s para 0.2 s)");
        }
    }

    secao("Metaheuristicas: tempo ja esgotado (tempoCorrido >= 5) deve devolver a inicial");
    {
        Grafo g = instancia_aleatoria(30, 5, 0.3);
        for (int meta = 0; meta < 3; ++meta) {
            std::vector<int> rota;
            const double ini = construir_solucao_inicial(g, rota, TipoConstrucao::InsercaoGulosa);
            const auto copia = rota;
            const double fim = busca(g, rota, meta, 5.0, 1);
            checar_rota(g, rota, fim, NOME_META[meta]);
            check(fim >= ini - 1e-9, std::string(NOME_META[meta]) + ": piorou com tempo esgotado");
            (void)copia;
        }
    }

    secao("Metaheuristicas: validacao de argumentos");
    {
        Grafo g = instancia_aleatoria(10, 5, 0.3);
        std::vector<int> r;
        construir_solucao_inicial(g, r, TipoConstrucao::InsercaoGulosa);
        check(lanca([&] { auto c = r; busca(g, c, 3, 0.0); }), "meta invalida (3) deve lancar");
        check(lanca([&] { auto c = r; busca(g, c, -1, 0.0); }), "meta invalida (-1) deve lancar");
        check(lanca([&] { auto c = r; SA(g, c, -1.0); }), "SA: tempoCorrido negativo deve lancar");
        check(lanca([&] { auto c = r; SA(g, c, NAN); }), "SA: tempoCorrido NaN deve lancar");
        check(lanca([&] { auto c = r; SA(g, c, 0.0, 1, 0); }), "SA: L=0 deve lancar");
        check(lanca([&] { auto c = r; SA(g, c, 0.0, 1, -5); }), "SA: L<0 deve lancar");
        check(lanca([&] { std::vector<int> c{ 1, 2, 9 }; SA(g, c, 0.0); }), "SA: rota sem origem deve lancar");
        check(lanca([&] { std::vector<int> c{ 0, 1, 1, 9 }; SA(g, c, 0.0); }), "SA: rota com repeticao deve lancar");
        check(lanca([&] { std::vector<int> c{ 0, 5, 9 }; Grafo h({ {0,0,0},{500,500,5},{1,1,0} }, 1.0); (void)h; SA(h, c, 0.0); }), "SA: rota inviavel (orcamento) deve lancar");
        check(lanca([&] { auto c = r; GRASP(g, c, 0.0, 1.5); }), "GRASP: alpha > 1 deve lancar");
        check(lanca([&] { auto c = r; GRASP(g, c, 0.0, -0.1); }), "GRASP: alpha < 0 deve lancar");
    }

    secao("SA: determinismo com mesma semente e variacao entre sementes");
    {
        Grafo g = instancia_aleatoria(25, 31, 0.3);
        std::vector<int> base;
        construir_solucao_inicial(g, base, TipoConstrucao::InsercaoGulosa);
        auto rodar = [&](unsigned sem) { auto r = base; double p = SA(g, r, 0.0, sem, 300); return std::make_pair(p, r); };
        // L pequeno para terminar por estagnacao antes dos 5 s (nao depende de relogio).
        auto a = rodar(5), b = rodar(5);
        check(a.first == b.first && a.second == b.second, "mesma semente deve dar o mesmo resultado (parada por L)");
        bool variou = false;
        for (unsigned sem = 1; sem <= 8; ++sem) if (rodar(sem).second != a.second) variou = true;
        check(variou, "sementes diferentes deveriam produzir rotas diferentes em alguma execucao");
    }

    secao("SA: instancias degeneradas");
    {
        Grafo g({ {0,0,0},{3,4,0} }, 5.0); // so origem e destino
        std::vector<int> r{ 0, 1 };
        double p = SA(g, r, 0.0);
        check(r == std::vector<int>({ 0,1 }) && p == 0.0, "n=2");
        Grafo z = instancia_aleatoria(10, 3, 0.3, true); // beneficios zero
        std::vector<int> rz; construir_solucao_inicial(z, rz, TipoConstrucao::InsercaoGulosa);
        p = SA(z, rz, 0.0);
        check(z.rota_viavel(rz) && p == 0.0, "beneficios zero");
        Grafo c({ {0,0,0},{5,0,9},{10,0,0} }, 10.0);  // orcamento justo
        std::vector<int> rc; construir_solucao_inicial(c, rc, TipoConstrucao::InsercaoGulosa);
        p = SA(c, rc, 0.0);
        check(c.rota_viavel(rc) && p == 9.0, "orcamento justo, vertice colinear");
    }
}

int main()
{
    teste_grafo();
    teste_grafo2();
    teste_parser();
    teste_construtivas();
    teste_metaheuristicas();
    std::cout << "\n" << (total - falhas) << "/" << total << " verificacoes passaram";
    if (falhas) std::cout << "  (" << falhas << " FALHAS)";
    std::cout << '\n';
    return falhas ? 1 : 0;
}
