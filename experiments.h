#pragma once
#include "topology.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <map>

// ---------------------------------------------------------------------------
// Статистика одного прогона MC
// ---------------------------------------------------------------------------
struct MCResult {
    double lcc_mean,    lcc_std,    lcc_ci,    lcc_se;
    double eff_mean,    eff_std,    eff_ci,    eff_se;
    double wsurv_mean,  wsurv_std,  wsurv_ci,  wsurv_se;
    double ffail_mean,  ffail_std,  ffail_ci,  ffail_se;
    double res_mean,    res_std,    res_ci,    res_se;
    double exhaustion_rate;
    std::vector<double> lcc_samples;
    std::vector<double> wsurv_samples;
};

// ---------------------------------------------------------------------------
// Результат попарного теста значимости
// ---------------------------------------------------------------------------
struct SigResult {
    double      U;
    double      p_raw;
    double      p_holm;
    bool        significant;
    std::string direction;
};

// Тип: {(a,b) -> SigResult}
using SigMap = std::map<std::pair<std::string,std::string>, SigResult>;

// ---------------------------------------------------------------------------
// Параметры топологии для передачи в MC
// ---------------------------------------------------------------------------
struct TopoKwargs {
    double base_P_attack   = 0.60;
    double base_P_recovery = 0.80;
    double base_mu         = 0.20;
    int    base_C_repair   = 1;
    double base_weight     = 1.0;
    bool   heterogeneous   = false;
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
std::pair<double,double> confidenceInterval(
    const std::vector<double>& samples, double alpha = 0.05);

SigMap pairwiseSignificance(
    const std::map<std::string, std::vector<double>>& raw_samples,
    double alpha = 0.05);

MCResult runMonteCarlo(
    const std::string& topology,
    int                num_nodes,
    const TopoKwargs&  tk,
    double             T,
    double             attack_lambda,
    double             global_resources,
    const std::string& attack_strategy = "targeted",
    const std::string& gatekeeper      = "fifo",
    int                n_sims          = 30,
    int                repair_slots    = 1,
    double             ci_alpha        = 0.05
);

// Сравнение топологий
struct TopoCompResult {
    std::map<std::string, MCResult> results;
    SigMap                          significance;
};
TopoCompResult runTopologyComparison(
    const std::vector<std::string>& topologies,
    int                             num_nodes,
    const TopoKwargs&               tk,
    double                          T,
    double                          attack_lambda,
    double                          global_resources,
    const std::string&              attack_strategy = "targeted",
    const std::string&              gatekeeper      = "fifo",
    int                             n_sims          = 30,
    int                             repair_slots    = 1
);

// Параметрический sweep
struct SweepResult {
    std::vector<double> lcc_means;
    std::vector<double> lcc_ci;
    std::vector<double> eff_means;
    std::vector<double> eff_ci;
    std::vector<double> wsurv_means;
    std::vector<double> wsurv_ci;
};
SweepResult runParameterSweep(
    const std::string&         param_name,
    const std::vector<double>& param_values,
    const std::map<std::string,double>& fixed_params,
    const std::string&         topology,
    int                        num_nodes,
    const std::string&         attack_strategy = "targeted",
    const std::string&         gatekeeper      = "fifo",
    int                        n_sims          = 20,
    double                     T               = 150.0,
    int                        repair_slots    = 1
);

// FIFO vs Triage
struct StrategyPoint {
    MCResult mc;
    struct Sig {
        double U, p;
        bool   significant;
        std::string direction;
    } sig;
};
// {topo -> {gatekeeper -> {R -> StrategyPoint}}}
using StrategyResults = std::map<std::string,
    std::map<std::string,
        std::map<double, StrategyPoint>>>;

StrategyResults runStrategyComparison(
    const std::vector<std::string>& topologies,
    int                             num_nodes,
    const TopoKwargs&               tk,
    double                          T,
    double                          attack_lambda,
    const std::vector<double>&      resource_levels,
    const std::string&              attack_strategy = "targeted",
    int                             n_sims          = 30,
    int                             repair_slots    = 1
);
