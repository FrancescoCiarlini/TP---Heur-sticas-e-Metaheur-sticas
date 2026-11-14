#include "Grafo.hpp"
#include "HeuristicasConstrutivas.hpp"
#include "Metaheuristicas.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>

namespace {
// Parametros utilizados em todas as execucoes e registrados nos CSVs.
constexpr double RHO = 0.2;
constexpr int L = 1000;
constexpr double ALPHA = 0.25;
constexpr int REPETICOES = 10;
const char* NOMES[] = { "ils", "grasp", "sa" };

std::string csv(const std::string& texto)
{
    std::string resultado = "\"";
    for (char c : texto) {
        if (c == '"') resultado += '"';
        resultado += c;
    }
    return resultado + '"';
}

double ganho_relativo(double inicial, double atual)
{
    return inicial > 0.0 ? 100.0 * (atual - inicial) / inicial
        : std::numeric_limits<double>::quiet_NaN();
}

void numero_csv(std::ostream& saida, double valor)
{
    if (std::isfinite(valor)) saida << valor;
    else saida << "NA";
}

double t_critico(int graus)
{
    static const double tabela[] = { 0, 12.7062047364, 4.3026527297,
        3.1824463053, 2.7764451052, 2.5705818356, 2.4469118511,
        2.3646242510, 2.3060041352, 2.2621571628, 2.2281388520,
        2.2009851601, 2.1788128297, 2.1603686565, 2.1447866879,
        2.1314495456, 2.1199052992, 2.1098155778, 2.1009220402,
        2.0930240544, 2.0859634473, 2.0796138447, 2.0738730679,
        2.0686576104, 2.0638985616, 2.0595385528, 2.0555294386,
        2.0518305165, 2.0484071418, 2.0452296421, 2.0422724563 };
    if (graus <= 30) return tabela[graus];
    const double z = 1.9599639845;
    const double v = graus;
    // Expansao do quantil t para os demais graus de liberdade.
    return z + (std::pow(z, 3) + z) / (4 * v)
        + (5 * std::pow(z, 5) + 16 * std::pow(z, 3) + 3 * z) / (96 * v * v)
        + (3 * std::pow(z, 7) + 19 * std::pow(z, 5)
            + 17 * std::pow(z, 3) - 15 * z) / (384 * v * v * v);
}

double escrever_resumo(std::ostream& saida, const std::string& escopo,
    const std::string& instancia, const std::string& construcao,
    int meta, const std::vector<double>& ganhos)
{
    const double indefinido = std::numeric_limits<double>::quiet_NaN();
    double media = indefinido, desvio = indefinido, margem = indefinido;
    if (!ganhos.empty()) {
        media = std::accumulate(ganhos.begin(), ganhos.end(), 0.0) / ganhos.size();
        if (ganhos.size() >= 2) {
            double soma = 0.0;
            for (double ganho : ganhos) soma += (ganho - media) * (ganho - media);
            desvio = std::sqrt(soma / (ganhos.size() - 1));
            margem = t_critico(static_cast<int>(ganhos.size()) - 1)
                * desvio / std::sqrt(static_cast<double>(ganhos.size()));
        }
    }
    saida << escopo << ';' << csv(instancia) << ';' << construcao
          << ';' << NOMES[meta] << ';' << ganhos.size() << ';';
    numero_csv(saida, media); saida << ';';
    numero_csv(saida, desvio); saida << ';';
    numero_csv(saida, media - margem); saida << ';';
    numero_csv(saida, media + margem); saida << ';';
    if (std::isfinite(margem)) saida << (media >= 5.0 && media - margem > 0.0 ? "sim" : "nao");
    else saida << "NA";
    saida << '\n';
    return media;
}
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 5) {
        std::cerr << "Uso: ./tp <arquivo|pasta> [gulosa|mst] "
                     "[construtiva|ils|grasp|sa|experimento] [semente]\n";
        return 1;
    }
    try {
        namespace fs = std::filesystem;
        const fs::path entrada(argv[1]);
        const std::string construcao = argc >= 3 ? argv[2] : "gulosa";
        const std::string metodo = argc >= 4 ? argv[3] : "construtiva";
        if (construcao != "gulosa" && construcao != "mst") {
            throw std::invalid_argument("Construcao invalida: use gulosa ou mst.");
        }
        const TipoConstrucao tipo = construcao == "gulosa"
            ? TipoConstrucao::InsercaoGulosa : TipoConstrucao::MST;
        const bool experimento = metodo == "experimento";
        int metaEscolhida = -1;
        for (int i = 0; i < 3; ++i) if (metodo == NOMES[i]) metaEscolhida = i;
        if (!experimento && metaEscolhida == -1 && metodo != "construtiva") {
            throw std::invalid_argument("Metodo invalido.");
        }
        unsigned int semente = 42;
        if (argc >= 5) {
            const std::string texto(argv[4]);
            std::size_t fim = 0;
            const auto valor = std::stoull(texto, &fim);
            if (fim != texto.size() || texto.empty() || texto.front() == '-' ||
                valor > std::numeric_limits<unsigned int>::max()) {
                throw std::invalid_argument("Semente invalida.");
            }
            semente = static_cast<unsigned int>(valor);
        }
        if (experimento && semente > std::numeric_limits<unsigned int>::max() - REPETICOES + 1) {
            throw std::invalid_argument("A semente nao permite dez valores distintos.");
        }

        std::vector<fs::path> arquivos;
        if (fs::is_directory(entrada)) {
            for (const auto& item : fs::directory_iterator(entrada)) {
                const auto extensao = item.path().extension();
                if (item.is_regular_file() && (extensao == ".txt" || extensao == ".oplib")) {
                    arquivos.push_back(item.path());
                }
            }
            std::sort(arquivos.begin(), arquivos.end());
        }
        else arquivos.push_back(entrada);
        if (arquivos.empty()) throw std::invalid_argument("Nenhuma instancia encontrada.");

        std::ofstream execucoes, evolucao, resumo;
        if (experimento) {
            const std::string prefixo = "resultados_"
                + fs::weakly_canonical(entrada).filename().string() + "_" + construcao;
            execucoes.open(prefixo + "_execucoes.csv");
            evolucao.open(prefixo + "_evolucao.csv");
            resumo.open(prefixo + "_resumo.csv");
            if (!execucoes || !evolucao || !resumo) throw std::runtime_error("Erro ao criar CSVs.");
            execucoes << std::setprecision(12)
                << "instancia;construcao;meta;semente;rho;L;alpha;inicial;final;custo;tempo_s;ganho_pct;rota\n";
            evolucao << std::setprecision(12)
                << "instancia;construcao;meta;semente;tempo_s;pontuacao;custo;ganho_pct\n";
            resumo << std::setprecision(12)
                << "escopo;instancia;construcao;meta;amostras;media_ganho_pct;desvio_padrao;ic95_inferior;ic95_superior;criterio_5pct\n";
            std::cout << "Arquivos de saida: " << prefixo << "_{execucoes,evolucao,resumo}.csv\n";
        }

        std::vector<std::vector<double>> mediasInstancias(3);
        for (const auto& caminho : arquivos) {
            std::ifstream arquivo(caminho);
            if (!arquivo) throw std::runtime_error("Erro ao abrir " + caminho.string());
            const Grafo g = Grafo::ler_instancia(arquivo);
            const int primeiro = experimento ? 0 : metaEscolhida;
            const int ultimo = experimento ? 2 : metaEscolhida;
            for (int meta = primeiro; meta <= ultimo; ++meta) {
                std::vector<double> ganhos;
                for (int r = 0; r < (experimento ? REPETICOES : 1); ++r) {
                    std::vector<int> rota;
                    const auto inicio = std::chrono::steady_clock::now();
                    const auto tempo = [&]() {
                        return std::chrono::duration<double>(std::chrono::steady_clock::now() - inicio).count();
                    };
                    const double inicial = construir_solucao_inicial(g, rota, tipo);
                    if (rota.empty()) throw std::runtime_error("Instancia inviavel: " + caminho.string());
                    if (!g.rota_viavel(rota)) throw std::runtime_error("Construcao gerou rota invalida.");
                    std::vector<RegistroEvolucao> historico{
                        { tempo(), inicial, g.custo_rota(rota) }
                    };
                    const unsigned int sementeAtual = semente + r;
                    const double final = meta == -1 ? inicial : busca(g, rota, meta, tempo(),
                        sementeAtual, tipo, RHO, L, ALPHA, experimento ? &historico : nullptr);
                    const double duracao = tempo();
                    if (!g.rota_viavel(rota) ||
                        std::abs(final - g.pontuacao_rota(rota)) > 1e-7 || final < inicial - 1e-9) {
                        throw std::runtime_error("Algoritmo devolveu pontuacao ou rota incorreta.");
                    }
                    const double custo = g.custo_rota(rota);
                    const double ganho = ganho_relativo(inicial, final);
                    if (!experimento) {
                        std::cout << std::setprecision(12) << caminho.filename().string()
                            << " | " << metodo << " | pontuacao=" << final
                            << " | custo=" << custo << " | tempo_s=" << duracao << "\nRota:";
                        for (int v : rota) std::cout << ' ' << v;
                        std::cout << '\n';
                        continue;
                    }
                    execucoes << csv(caminho.string()) << ';' << construcao << ';' << NOMES[meta]
                        << ';' << sementeAtual << ';' << RHO << ';' << L << ';' << ALPHA
                        << ';' << inicial << ';' << final << ';' << custo << ';' << duracao << ';';
                    numero_csv(execucoes, ganho); execucoes << ";\"";
                    for (std::size_t p = 0; p < rota.size(); ++p) execucoes << (p ? " " : "") << rota[p];
                    execucoes << "\"\n";
                    historico.push_back({ duracao, final, custo });
                    for (const auto& registro : historico) {
                        evolucao << csv(caminho.string()) << ';' << construcao << ';' << NOMES[meta]
                            << ';' << sementeAtual << ';' << registro.tempo << ';' << registro.pontuacao
                            << ';' << registro.custo << ';';
                        numero_csv(evolucao, ganho_relativo(inicial, registro.pontuacao));
                        evolucao << '\n';
                    }
                    if (std::isfinite(ganho)) ganhos.push_back(ganho);
                    std::cout << caminho.filename().string() << " | " << NOMES[meta]
                        << " | execucao " << r + 1 << '/' << REPETICOES << '\n';
                }
                if (experimento) {
                    const double media = escrever_resumo(resumo, "instancia", caminho.string(), construcao, meta, ganhos);
                    if (std::isfinite(media)) mediasInstancias[meta].push_back(media);
                }
            }
        }
        if (experimento) {
            for (int meta = 0; meta < 3; ++meta) {
                escrever_resumo(resumo, "conjunto", "TODAS", construcao, meta, mediasInstancias[meta]);
            }
            std::cout << "NA indica ganho indefinido (pontuacao inicial zero) ou amostras insuficientes.\n"
                << "No resumo do conjunto, cada amostra e a media de uma instancia.\n";
        }
        return 0;
    }
    catch (const std::exception& erro) {
        std::cerr << "Erro: " << erro.what() << '\n';
        return 1;
    }
}
