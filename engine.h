#pragma once
#include "topology.h"
#include <vector>
#include <string>

struct SimResult {
    std::vector<double> lcc_history;
    std::vector<double> eff_norm_history;
    std::vector<double> weighted_surv_history;
    std::vector<double> time_points;
    double              first_failure_time;
    double              resources_consumed;
    bool                global_resource_exhausted;
    int                 recoveries_success;
    int                 recoveries_failed;
    std::vector<int>    queue_depth_history;
    std::vector<int>    slots_used_history;
};

SimResult runDetailedSimulation(
    const Graph&       G,
    double             T,
    double             attack_lambda,
    double             total_global_resources,
    const std::string& attack_strategy = "targeted",
    const std::string& gatekeeper      = "fifo",
    int                repair_slots    = 1,
    int                sample_points   = 200,
    uint64_t           rng_seed        = 0
);
