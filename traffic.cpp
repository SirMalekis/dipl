#include "traffic.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

namespace netsim {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

struct ShortestPathTree {
    std::vector<double> dist;         // стоимость пути для Дейкстры
    std::vector<double> actual_delay; // фактическая суммарная IPTD
    std::vector<double> log_surv;     // ln Π(1 − IPLR_e)
    std::vector<double> ipdv_sq;      // Σ IPDV_e²
    std::vector<int>    parent_node;
    std::vector<int>    parent_edge;

    void reset(int n) {
        dist.assign(n, kInf);
        actual_delay.assign(n, 0.0);
        log_surv.assign(n, -kInf);
        ipdv_sq.assign(n, 0.0);
        parent_node.assign(n, -1);
        parent_edge.assign(n, -1);
    }
};

void runDijkstra(const Graph& graph,
                 int src,
                 const std::vector<char>& active,
                 const QosParams& qos,
                 double hop_penalty,
                 ShortestPathTree& tree) {
    const int n = graph.num_nodes;
    tree.reset(n);
    if (!active[src]) return;

    tree.dist[src] = 0.0;
    tree.actual_delay[src] = 0.0;
    tree.log_surv[src] = 0.0;

    using QueueItem = std::pair<double, int>;  // (стоимость, вершина)
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
    queue.push({0.0, src});

    while (!queue.empty()) {
        const auto [du, u] = queue.top();
        queue.pop();
        if (du > tree.dist[u] + 1.0e-15) continue;

        for (const Edge& edge : graph.adj[u]) {
            const int v = edge.target;
            if (!active[v]) continue;

            const EdgeQos eq = evaluateEdgeQos(edge, qos);
            const double candidate =
                tree.dist[u] + eq.delay_s + hop_penalty;
            if (candidate + 1.0e-15 >= tree.dist[v]) continue;

            tree.dist[v] = candidate;
            tree.actual_delay[v] = tree.actual_delay[u] + eq.delay_s;
            tree.parent_node[v] = u;
            tree.parent_edge[v] = static_cast<int>(&edge - graph.adj[u].data());

            const double survival = std::max(0.0, 1.0 - eq.loss);
            const double log_add =
                (survival <= 0.0) ? -kInf : std::log(survival);
            tree.log_surv[v] = tree.log_surv[u] + log_add;
            tree.ipdv_sq[v] = tree.ipdv_sq[u] + eq.ipdv_s * eq.ipdv_s;

            queue.push({candidate, v});
        }
    }
}

}  // namespace

TrafficAssignment assignTraffic(Graph& graph,
                                const TrafficParams& traffic,
                                const QosParams& qos) {
    const int n = graph.num_nodes;
    graph.rebuildEdgeIndex();

    for (auto& list : graph.adj)
        for (auto& edge : list) edge.load = 0.0;

    std::vector<int> active_nodes;
    std::vector<char> active(n, 0);
    for (int i = 0; i < n; ++i) {
        if (graph.nodes[i].state == 1) {
            active[i] = 1;
            active_nodes.push_back(i);
        }
    }

    TrafficAssignment result;
    result.routing.n = n;
    result.routing.active_nodes = static_cast<int>(active_nodes.size());
    result.routing.pair.assign(n, std::vector<PairState>(n));

    if (active_nodes.size() < 2) {
        return result;
    }

    const double base_demand = std::max(0.0, traffic.demand_per_pair_bps);
    double demand = base_demand;
    if (traffic.preserve_total_demand && n >= 2 && active_nodes.size() >= 2) {
        const double base_pairs =
            static_cast<double>(n) * static_cast<double>(n - 1);
        const double active_pairs =
            static_cast<double>(active_nodes.size()) *
            static_cast<double>(active_nodes.size() - 1);
        if (active_pairs > 0.0) demand *= base_pairs / active_pairs;
    }
    const int iterations = std::max(1, traffic.assignment_iterations);
    result.iterations = iterations;
    result.total_demand_bps =
        static_cast<double>(active_nodes.size()) *
        static_cast<double>(active_nodes.size() - 1) * demand;

    std::vector<double> load(graph.num_edges, 0.0);
    std::vector<double> new_load(graph.num_edges, 0.0);
    ShortestPathTree tree;

    for (int iter = 0; iter < iterations; ++iter) {
        // Текущие нагрузки попадают в веса Дейкстры.
        for (auto& list : graph.adj)
            for (auto& edge : list) edge.load = load[edge.id];

        std::fill(new_load.begin(), new_load.end(), 0.0);

        for (int src : active_nodes) {
            runDijkstra(graph, src, active, qos, traffic.hop_penalty_s, tree);
            for (int dst : active_nodes) {
                if (dst == src || !std::isfinite(tree.dist[dst])) continue;
                int cur = dst;
                int guard = 0;
                while (cur != src && guard++ < n) {
                    const int p = tree.parent_node[cur];
                    const int idx = tree.parent_edge[cur];
                    if (p < 0 || idx < 0) break;
                    const Edge& edge = graph.adj[p][idx];
                    new_load[edge.id] += demand;
                    cur = p;
                }
            }
        }

        // Метод последовательных средних: α = 1/(k+1).
        const double alpha = 1.0 / static_cast<double>(iter + 1);
        for (auto& list : graph.adj) {
            for (auto& edge : list) {
                const int id = edge.id;
                load[id] = (1.0 - alpha) * load[id] + alpha * new_load[id];
                edge.load = load[id];
            }
        }
    }

    for (auto& list : graph.adj)
        for (auto& edge : list) edge.load = load[edge.id];

    // Финальный расчёт матрицы путей на сошедшихся нагрузках.
    for (int src : active_nodes) {
        runDijkstra(graph, src, active, qos, traffic.hop_penalty_s, tree);
        for (int dst : active_nodes) {
            PairState& state = result.routing.pair[src][dst];
            if (dst == src) {
                state.reachable = true;
                state.delay_s = 0.0;
                state.loss = 0.0;
                state.ipdv_s = 0.0;
                continue;
            }
            if (!std::isfinite(tree.dist[dst])) continue;

            const double delay = std::max(1.0e-12, tree.actual_delay[dst]);
            const double survival = std::exp(tree.log_surv[dst]);
            const double loss = std::min(1.0, std::max(0.0, 1.0 - survival));
            const double ipdv = std::sqrt(std::max(0.0, tree.ipdv_sq[dst]));

            const PathQos checks =
                evaluatePathQos(delay, loss, ipdv, true, qos);
            state.delay_s = delay;
            state.loss = loss;
            state.ipdv_s = ipdv;
            state.reachable = true;
            state.saturated = tree.actual_delay[dst] >= kSaturatedDelay * 0.5;
            state.iptd_ok = checks.iptd_ok;
            state.iplr_ok = checks.iplr_ok;
            state.ipdv_ok = checks.ipdv_ok;
            state.conforms = checks.conforms;

            ++result.routing.reachable_pairs;
            result.routing.sum_inv_delay += 1.0 / delay;
            if (state.conforms) {
                ++result.routing.conforming_pairs;
                result.routing.sum_inv_delay_qos += 1.0 / delay;
            }
        }
    }

    return result;
}

}  // namespace netsim
