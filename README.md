## Algoritmos Implementados

### Heurísticas de Controle (Construtivas)
1. **Inserção Gulosa (`gulosa`):** Constrói a rota a partir de `(origem, destino)` avaliando todas as posições viáveis para vértices não visitados[cite: 3, 20]. Prioriza inserções com $\Delta \le 0$ (maior benefício $S_i$ e menor $\Delta$) e, na ausência destas, maximiza a razão $S_i / \Delta$[cite: 3, 20].
2. **MST com Reparação e Inserção Gulosa (`mst`):** Gera uma Árvore Geradora Mínima (Prim), extrai o caminho entre origem e destino via DFS, remove iterativamente os vértices internos de maior custo incidente até respeitar $T_{\max}$ e completa a rota por inserção gulosa[cite: 3, 20, 21].

### Metaheurísticas Propostas
1. **Iterated Local Search (`ils`):** Alterna busca local (vizinhanças sequenciais *2-opt*, *inserção* e *troca*) com perturbações que removem vértices internos com probabilidade $\rho = 0{,}2$, bloqueando-os temporariamente para reinserção direta[cite: 5, 6, 17]. Encerra após $L = 1000$ iterações sem melhoria ou ao atingir o limite de 5 segundos[cite: 5, 6, 17].
2. **Greedy Randomized Adaptive Search Procedure (`grasp`):** Parte da solução construtiva refinada por busca local e, iterativamente até o limite de 5 segundos, gera novas soluções via construção gulosa randomizada (Lista Restrita de Candidatos controlada por $\alpha = 0{,}25$) seguidas de busca local[cite: 5, 6, 19, 20].
3. **Simulated Annealing (`sa`):** Explora o espaço de busca sorteando uniformemente quatro operadores de vizinhança (*inserção gulosa*, *remoção aleatória*, *troca aleatória* e *2-opt guloso*)[cite: 6, 18]. Aceita movimentos de piora com probabilidade $e^{\Delta/\theta}$, utilizando temperatura inicial $\theta_0 = -\overline{p} / \ln(0{,}8)$ e fator de resfriamento $0{,}95$ a cada $n$ tentativas, parando após $L = 1000$ tentativas sem melhoria ou 5 segundos[cite: 5, 6, 18].

---

## Estrutura do Repositório

├── Grafo.hpp / Grafo.cpp                             # Modelagem do grafo, parsers (KU Leuven e OPLib) e validação de rotas
├── HeuristicasConstrutivas.hpp / .cpp                # Heurísticas iniciais: Inserção Gulosa e MST com reparação
├── Metaheuristicas.hpp / .cpp                        # Busca local compartilhada e metaheurísticas (ILS, GRASP, SA)
├── main.cpp                                          # Interface CLI, execução individual e runner experimental com exportação CSV
├── testes_corretude.cpp                              # Suíte automatizada de testes unitários, força bruta e casos de borda
└── instancias/                                       # Conjunto de 95 instâncias de benchmark
    ├── Chao/                                         # 40 instâncias (64 e 66 vértices - KU Leuven)
    │   ├── set_64_1/
    │   └── set_66_1/
    ├── Tsiligirides/                                 # 49 instâncias (21, 32 e 33 vértices - KU Leuven)
    │   ├── Tsiligirides 1/
    │   ├── Tsiligirides 2/
    │   └── Tsiligirides 3/
    └── OPLib/                                        # 6 instâncias adaptadas (kroA100, kroA200, rd400)
        ├── gen2/                                     # Pontuações pseudoaleatórias
        └── gen3/                                     # Pontuações maiores para vértices distantes

### 2. Executar uma Instância ou Experimento

A sintaxe geral de execução via linha de comando é:

./tp <arquivo|pasta> [gulosa|mst] [construtiva|ils|grasp|sa|experimento] [semente]
