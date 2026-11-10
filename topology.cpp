#include "topology.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>

namespace netsim {

const std::unordered_map<std::string, NodeProfile> NODE_PROFILES = {
    {"server", {0.55, 0.85, 0.15, 3, 10.0}},
    {"switch", {0.65, 0.90, 0.30, 2,  5.0}},
    {"host",   {0.70, 0.75, 0.20, 1,  1.0}},
};

const std::unordered_map<std::string, double> DEFAULT_NODE_MIX = {
    {"server", 0.15},
    {"switch", 0.20},
    {"host",   0.65},
};

namespace {

double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

double jitter(std::mt19937_64& rng, double base, double spread,
              double lo = 0.01, double hi = 0.99) {
    std::uniform_real_distribution<double> d(-spread, spread);
    return clamp(base + d(rng), lo, hi);
}

std::string sampleType(std::mt19937_64& rng,
                       const std::unordered_map<std::string, double>& mix) {
    // Фиксированный порядок обхода обеспечивает воспроизводимость.
    static const char* order[] = {"server", "switch", "host"};
    std::uniform_real_distribution<double> u(0.0, 1.0);
    const double r = u(rng);
    double cum = 0.0;
    for (const char* type : order) {
        auto it = mix.find(type);
        if (it == mix.end()) continue;
        cum += it->second;
        if (r <= cum) return type;
    }
    return "host";
}

// Связные компоненты (для гарантии связности ER-графа).
std::vector<std::vector<int>> components(const Graph& g) {
    const int n = g.num_nodes;
    std::vector<int> comp(n, -1);
    std::vector<std::vector<int>> result;
    for (int s = 0; s < n; ++s) {
        if (comp[s] != -1) continue;
        const int id = static_cast<int>(result.size());
        result.push_back({});
        std::queue<int> q;
        q.push(s);
        comp[s] = id;
        while (!q.empty()) {
            const int u = q.front();
            q.pop();
            result[id].push_back(u);
            for (const auto& e : g.adj[u]) {
                if (comp[e.target] == -1) {
                    comp[e.target] = id;
                    q.push(e.target);
                }
            }
        }
    }
    return result;
}

void ensureConnected(Graph& g, double capacity, double latency) {
    auto comps = components(g);
    if (comps.size() <= 1) return;
    const int hub = comps[0].front();
    for (size_t k = 1; k < comps.size(); ++k) {
        const int other = comps[k].front();
        g.addBidirectional(hub, other, capacity, latency);
    }
}

// Назначение пропускной способности дугам по степеням вершин: каналы,
// соединяющие высокосвязные узлы, получают ёмкость ядра, периферийные —
// ёмкость доступа. Это создаёт неоднородность, характерную для реальных сетей.
void assignCapacitiesByDegree(Graph& g, double access_capacity, double core_capacity) {
    const int n = g.num_nodes;
    if (n == 0) return;
    double sum_deg = 0.0;
    for (int i = 0; i < n; ++i) sum_deg += g.degree(i);
    const double avg_deg = sum_deg / n;
    for (int u = 0; u < n; ++u) {
        for (auto& e : g.adj[u]) {
            const int v = e.target;
            const double min_deg = std::min(g.degree(u), g.degree(v));
            e.capacity = (min_deg >= avg_deg * 1.10) ? core_capacity : access_capacity;
        }
    }
}

void addLink(Graph& g, int u, int v, double capacity, double latency) {
    if (u == v) return;
    g.addBidirectional(u, v, capacity, latency);
}

}  // namespace

bool isKnownTopology(const std::string& name) {
    const auto all = availableTopologies();
    return std::find(all.begin(), all.end(), name) != all.end();
}

std::vector<std::string> availableTopologies() {
    return {"star", "ring", "full_mesh", "erdos_renyi", "barabasi_albert"};
}

std::string topologyDescription(const std::string& name) {
    if (name == "star") return "Звезда: центральный хаб и периферийные узлы";
    if (name == "ring") return "Кольцо: каждый узел связан с двумя соседями";
    if (name == "full_mesh") return "Полносвязная сеть: все узлы соединены попарно";
    if (name == "erdos_renyi") return "Случайный граф Эрдёша–Реньи G(N, p)";
    if (name == "barabasi_albert") return "Безмасштабная сеть Барабаши–Альберта";
    return "Неизвестная топология";
}

Graph createNetworkTopology(const TopologyParams& p) {
    if (p.num_nodes < 2)
        throw std::invalid_argument("Количество узлов должно быть не меньше 2");
    if (!isKnownTopology(p.topology))
        throw std::invalid_argument(
            "Неизвестная топология: '" + p.topology +
            "'. Допустимые: star, ring, full_mesh, erdos_renyi, barabasi_albert");

    const int n = p.num_nodes;
    const double acc_cap = std::max(1.0, p.access_capacity);
    const double core_cap = std::max(acc_cap, p.core_capacity);
    const double base_lat = std::max(1.0e-6, p.base_latency);
    const double jitter_lat = std::max(0.0, p.latency_jitter);

    std::mt19937_64 rng(p.seed == 0 ? std::random_device{}() : p.seed);
    std::uniform_real_distribution<double> lat_noise(-jitter_lat, jitter_lat);
    auto latency = [&]() { return std::max(1.0e-6, base_lat + lat_noise(rng)); };

    Graph g;
    g.topology = p.topology;
    g.num_nodes = n;
    g.nodes.resize(n);
    g.adj.resize(n);

    if (p.topology == "star") {
        for (int i = 1; i < n; ++i)
            addLink(g, 0, i, core_cap, latency());
    } else if (p.topology == "ring") {
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            if (i < j) addLink(g, i, j, core_cap, latency());
        }
    } else if (p.topology == "full_mesh") {
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                addLink(g, i, j, core_cap, latency());
    } else if (p.topology == "erdos_renyi") {
        const double prob = clamp(p.er_p, 0.0, 1.0);
        std::uniform_real_distribution<double> u01(0.0, 1.0);
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (u01(rng) <= prob)
                    addLink(g, i, j, core_cap, latency());
        ensureConnected(g, core_cap, latency());
    } else if (p.topology == "barabasi_albert") {
        int m = std::max(1, std::min(p.ba_m, n - 1));
        int m0 = std::max(m + 1, 3);
        if (m0 > n) m0 = n;

        // Начальное ядро — полный граф на m0 вершинах.
        for (int i = 0; i < m0; ++i)
            for (int j = i + 1; j < m0; ++j)
                addLink(g, i, j, core_cap, latency());

        // Список повторений вершин пропорционально их степеням.
        std::vector<int> repeated;
        for (int i = 0; i < m0; ++i)
            for (int k = 0; k < g.degree(i); ++k) repeated.push_back(i);
        if (repeated.empty()) {
            for (int i = 0; i < m0; ++i) repeated.push_back(i);
        }

        std::uniform_int_distribution<size_t> pick(0, repeated.size() - 1);
        for (int v = m0; v < n; ++v) {
            std::vector<int> targets;
            targets.reserve(m);
            int guard = 0;
            while (static_cast<int>(targets.size()) < m && guard < 10000) {
                const int t = repeated[pick(rng)];
                if (t != v &&
                    std::find(targets.begin(), targets.end(), t) == targets.end())
                    targets.push_back(t);
                ++guard;
            }
            for (int t : targets) {
                addLink(g, v, t, core_cap, latency());
                repeated.push_back(v);
                repeated.push_back(t);
            }
        }
        ensureConnected(g, core_cap, latency());
    }

    if (p.topology == "erdos_renyi" || p.topology == "barabasi_albert")
        assignCapacitiesByDegree(g, acc_cap, core_cap);

    g.rebuildEdgeIndex();

    // -----------------------------------------------------------------
    // Профилирование узлов
    // -----------------------------------------------------------------
    std::vector<std::pair<int, int>> degree_with_id(n);
    for (int i = 0; i < n; ++i) degree_with_id[i] = {g.degree(i), i};
    std::sort(degree_with_id.begin(), degree_with_id.end(),
              [](const auto& a, const auto& b) {
                  if (a.first != b.first) return a.first > b.first;
                  return a.second < b.second;
              });

    const bool hetero = p.heterogeneous;
    const double mu_mult = (p.base_mu > 0.0) ? (p.base_mu / 0.20) : 1.0;
    const double attack_mult =
        (p.base_P_attack > 0.0) ? (p.base_P_attack / 0.60) : 1.0;

    int n_servers = std::max(1, static_cast<int>(n * 0.15));
    int n_switches = std::max(1, static_cast<int>(n * 0.20));
    if (n_servers + n_switches > n - 1) {
        n_switches = std::max(1, n - n_servers - 1);
        if (n_servers + n_switches > n) n_switches = 0;
    }

    for (int i = 0; i < n; ++i) {
        Node& node = g.nodes[i];
        node.id = i;
        node.state = 1;
        node.node_type = "host";

        if (hetero) {
            const int rank = static_cast<int>(
                std::find_if(degree_with_id.begin(), degree_with_id.end(),
                             [i](const auto& pair) { return pair.second == i; }) -
                degree_with_id.begin());
            std::string type = "host";
            if (rank < n_servers) type = "server";
            else if (rank < n_servers + n_switches) type = "switch";

            const NodeProfile& prof = NODE_PROFILES.at(type);
            node.node_type = type;
            node.P_attack = jitter(rng, prof.P_attack * attack_mult, 0.05);
            node.P_recovery = jitter(rng, prof.P_recovery, 0.05);
            node.mu = clamp(prof.mu * mu_mult +
                            std::uniform_real_distribution<double>(-0.02, 0.02)(rng),
                            0.01, 1.0);
            node.C_repair = prof.C_repair;
            node.weight = prof.weight;
        } else {
            node.P_attack = clamp(p.base_P_attack +
                                  std::uniform_real_distribution<double>(-0.05, 0.05)(rng),
                                  0.01, 0.99);
            node.P_recovery = clamp(p.base_P_recovery +
                                    std::uniform_real_distribution<double>(-0.05, 0.05)(rng),
                                    0.01, 0.99);
            node.mu = clamp(p.base_mu +
                            std::uniform_real_distribution<double>(-0.01, 0.01)(rng),
                            0.01, 1.0);
            node.C_repair = p.base_C_repair;
            node.weight = p.base_weight;
        }
    }

    return g;
}

}  // namespace netsim
