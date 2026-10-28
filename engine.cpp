#include "engine.h"
#include "metrics.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <numeric>
#include <limits>

// ---------------------------------------------------------------------------
// Выбор цели атаки
// ---------------------------------------------------------------------------
static int selectAttackTarget(const Graph& G,
                               const std::vector<int>& active_nodes,
                               const std::string& strategy,
                               std::mt19937_64& rng) {
    if (active_nodes.empty()) return -1;

    if (strategy == "targeted") {
        int best = -1, best_deg = -1;
        for (int n : active_nodes) {
            int d = G.degree(n);
            if (d > best_deg) { best_deg = d; best = n; }
        }
        // Если несколько с максимальной степенью — случайный выбор среди них
        std::vector<int> candidates;
        for (int n : active_nodes)
            if (G.degree(n) == best_deg) candidates.push_back(n);
        std::uniform_int_distribution<int> uid(0, (int)candidates.size()-1);
        return candidates[uid(rng)];
    }
    else if (strategy == "random") {
        std::uniform_int_distribution<int> uid(0, (int)active_nodes.size()-1);
        return active_nodes[uid(rng)];
    }
    else if (strategy == "cascading") {
        // Соседи уже отказавших узлов
        std::vector<bool> exposed_set(G.num_nodes, false);
        std::vector<int>  exposed;
        for (int n = 0; n < G.num_nodes; ++n) {
            if (G.nodes[n].state == 0) {
                for (const Edge& edge : G.adj[n]) {
                    int nb = edge.target;
                    if (G.nodes[nb].state == 1 && !exposed_set[nb]) {
                        exposed_set[nb] = true;
                        exposed.push_back(nb);
                    }
                }
            }
        }
        const auto& pool = exposed.empty() ? active_nodes : exposed;
        std::uniform_int_distribution<int> uid(0, (int)pool.size()-1);
        return pool[uid(rng)];
    }
    else {
        throw std::invalid_argument(
            "Неизвестная стратегия: '" + strategy +
            "'. Допустимые: targeted, random, cascading.");
    }
}

// ---------------------------------------------------------------------------
// Построение очереди восстановления
// ---------------------------------------------------------------------------
static std::vector<int> buildRepairQueue(Graph& G,
                                          const std::string& gatekeeper,
                                          double t) {
    std::vector<int> ready;
    for (int i = 0; i < G.num_nodes; ++i) {
        const Node& nd = G.nodes[i];
        if (nd.state == 0
            && nd.recovery_end_time != -1.0
            && nd.recovery_end_time != std::numeric_limits<double>::infinity()
            && nd.recovery_end_time <= t)
        {
            ready.push_back(i);
        }
    }
    if (ready.empty()) return ready;

    if (gatekeeper == "fifo") {
        std::sort(ready.begin(), ready.end(), [&](int a, int b){
            return G.nodes[a].recovery_end_time < G.nodes[b].recovery_end_time;
        });
    } else if (gatekeeper == "triage") {
        std::sort(ready.begin(), ready.end(), [&](int a, int b){
            return G.nodes[a].weight > G.nodes[b].weight;
        });
    }
    return ready;
}

// ---------------------------------------------------------------------------
// Основной движок
// ---------------------------------------------------------------------------
SimResult runDetailedSimulation(
    const Graph&       G_orig,
    double             T,
    double             attack_lambda,
    double             total_global_resources,
    const std::string& attack_strategy,
    const std::string& gatekeeper,
    int                repair_slots,
    int                sample_points,
    uint64_t           rng_seed)
{
    Graph G = G_orig;  // рабочая копия
    std::mt19937_64 rng(rng_seed == 0 ? std::random_device{}() : rng_seed);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::exponential_distribution<double>  expDist(1.0);  // λ=1, масштабируем вручную

    auto expSample = [&](double rate) -> double {
        if (rate <= 0) return std::numeric_limits<double>::infinity();
        return expDist(rng) / rate;
    };

    int    N               = G.num_nodes;
    double E0              = computeInitialEfficiency(G);
    double current_res     = total_global_resources;
    bool   repair_forbidden = (total_global_resources == 0.0);

    SimResult stats;
    stats.first_failure_time        = T;
    stats.resources_consumed        = 0.0;
    stats.global_resource_exhausted = false;
    stats.recoveries_success        = 0;
    stats.recoveries_failed         = 0;

    // Генерация потока атак (Пуассон)
    std::vector<double> attack_times;
    if (attack_lambda > 0.0) {
        double t_curr = expSample(attack_lambda);
        while (t_curr <= T) {
            attack_times.push_back(t_curr);
            t_curr += expSample(attack_lambda);
        }
    }
    int attack_idx   = 0;
    bool failure_seen = false;

    // Временная ось
    double dt = T / (sample_points - 1);
    stats.time_points.resize(sample_points);
    for (int i = 0; i < sample_points; ++i)
        stats.time_points[i] = i * dt;
    // Точная последняя точка
    if (sample_points > 0) stats.time_points.back() = T;

    stats.lcc_history.reserve(sample_points);
    stats.eff_norm_history.reserve(sample_points);
    stats.weighted_surv_history.reserve(sample_points);
    stats.queue_depth_history.reserve(sample_points);
    stats.slots_used_history.reserve(sample_points);

    for (int step = 0; step < sample_points; ++step) {
        double t = stats.time_points[step];

        // ── A: Атаки ─────────────────────────────────────────────────
        while (attack_idx < (int)attack_times.size() && attack_times[attack_idx] <= t) {
            double t_atk = attack_times[attack_idx];
            std::vector<int> active;
            active.reserve(N);
            for (int i = 0; i < N; ++i)
                if (G.nodes[i].state == 1) active.push_back(i);

            if (!active.empty()) {
                int target = selectAttackTarget(G, active, attack_strategy, rng);
                if (target >= 0) {
                    if (u01(rng) < G.nodes[target].P_attack) {
                        G.nodes[target].state = 0;
                        if (!failure_seen) {
                            stats.first_failure_time = t_atk;
                            failure_seen = true;
                        }
                        double mu = std::max(G.nodes[target].mu, 1e-9);
                        G.nodes[target].recovery_end_time = t_atk + expSample(mu);
                    }
                }
            }
            ++attack_idx;
        }

        // ── B: Восстановление ────────────────────────────────────────
        int slots_used = 0;
        if (!repair_forbidden) {
            auto queue = buildRepairQueue(G, gatekeeper, t);
            stats.queue_depth_history.push_back((int)queue.size());

            for (int node : queue) {
                if (slots_used >= repair_slots) break;

                Node& nd = G.nodes[node];
                int cost = nd.C_repair;

                if (current_res >= cost) {
                    current_res              -= cost;
                    stats.resources_consumed += cost;
                    ++slots_used;

                    if (u01(rng) < nd.P_recovery) {
                        nd.state              = 1;
                        nd.recovery_end_time  = -1.0;
                        ++stats.recoveries_success;
                    } else {
                        ++stats.recoveries_failed;
                        double mu = std::max(nd.mu, 1e-9);
                        nd.recovery_end_time = t + expSample(mu);
                    }
                } else {
                    stats.global_resource_exhausted = true;
                    // Временный дефицит — ждём следующего шага
                }
            }
        } else {
            stats.queue_depth_history.push_back(0);
        }
        stats.slots_used_history.push_back(slots_used);

        // ── C: Метрики ───────────────────────────────────────────────
        // Имитируем передачу 500 Кбит/с между каждой парой узлов (all-to-all)
        // Для N=50 это может создать пиковую нагрузку до 1.2 Гбит/с на центральных каналах
        recalculateTraffic(G, 500000.0);

        stats.lcc_history.push_back(computeLccRatio(G, N));
        stats.eff_norm_history.push_back(computeNormalizedEfficiency(G, E0));
        stats.weighted_surv_history.push_back(computeWeightedSurvivability(G));
    }

    return stats;
}
