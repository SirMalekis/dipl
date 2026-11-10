#pragma once
// ---------------------------------------------------------------------------
// experiments.h — постановка численных экспериментов и статистическая
// обработка результатов Монте-Карло.
// ---------------------------------------------------------------------------

#include "config.h"
#include "engine.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace netsim {

using ProgressFn = std::function<void(double fraction, const std::string& stage)>;

struct Stat {
    double mean = 0.0;
    double std = 0.0;
    double ci = 0.0;
    double se = 0.0;
};

struct MCResult {
    int n_sims = 0;

    Stat lcc;
    Stat phi_w;
    Stat e_phys;
    Stat e_qos;
    Stat qos_ratio;
    Stat iptd_ms;
    Stat iplr;
    Stat ipdv_ms;
    Stat queue_depth;
    Stat first_failure;
    Stat resources;

    double exhaustion_rate = 0.0;
    double baseline_qos_rate = 0.0;  // доля прогонов с E0_QoS > 0

    std::vector<double> lcc_samples;
    std::vector<double> eqos_samples;
    std::vector<double> qos_samples;
    std::vector<double> phi_samples;
};

struct SigResult {
    double U = 0.0;
    double p_raw = 1.0;
    double p_holm = 1.0;
    bool significant = false;
    std::string direction;
};

using SigMap = std::map<std::pair<std::string, std::string>, SigResult>;

struct TopoCompResult {
    std::map<std::string, MCResult> results;
    SigMap significance_qos;
    SigMap significance_lcc;
};

struct SweepResult {
    std::vector<double> lcc_means, lcc_ci;
    std::vector<double> phi_means, phi_ci;
    std::vector<double> e_phys_means, e_phys_ci;
    std::vector<double> e_qos_means, e_qos_ci;
    std::vector<double> qos_means, qos_ci;
    std::vector<double> iptd_means, iptd_ci;
    std::vector<double> iplr_means, iplr_ci;
};

struct StrategyPoint {
    MCResult mc;
    SigResult sig;      // сравнение по E_QoS
    SigResult sig_phi;  // сравнение по φ_w
};

using StrategyResults = std::map<std::string,
    std::map<std::string,
        std::map<double, StrategyPoint>>>;

// Статистические функции.
std::pair<double, double> confidenceInterval(const std::vector<double>& samples,
                                             double alpha = 0.05);
SigMap pairwiseSignificance(
    const std::map<std::string, std::vector<double>>& raw_samples,
    double alpha = 0.05);

// Один сценарий Монте-Карло.
MCResult runMonteCarlo(const SimConfig& base,
                       int n_sims,
                       const ProgressFn& progress = {});

// Сравнение топологий по всем метрикам.
TopoCompResult runTopologyComparison(const SimConfig& base,
                                     const std::vector<std::string>& topologies,
                                     int n_sims,
                                     const ProgressFn& progress = {});

// Однофакторный параметрический анализ для одной топологии.
SweepResult runParameterSweep(const SimConfig& base,
                              const std::string& param_name,
                              const std::vector<double>& param_values,
                              const std::string& topology,
                              int n_sims,
                              const ProgressFn& progress = {});

// Сравнение FIFO и Triage при заданных уровнях ресурсов.
StrategyResults runStrategyComparison(
    const SimConfig& base,
    const std::vector<std::string>& topologies,
    const std::vector<double>& resource_levels,
    int n_sims,
    const ProgressFn& progress = {});

// Сравнение векторов атак.
std::map<std::string, std::map<std::string, MCResult>>
runAttackVectorAnalysis(const SimConfig& base,
                        const std::vector<std::string>& strategies,
                        const std::vector<std::string>& topologies,
                        int n_sims,
                        const ProgressFn& progress = {});

}  // namespace netsim
