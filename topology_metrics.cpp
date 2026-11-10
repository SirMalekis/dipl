#include "topology_metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <vector>

namespace netsim {

namespace {

void computeComponents(const Graph& graph,
                       int& components,
                       double& lcc_fraction) {
    const int n = graph.num_nodes;
    std::vector<char> visited(n, 0);
    int largest = 0;
    components = 0;

    for (int start = 0; start < n; ++start) {
        if (visited[start]) continue;
        ++components;
        int size = 0;
        std::queue<int> queue;
        queue.push(start);
        visited[start] = 1;
        while (!queue.empty()) {
            const int u = queue.front();
            queue.pop();
            ++size;
            for (const Edge& edge : graph.adj[u]) {
                const int v = edge.target;
                if (!visited[v]) {
                    visited[v] = 1;
                    queue.push(v);
                }
            }
        }
        largest = std::max(largest, size);
    }
    lcc_fraction = n > 0 ? static_cast<double>(largest) / n : 0.0;
}

void computePaths(const Graph& graph, double& avg_path, int& diameter) {
    const int n = graph.num_nodes;
    long long total_distance = 0;
    long long pairs = 0;
    diameter = 0;

    for (int source = 0; source < n; ++source) {
        std::vector<int> distance(n, -1);
        std::queue<int> queue;
        distance[source] = 0;
        queue.push(source);
        while (!queue.empty()) {
            const int u = queue.front();
            queue.pop();
            for (const Edge& edge : graph.adj[u]) {
                const int v = edge.target;
                if (distance[v] == -1) {
                    distance[v] = distance[u] + 1;
                    queue.push(v);
                }
            }
        }
        for (int target = source + 1; target < n; ++target) {
            if (distance[target] < 0) continue;
            total_distance += distance[target];
            ++pairs;
            diameter = std::max(diameter, distance[target]);
        }
    }
    avg_path = pairs > 0 ? static_cast<double>(total_distance) / pairs : 0.0;
}

void computeClustering(const Graph& graph, double& global_clustering) {
    const int n = graph.num_nodes;
    std::vector<std::vector<char>> connected(
        n, std::vector<char>(n, 0));
    for (int u = 0; u < n; ++u)
        for (const Edge& edge : graph.adj[u])
            connected[u][edge.target] = 1;

    long long closed_triples = 0;
    long long triples = 0;
    for (int u = 0; u < n; ++u) {
        std::vector<int> neighbours;
        for (int v = 0; v < n; ++v)
            if (connected[u][v]) neighbours.push_back(v);

        const int degree = static_cast<int>(neighbours.size());
        triples += static_cast<long long>(degree) * (degree - 1) / 2;
        for (size_t i = 0; i < neighbours.size(); ++i)
            for (size_t j = i + 1; j < neighbours.size(); ++j)
                if (connected[neighbours[i]][neighbours[j]])
                    ++closed_triples;
    }
    global_clustering =
        triples > 0 ? static_cast<double>(closed_triples) / triples : 0.0;
}

void computeAssortativity(const Graph& graph,
                          const std::vector<int>& degree,
                          double& assortativity) {
    const int n = graph.num_nodes;
    std::vector<std::vector<char>> connected(
        n, std::vector<char>(n, 0));
    double sum_jk = 0.0;
    double sum_j_plus_k = 0.0;
    double sum_j2_plus_k2 = 0.0;
    long long edges = 0;

    for (int u = 0; u < n; ++u) {
        for (const Edge& edge : graph.adj[u]) {
            const int v = edge.target;
            if (u >= v || connected[u][v]) continue;
            connected[u][v] = 1;
            connected[v][u] = 1;
            const double j = degree[u];
            const double k = degree[v];
            sum_jk += j * k;
            sum_j_plus_k += j + k;
            sum_j2_plus_k2 += j * j + k * k;
            ++edges;
        }
    }

    if (edges == 0) {
        assortativity = 0.0;
        return;
    }
    const double e = static_cast<double>(edges);
    const double mean = sum_j_plus_k / (2.0 * e);
    const double numerator = sum_jk / e - mean * mean;
    const double denominator = sum_j2_plus_k2 / (2.0 * e) - mean * mean;
    assortativity = denominator > 0.0 ? numerator / denominator : 0.0;
}

void computePowerLaw(const std::vector<std::pair<int, int>>& histogram,
                     int n,
                     double& slope,
                     double& gamma_est) {
    slope = 0.0;
    gamma_est = 0.0;
    if (n <= 0 || histogram.empty()) return;

    int cumulative = 0;
    std::vector<double> xs;
    std::vector<double> ys;
    for (auto it = histogram.rbegin(); it != histogram.rend(); ++it) {
        cumulative += it->second;
        const int degree = it->first;
        if (degree < 2) continue;
        const double ccdf = static_cast<double>(cumulative) / n;
        if (ccdf <= 0.0 || ccdf > 1.0) continue;
        xs.push_back(std::log(static_cast<double>(degree)));
        ys.push_back(std::log(ccdf));
    }
    if (xs.size() >= 2) {
        const double mean_x =
            std::accumulate(xs.begin(), xs.end(), 0.0) / xs.size();
        const double mean_y =
            std::accumulate(ys.begin(), ys.end(), 0.0) / ys.size();
        double covariance = 0.0;
        double variance = 0.0;
        for (size_t i = 0; i < xs.size(); ++i) {
            covariance += (xs[i] - mean_x) * (ys[i] - mean_y);
            variance += (xs[i] - mean_x) * (xs[i] - mean_x);
        }
        slope = variance > 0.0 ? covariance / variance : 0.0;
    }

    // Оценка Хилла для дискретного степенного закона (x_min = 2).
    // Устойчивее линейной регрессии CCDF на малых выборках.
    constexpr int kXmin = 2;
    double sum_log = 0.0;
    int tail = 0;
    for (const auto& [degree, count] : histogram) {
        if (degree < kXmin) continue;
        const double value = std::log(
            static_cast<double>(degree) / (kXmin - 0.5));
        sum_log += value * count;
        tail += count;
    }
    if (sum_log > 0.0)
        gamma_est = 1.0 + static_cast<double>(tail) / sum_log;
}

}  // namespace

std::string topologyPurpose(const std::string& topology) {
    if (topology == "erdos_renyi")
        return "Однородный случайный граф G(N,p): базовая модель без "
               "выраженных хабов, нулевая гипотеза для сравнения.";
    if (topology == "barabasi_albert")
        return "Безмасштабная сеть с предпочтительным присоединением: "
               "степенное распределение и хабы, модель реальных сетей.";
    if (topology == "star")
        return "Централизованная звезда: максимальная уязвимость к атаке "
               "на центральный хаб.";
    if (topology == "ring")
        return "Кольцо: локальная связность и устойчивость к каскадам, "
               "но уязвимость к разрывам.";
    if (topology == "full_mesh")
        return "Полносвязная сеть: максимальная избыточность и стоимость.";
    return "Пользовательская топология.";
}

TopologyStructure analyzeTopology(const Graph& graph,
                                  const TopologyParams& params) {
    TopologyStructure out;
    out.topology = graph.topology;
    out.purpose = topologyPurpose(graph.topology);
    out.num_nodes = graph.num_nodes;

    const int n = graph.num_nodes;
    if (n <= 0) return out;

    std::vector<int> degree(n, 0);
    std::map<int, int> histogram;
    double degree_sum = 0.0;
    out.min_degree = std::numeric_limits<int>::max();
    out.max_degree = 0;

    for (int i = 0; i < n; ++i) {
        degree[i] = graph.degree(i);
        degree_sum += degree[i];
        out.min_degree = std::min(out.min_degree, degree[i]);
        out.max_degree = std::max(out.max_degree, degree[i]);
        ++histogram[degree[i]];
    }
    if (out.min_degree == std::numeric_limits<int>::max()) out.min_degree = 0;

    // Уникальные неориентированные рёбра.
    std::vector<std::vector<char>> connected(
        n, std::vector<char>(n, 0));
    int edges = 0;
    for (int u = 0; u < n; ++u) {
        for (const Edge& edge : graph.adj[u]) {
            const int v = edge.target;
            if (u < v && !connected[u][v]) {
                connected[u][v] = 1;
                connected[v][u] = 1;
                ++edges;
            }
        }
    }
    out.num_edges = edges;

    out.avg_degree = n > 0 ? 2.0 * edges / n : 0.0;
    double variance = 0.0;
    for (int value : degree)
        variance += (value - out.avg_degree) * (value - out.avg_degree);
    const double std_degree =
        n > 1 ? std::sqrt(variance / (n - 1)) : 0.0;
    out.degree_cv = out.avg_degree > 0.0 ? std_degree / out.avg_degree : 0.0;

    for (const auto& [value, count] : histogram)
        out.degree_histogram.push_back({value, count});

    computeComponents(graph, out.components, out.lcc_fraction);
    computePaths(graph, out.avg_path_length, out.diameter);
    computeClustering(graph, out.global_clustering);
    computeAssortativity(graph, degree, out.assortativity);
    computePowerLaw(out.degree_histogram, n, out.ccdf_slope, out.gamma_est);
    if (graph.topology != "erdos_renyi" &&
        graph.topology != "barabasi_albert") {
        out.ccdf_slope = std::numeric_limits<double>::quiet_NaN();
        out.gamma_est = std::numeric_limits<double>::quiet_NaN();
    }

    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (graph.topology == "erdos_renyi") {
        out.expected_avg_degree = params.er_p * (n - 1);
    } else if (graph.topology == "barabasi_albert") {
        out.expected_avg_degree = 2.0 * params.ba_m;
    } else if (graph.topology == "star") {
        out.expected_avg_degree = 2.0 * (n - 1) / n;
    } else if (graph.topology == "ring") {
        out.expected_avg_degree = n > 2 ? 2.0 : 1.0;
    } else if (graph.topology == "full_mesh") {
        out.expected_avg_degree = n - 1;
    } else {
        out.expected_avg_degree = nan;
    }

    return out;
}

}  // namespace netsim
