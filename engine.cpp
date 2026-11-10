#include "engine.h"

#include "qos.h"
#include "traffic.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>

namespace netsim {

namespace {

constexpr double kEps = 1.0e-12;

enum class EventType {
    Attack,
    RepairComplete
};

struct Event {
    double    time = 0.0;
    EventType type = EventType::Attack;
    int       node = -1;
    uint64_t  seq = 0;
};

struct EventCompare {
    bool operator()(const Event& a, const Event& b) const {
        if (a.time != b.time) return a.time > b.time;
        return a.seq > b.seq;
    }
};

}  // namespace

SimResult runSimulation(const Graph& graph, const SimConfig& config) {
    if (config.sample_points < 2)
        throw std::invalid_argument("sample_points должно быть ≥ 2");

    Graph g = graph;
    g.rebuildEdgeIndex();

    const QosParams qos = config.qosParams();
    const TrafficParams traffic = config.trafficParams();
    const int n = g.num_nodes;

    std::mt19937_64 rng(config.seed == 0 ? std::random_device{}() : config.seed);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::exponential_distribution<double> exp_unit(1.0);

    auto sampleExp = [&](double rate) -> double {
        if (rate <= 0.0) return std::numeric_limits<double>::infinity();
        return exp_unit(rng) / rate;
    };

    // -----------------------------------------------------------------
    // Калибровка: эталонная эффективность полностью исправной сети.
    // -----------------------------------------------------------------
    MetricsContext context;
    context.qos = qos;

    TrafficAssignment initial = assignTraffic(g, traffic, qos);
    context.calibrate(initial.routing);

    RoutingMatrix current_routing = initial.routing;
    NetworkMetrics current_metrics =
        computeMetrics(g, current_routing, context, 0.0);
    bool dirty = false;

    SimResult result;
    result.num_nodes = n;
    result.e0_phys = context.e0_phys;
    result.e0_qos = context.e0_qos;
    result.baseline_qos_ok = context.baseline_qos_ok;
    result.first_failure_time = config.T;

    // -----------------------------------------------------------------
    // Состояние восстановления
    // -----------------------------------------------------------------
    std::vector<char> in_repair(n, 0);
    std::vector<int>  waiting;
    std::vector<double> failure_time(n, 0.0);
    double resources = config.global_resources;
    int used_slots = 0;
    bool resource_exhausted = false;

    std::priority_queue<Event, std::vector<Event>, EventCompare> events;
    uint64_t next_seq = 0;

    auto schedule = [&](double time, EventType type, int node) {
        if (!std::isfinite(time)) return;
        events.push(Event{time, type, node, next_seq++});
    };

    // Выбор узла из очереди ожидания.
    auto selectWaiting = [&]() -> int {
        int best = -1;
        for (int node : waiting) {
            if (in_repair[node]) continue;
            if (best < 0) {
                best = node;
                continue;
            }
            if (config.gatekeeper == "triage") {
                const Node& candidate = g.nodes[node];
                const Node& current = g.nodes[best];
                if (candidate.weight > current.weight) {
                    best = node;
                } else if (candidate.weight == current.weight &&
                           failure_time[node] < failure_time[best]) {
                    best = node;
                }
            } else {
                if (failure_time[node] < failure_time[best] ||
                    (failure_time[node] == failure_time[best] && node < best)) {
                    best = node;
                }
            }
        }
        return best;
    };

    std::function<void(double)> dispatch = [&](double now) {
        if (config.repair_slots <= 0) return;
        while (used_slots < config.repair_slots) {
            const int node = selectWaiting();
            if (node < 0) break;

            Node& nd = g.nodes[node];
            const double cost = static_cast<double>(nd.C_repair);
            if (resources + kEps < cost) {
                resource_exhausted = true;
                break;
            }

            waiting.erase(std::find(waiting.begin(), waiting.end(), node));
            resources -= cost;
            result.resources_consumed += cost;
            ++used_slots;
            in_repair[node] = 1;

            double duration = sampleExp(nd.mu);
            if (!std::isfinite(duration)) duration = config.T;
            duration = std::max(duration, 1.0e-9);
            schedule(now + duration, EventType::RepairComplete, node);
        }
    };

    auto selectAttackTarget = [&](const std::vector<int>& active) -> int {
        if (active.empty()) return -1;

        if (config.attack_strategy == "random") {
            std::uniform_int_distribution<size_t> pick(0, active.size() - 1);
            return active[pick(rng)];
        }

        if (config.attack_strategy == "cascading") {
            std::vector<char> exposed_flag(n, 0);
            std::vector<int> exposed;
            for (int node = 0; node < n; ++node) {
                if (g.nodes[node].state != 0) continue;
                for (const Edge& edge : g.adj[node]) {
                    const int nb = edge.target;
                    if (g.nodes[nb].state == 1 && !exposed_flag[nb]) {
                        exposed_flag[nb] = 1;
                        exposed.push_back(nb);
                    }
                }
            }
            const std::vector<int>& pool = exposed.empty() ? active : exposed;
            std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
            return pool[pick(rng)];
        }

        // targeted: узел с максимальной степенью, ties — случайно.
        int best_degree = -1;
        for (int node : active) best_degree = std::max(best_degree, g.degree(node));
        std::vector<int> candidates;
        for (int node : active)
            if (g.degree(node) == best_degree) candidates.push_back(node);
        std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
        return candidates[pick(rng)];
    };

    auto registerFailure = [&](int node, double now) {
        g.nodes[node].state = 0;
        failure_time[node] = now;
        waiting.push_back(node);
        result.max_queue_depth =
            std::max(result.max_queue_depth, static_cast<int>(waiting.size()));
        if (!result.had_failure) {
            result.had_failure = true;
            result.first_failure_time = now;
        }
        dirty = true;
    };

    auto processEvent = [&](const Event& event) {
        const double now = event.time;

        if (event.type == EventType::Attack) {
            ++result.attacks;

            std::vector<int> active;
            active.reserve(n);
            for (int i = 0; i < n; ++i)
                if (g.nodes[i].state == 1) active.push_back(i);

            const int target = selectAttackTarget(active);
            if (target >= 0 && u01(rng) < g.nodes[target].P_attack) {
                ++result.attacks_successful;
                registerFailure(target, now);
                dispatch(now);
            }

            if (config.attack_lambda > 0.0) {
                const double next = now + sampleExp(config.attack_lambda);
                schedule(next, EventType::Attack, -1);
            }
            return;
        }

        // Завершение ремонта.
        const int node = event.node;
        if (node < 0 || node >= n) return;
        --used_slots;
        in_repair[node] = 0;

        Node& nd = g.nodes[node];
        if (u01(rng) < nd.P_recovery) {
            nd.state = 1;
            ++result.recoveries_success;
        } else {
            ++result.recoveries_failed;
            waiting.push_back(node);
            failure_time[node] = now;
            result.max_queue_depth =
                std::max(result.max_queue_depth, static_cast<int>(waiting.size()));
        }
        dirty = true;
        dispatch(now);
    };

    // -----------------------------------------------------------------
    // Планирование первой атаки и временная сетка
    // -----------------------------------------------------------------
    if (config.attack_lambda > 0.0) {
        const double first = sampleExp(config.attack_lambda);
        schedule(first, EventType::Attack, -1);
    }

    std::vector<double> times(config.sample_points);
    for (int i = 0; i < config.sample_points; ++i)
        times[i] = config.T * static_cast<double>(i) /
                   static_cast<double>(config.sample_points - 1);
    times.back() = config.T;

    result.time_points = times;
    result.metrics.reserve(config.sample_points);

    double last_integral_time = 0.0;
    double queue_area = 0.0;
    double slots_area = 0.0;

    for (int step = 0; step < config.sample_points; ++step) {
        const double now = times[step];

        while (!events.empty() && events.top().time <= now + kEps) {
            const Event event = events.top();
            events.pop();

            queue_area += static_cast<double>(waiting.size()) *
                          (event.time - last_integral_time);
            slots_area += static_cast<double>(used_slots) *
                          (event.time - last_integral_time);
            last_integral_time = event.time;

            processEvent(event);
        }

        if (dirty) {
            TrafficAssignment assignment = assignTraffic(g, traffic, qos);
            current_routing = std::move(assignment.routing);
            current_metrics = computeMetrics(g, current_routing, context, now);
            dirty = false;
        } else {
            current_metrics.time = now;
        }

        result.metrics.push_back(current_metrics);
    }

    // Точные интегралы по времени для средних значений очереди и занятых бригад.
    queue_area += static_cast<double>(waiting.size()) *
                  (config.T - last_integral_time);
    slots_area += static_cast<double>(used_slots) *
                  (config.T - last_integral_time);

    if (config.T > 0.0) {
        result.mean_queue_depth = queue_area / config.T;
        result.mean_slots_used = slots_area / config.T;
    }
    result.global_resource_exhausted = resource_exhausted;

    return result;
}

}  // namespace netsim
