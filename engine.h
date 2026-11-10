#pragma once
// ---------------------------------------------------------------------------
// engine.h — дискретно-событийный движок Монте-Карло.
//
// События: пуассоновские атаки, завершение ремонта. Восстановление
// моделируется пулом из repair_slots бригад: узел занимает бригаду на
// случайное время Exp(μ), после чего с вероятностью P_recovery считается
// восстановленным; при неудаче возвращается в очередь. Диспетчер FIFO /
// Triage выбирает, какого отказавшего узла ремонтировать следующим.
// ---------------------------------------------------------------------------

#include "config.h"
#include "metrics.h"
#include "topology.h"

#include <vector>

namespace netsim {

struct SimResult {
    std::vector<double> time_points;
    std::vector<NetworkMetrics> metrics;

    double first_failure_time = 0.0;
    bool   had_failure = false;
    double resources_consumed = 0.0;
    bool   global_resource_exhausted = false;

    int attacks = 0;
    int attacks_successful = 0;
    int recoveries_success = 0;
    int recoveries_failed = 0;

    int    max_queue_depth = 0;
    double mean_queue_depth = 0.0;
    double mean_slots_used = 0.0;

    double e0_phys = 0.0;
    double e0_qos = 0.0;
    bool   baseline_qos_ok = false;

    int num_nodes = 0;
};

// Запуск одной реализации на копии графа.
SimResult runSimulation(const Graph& graph, const SimConfig& config);

}  // namespace netsim
