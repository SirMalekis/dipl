#include "metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <vector>

namespace netsim {

namespace {

struct LccInfo {
    std::vector<char> in_lcc;
    int size = 0;
    double weight = 0.0;
};

LccInfo largestComponent(const Graph& graph) {
    const int n = graph.num_nodes;
    LccInfo info;
    info.in_lcc.assign(n, 0);

    std::vector<char> visited(n, 0);
    std::vector<int> best_nodes;

    for (int start = 0; start < n; ++start) {
        if (graph.nodes[start].state != 1 || visited[start]) continue;
        std::vector<int> component;
        std::queue<int> queue;
        queue.push(start);
        visited[start] = 1;
        while (!queue.empty()) {
            const int u = queue.front();
            queue.pop();
            component.push_back(u);
            for (const Edge& edge : graph.adj[u]) {
                const int v = edge.target;
                if (graph.nodes[v].state == 1 && !visited[v]) {
                    visited[v] = 1;
                    queue.push(v);
                }
            }
        }
        if (component.size() > best_nodes.size())
            best_nodes = std::move(component);
    }

    info.size = static_cast<int>(best_nodes.size());
    for (int node : best_nodes) {
        info.in_lcc[node] = 1;
        info.weight += graph.nodes[node].weight;
    }
    return info;
}

}  // namespace

void MetricsContext::calibrate(const RoutingMatrix& routing) {
    const double denom =
        static_cast<double>(routing.n) * static_cast<double>(routing.n - 1);
    e0_phys = denom > 0.0 ? routing.sum_inv_delay / denom : 0.0;
    e0_qos = denom > 0.0 ? routing.sum_inv_delay_qos / denom : 0.0;
    baseline_reachable_pairs = routing.reachable_pairs;
    baseline_conforming_pairs = routing.conforming_pairs;
    baseline_qos_ok = e0_qos > 0.0;
}

double computeLccRatio(const Graph& graph) {
    if (graph.num_nodes == 0) return 0.0;
    return static_cast<double>(largestComponent(graph).size) /
           static_cast<double>(graph.num_nodes);
}

double computeWeightedSurvivability(const Graph& graph) {
    double total_weight = 0.0;
    for (const Node& node : graph.nodes) total_weight += node.weight;
    if (total_weight <= 0.0) return 0.0;
    return largestComponent(graph).weight / total_weight;
}

double computeActiveWeightRatio(const Graph& graph) {
    double total_weight = 0.0;
    double active_weight = 0.0;
    for (const Node& node : graph.nodes) {
        total_weight += node.weight;
        if (node.state == 1) active_weight += node.weight;
    }
    return total_weight > 0.0 ? active_weight / total_weight : 0.0;
}

NetworkMetrics computeMetrics(const Graph& graph,
                              const RoutingMatrix& routing,
                              const MetricsContext& context,
                              double time) {
    NetworkMetrics out;
    out.time = time;

    const int n = graph.num_nodes;
    const double denom = static_cast<double>(n) * static_cast<double>(n - 1);

    const LccInfo lcc = largestComponent(graph);
    out.lcc = n > 0 ? static_cast<double>(lcc.size) / n : 0.0;
    out.phi_w = computeWeightedSurvivability(graph);
    out.phi_active = computeActiveWeightRatio(graph);
    out.active_nodes = graph.activeNodes();

    out.e_phys = denom > 0.0 ? routing.sum_inv_delay / denom : 0.0;
    out.e_qos = denom > 0.0 ? routing.sum_inv_delay_qos / denom : 0.0;
    // Нормированная эффективность — индекс сохранённого потенциала:
    // значения выше 1 возникают, если базовая сеть была перегружена,
    // и ограничиваются единицей (сеть не может быть «лучше исправной»).
    const double phys_ratio =
        context.e0_phys > 0.0 ? out.e_phys / context.e0_phys : 0.0;
    const double qos_ratio =
        context.e0_qos > 0.0 ? out.e_qos / context.e0_qos : 0.0;
    out.e_phys_norm = std::max(0.0, std::min(1.0, phys_ratio));
    out.e_qos_norm = std::max(0.0, std::min(1.0, qos_ratio));
    out.qos_ratio =
        denom > 0.0 ? static_cast<double>(routing.conforming_pairs) / denom : 0.0;
    out.reachable_ratio =
        denom > 0.0 ? static_cast<double>(routing.reachable_pairs) / denom : 0.0;
    out.qos_reachable_ratio =
        routing.reachable_pairs > 0
            ? static_cast<double>(routing.conforming_pairs) /
                  routing.reachable_pairs
            : 0.0;
    out.reachable_pairs = routing.reachable_pairs;
    out.conforming_pairs = routing.conforming_pairs;

    // Статистика задержек и потерь рассчитывается по достижимым путям,
    // не содержащим перегруженных звеньев: путь с задержкой kSaturatedDelay
    // иначе полностью искажает среднее значение.
    std::vector<double> delays_ms;
    delays_ms.reserve(routing.reachable_pairs);
    double loss_sum = 0.0;
    double ipdv_sum = 0.0;
    int samples = 0;

    for (int src = 0; src < n; ++src) {
        for (int dst = 0; dst < n; ++dst) {
            if (src == dst) continue;
            const PairState& pair = routing.pair[src][dst];
            if (!pair.reachable || pair.saturated) continue;
            delays_ms.push_back(pair.delay_s * 1000.0);
            loss_sum += pair.loss;
            ipdv_sum += pair.ipdv_s * 1000.0;
            ++samples;
        }
    }

    if (samples > 0) {
        out.iptd_mean_ms =
            std::accumulate(delays_ms.begin(), delays_ms.end(), 0.0) / samples;
        out.iplr_mean = loss_sum / samples;
        out.ipdv_mean_ms = ipdv_sum / samples;

        std::sort(delays_ms.begin(), delays_ms.end());
        const int idx = std::min(samples - 1,
                                 static_cast<int>(std::ceil(0.95 * samples)) - 1);
        out.iptd_p95_ms = delays_ms[std::max(0, idx)];
    }

    double utilization_sum = 0.0;
    int utilization_count = 0;
    double total_load = 0.0;

    for (int u = 0; u < n; ++u) {
        if (graph.nodes[u].state != 1) continue;
        for (const Edge& edge : graph.adj[u]) {
            const int v = edge.target;
            if (graph.nodes[v].state != 1) continue;
            ++out.active_edges;
            total_load += edge.load;
            if (edge.capacity > 0.0) {
                const double rho = edge.load / edge.capacity;
                utilization_sum += rho;
                out.max_utilization = std::max(out.max_utilization, rho);
                ++utilization_count;
            }
        }
    }

    if (utilization_count > 0)
        out.avg_utilization = utilization_sum / utilization_count;
    out.total_load_gbps = total_load / 1.0e9;

    return out;
}

}  // namespace netsim
