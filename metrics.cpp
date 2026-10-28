#include "metrics.h"
#include <queue>
#include <vector>
#include <numeric>
#include <algorithm>
#include <limits>

// ---------------------------------------------------------------------------
// Динамический вес ребра (задержка IPTD)
// ---------------------------------------------------------------------------
static double calculateEdgeDelay(const Edge& e) {
    // Если нагрузка превышает 95% канала - задержка максимальна
    if (e.load >= 0.95 * e.capacity) {
        return e.base_latency + 1.0; // 1 секунда (выпадение из QoS)
    }
    // M/M/1 аппроксимация (средний размер пакета ~12000 бит)
    double packet_size = 12000.0;
    double mu = e.capacity / packet_size;
    double lambda = e.load / packet_size;

    return e.base_latency + (1.0 / (mu - lambda));
}

// ---------------------------------------------------------------------------
// Алгоритм Дейкстры для расчета IPTD между активными узлами
// ---------------------------------------------------------------------------
static std::vector<double> dijkstraDistances(const Graph& G, int src, const std::vector<int>& active_mask) {
    int N = G.num_nodes;
    std::vector<double> dist(N, std::numeric_limits<double>::infinity());
    if (!active_mask[src]) return dist;

    dist[src] = 0.0;
    using pdi = std::pair<double, int>;
    std::priority_queue<pdi, std::vector<pdi>, std::greater<pdi>> pq;
    pq.push({ 0.0, src });

    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;

        for (const Edge& e : G.adj[u]) {
            int v = e.target;
            if (active_mask[v]) {
                double weight = calculateEdgeDelay(e);
                if (dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    pq.push({ dist[v], v });
                }
            }
        }
    }
    return dist;
}

// ---------------------------------------------------------------------------
// Наибольшая связная компонента (LCC)
// ---------------------------------------------------------------------------
double computeLccRatio(const Graph& G, int N_total) {
    int N = G.num_nodes;
    std::vector<bool> visited(N, false);
    int lcc_size = 0;

    for (int s = 0; s < N; ++s) {
        if (G.nodes[s].state != 1 || visited[s]) continue;
        // BFS
        int comp_size = 0;
        std::queue<int> q;
        q.push(s);
        visited[s] = true;
        while (!q.empty()) {
            int u = q.front(); q.pop();
            ++comp_size;
            // Адаптировано под структуру Edge
            for (const Edge& e : G.adj[u]) {
                int v = e.target;
                if (G.nodes[v].state == 1 && !visited[v]) {
                    visited[v] = true;
                    q.push(v);
                }
            }
        }
        lcc_size = std::max(lcc_size, comp_size);
    }
    return N_total > 0 ? (double)lcc_size / N_total : 0.0;
}

// ---------------------------------------------------------------------------
// Global Efficiency — "доля сохранённого коммуникационного потенциала"
// Нормировка на N_total * (N_total - 1), чтобы E_norm ∈ [0, 1]
// ---------------------------------------------------------------------------
double computeGlobalEfficiency(const Graph& G) {
    int N = G.num_nodes;
    if (N < 2) return 0.0;

    // Строим маску активных узлов (для Дейкстры)
    std::vector<int> active_mask(N, 0);
    std::vector<int> active;
    for (int i = 0; i < N; ++i) {
        if (G.nodes[i].state == 1) {
            active_mask[i] = 1;
            active.push_back(i);
        }
    }

    int na = (int)active.size();
    if (na < 2) return 0.0;

    // Сумма обратных задержек (IPTD) между активными узлами
    double sum_inv = 0.0;
    for (int src : active) {
        auto dist = dijkstraDistances(G, src, active_mask);
        for (int v : active) {
            if (v != src && dist[v] < std::numeric_limits<double>::infinity())
                sum_inv += 1.0 / dist[v];
        }
    }

    return sum_inv / ((double)N * (N - 1));
}

double computeNormalizedEfficiency(const Graph& G, double E0) {
    if (E0 <= 0.0) return 0.0;
    return computeGlobalEfficiency(G) / E0;
}

double computeWeightedSurvivability(const Graph& G) {
    double total_w = 0.0, active_w = 0.0;
    for (const auto& n : G.nodes) {
        total_w += n.weight;
        if (n.state == 1) active_w += n.weight;
    }
    return total_w > 0.0 ? active_w / total_w : 0.0;
}

double computeInitialEfficiency(const Graph& G) {
    return computeGlobalEfficiency(G);
}

void recalculateTraffic(Graph& G, double traffic_per_pair) {
    int N = G.num_nodes;

    // 1. Сброс текущей нагрузки на всех ребрах
    for (int i = 0; i < N; ++i) {
        for (Edge& e : G.adj[i]) {
            e.load = 0.0;
        }
    }

    // 2. Собираем маску активных узлов
    std::vector<int> active;
    for (int i = 0; i < N; ++i) {
        if (G.nodes[i].state == 1) active.push_back(i);
    }
    if (active.size() < 2) return;

    // 3. All-to-All маршрутизация трафика
    for (int src : active) {
        std::vector<double> dist(N, std::numeric_limits<double>::infinity());
        std::vector<int> parent_node(N, -1);
        std::vector<int> parent_edge_idx(N, -1);

        dist[src] = 0.0;
        using pdi = std::pair<double, int>;
        std::priority_queue<pdi, std::vector<pdi>, std::greater<pdi>> pq;
        pq.push({ 0.0, src });

        // Алгоритм Дейкстры с сохранением пути
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist[u]) continue;

            for (size_t i = 0; i < G.adj[u].size(); ++i) {
                const Edge& e = G.adj[u][i];
                int v = e.target;
                if (G.nodes[v].state == 1) {
                    double weight = calculateEdgeDelay(e);
                    if (dist[u] + weight < dist[v]) {
                        dist[v] = dist[u] + weight;
                        parent_node[v] = u;
                        parent_edge_idx[v] = (int)i; // Запоминаем, по какому ребру пришли
                        pq.push({ dist[v], v });
                    }
                }
            }
        }

        // 4. Добавляем трафик на найденные кратчайшие пути до всех других активных узлов
        for (int dest : active) {
            if (src == dest || dist[dest] == std::numeric_limits<double>::infinity()) continue;

            // Обратный проход от dest к src
            int curr = dest;
            while (curr != src && parent_node[curr] != -1) {
                int p = parent_node[curr];
                int idx = parent_edge_idx[curr];

                // Добавляем нагрузку на прямое ребро (p -> curr)
                G.adj[p][idx].load += traffic_per_pair;

                curr = p;
            }
        }
    }
}