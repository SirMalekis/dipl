#include "topology.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Профили
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Вспомогательные
// ---------------------------------------------------------------------------
static double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

static double jitter(std::mt19937_64& rng, double base, double spread,
    double lo = 0.01, double hi = 0.99) {
    std::uniform_real_distribution<double> d(-spread, spread);
    return clamp(base + d(rng), lo, hi);
}

// Дискретное сэмплирование из categorical distribution
static std::string sampleType(std::mt19937_64& rng,
    const std::unordered_map<std::string, double>& mix) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r = u(rng), cum = 0.0;
    for (auto& [type, prob] : mix) {
        cum += prob;
        if (r <= cum) return type;
    }
    return mix.begin()->first;
}

// ---------------------------------------------------------------------------
// Создание топологии
// ---------------------------------------------------------------------------
Graph createNetworkTopology(
    const std::string& topology_type,
    int                num_nodes,
    double             base_P_attack,
    double             base_P_recovery,
    double             base_mu,
    int                base_C_repair,
    double             base_weight,
    bool               heterogeneous,
    const std::unordered_map<std::string, double>& node_mix_arg,
    uint64_t           seed)
{
    const auto& node_mix = node_mix_arg.empty() ? DEFAULT_NODE_MIX : node_mix_arg;

    std::mt19937_64 rng(seed == 0 ? std::random_device{}() : seed);
    std::uniform_real_distribution<double> u05(-0.05, 0.05);
    std::uniform_real_distribution<double> u01(-0.01, 0.01);

    Graph G;
    G.topology = topology_type;
    G.num_nodes = num_nodes;
    G.nodes.resize(num_nodes);
    G.adj.resize(num_nodes);

    // ── 1. СНАЧАЛА создаём рёбра ────────────────────────────────────────
    if (topology_type == "star") {
        for (int i = 1; i < num_nodes; ++i)
            G.addEdge(0, i);
    }
    else if (topology_type == "full_mesh") {
        for (int i = 0; i < num_nodes; ++i)
            for (int j = i + 1; j < num_nodes; ++j)
                G.addEdge(i, j);
    }
    else if (topology_type == "ring") {
        for (int i = 0; i < num_nodes; ++i)
            G.addEdge(i, (i + 1) % num_nodes);
    }
    else if (topology_type == "erdos_renyi") {
        double p = 0.3; // Вероятность связи (можно вынести в аргументы)
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        for (int i = 0; i < num_nodes; ++i) {
            for (int j = i + 1; j < num_nodes; ++j) {
                if (dist(rng) <= p) G.addEdge(i, j);
            }
        }
    }
    else if (topology_type == "barabasi_albert") {
        int m0 = 3; // Начальное ядро
        int m = 2;  // Связей на новый узел
        std::vector<int> degree_list;
        for (int i = 0; i < m0; ++i) {
            for (int j = i + 1; j < m0; ++j) {
                G.addEdge(i, j);
                degree_list.push_back(i); degree_list.push_back(j);
            }
        }
        for (int i = m0; i < num_nodes; ++i) {
            std::vector<int> targets;
            std::uniform_int_distribution<int> dist(0, degree_list.size() - 1);
            while (targets.size() < (size_t)m) {
                int target = degree_list[dist(rng)];
                if (std::find(targets.begin(), targets.end(), target) == targets.end())
                    targets.push_back(target);
            }
            for (int target : targets) {
                G.addEdge(i, target);
                degree_list.push_back(i); degree_list.push_back(target);
            }
        }
    }
    else {
        throw std::invalid_argument(
            "Неизвестная топология: '" + topology_type +
            "'. Допустимые: star, full_mesh, ring, erdos_renyi, barabasi_albert");
    }

    // ── 2. ТЕПЕРЬ назначаем атрибуты узлам ──────────────────────────────
    for (int i = 0; i < num_nodes; ++i) {
        Node& n = G.nodes[i];
        n.id = i;
        n.state = 1;
        n.recovery_end_time = -1.0;

        if (heterogeneous) {
            // 1. Собираем степени всех узлов (ТЕПЕРЬ ОНИ КОРРЕКТНЫ!)
            std::vector<std::pair<int, int>> degree_with_id(num_nodes);
            for (int i = 0; i < num_nodes; ++i) {
                degree_with_id[i] = { G.degree(i), i };
            }

            // 2. Сортируем по убыванию степени
            std::sort(degree_with_id.begin(), degree_with_id.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });

            // 3. Назначаем типы по рангу
            int n_servers = std::max(1, (int)(num_nodes * 0.15));
            int n_switches = std::max(1, (int)(num_nodes * 0.20));

            // ── ДОБАВЛЕНО: Множители для sweep ─────────────────────────────
            // Базовые значения (0.20 для mu, 0.60 для P_attack) используются
            // как точка отсчёта. Если sweep задаёт base_mu=0.40 (в 2 раза больше),
            // то все mu узлов удваиваются, сохраняя пропорции между типами.
            double mu_mult = (base_mu > 0.0) ? (base_mu / 0.20) : 1.0;
            double p_attack_mult = (base_P_attack > 0.0) ? (base_P_attack / 0.60) : 1.0;

            for (int rank = 0; rank < num_nodes; ++rank) {
                int node_id = degree_with_id[rank].second;
                Node& n = G.nodes[node_id];

                std::string ntype;
                if (rank < n_servers) {
                    ntype = "server";
                }
                else if (rank < n_servers + n_switches) {
                    ntype = "switch";
                }
                else {
                    ntype = "host";
                }

                const NodeProfile& prof = NODE_PROFILES.at(ntype);
                n.node_type = ntype;

                // ── ИЗМЕНЕНО: Применяем множители ──────────────────────────
                n.P_attack = jitter(rng, prof.P_attack * p_attack_mult, 0.05);
                n.P_recovery = jitter(rng, prof.P_recovery, 0.05);
                n.mu = clamp(prof.mu * mu_mult + std::uniform_real_distribution<double>(-0.02, 0.02)(rng), 0.01, 1.0);
                n.C_repair = prof.C_repair;
                n.weight = prof.weight;
            }
        }
        else {
            // Гомогенный режим
            n.node_type = "host";
            n.P_attack = clamp(base_P_attack + u05(rng), 0.01, 0.99);
            n.P_recovery = clamp(base_P_recovery + u05(rng), 0.01, 0.99);
            n.mu = clamp(base_mu + u01(rng), 0.01, 1.00);
            n.C_repair = base_C_repair;
            n.weight = base_weight;
        }
    }

    return G;
}