#pragma once
// ---------------------------------------------------------------------------
// csv_export.h — сериализация результатов симуляции в CSV.
// ---------------------------------------------------------------------------

#include "config.h"
#include "engine.h"
#include "experiments.h"

#include <map>
#include <string>
#include <vector>

namespace netsim {

void exportTimeSeries(const SimResult& result, const std::string& path);

void exportTopologyComparison(const TopoCompResult& comparison,
                              const std::string& path);

void exportTopologySignificance(const TopoCompResult& comparison,
                                const std::string& path);

void exportParameterSweep(
    const std::map<std::string, std::map<std::string, SweepResult>>& sweep_data,
    const std::map<std::string, std::vector<double>>& x_ranges,
    const std::string& out_dir);

void exportStrategyComparison(const StrategyResults& results,
                              const std::vector<double>& resource_levels,
                              const std::string& path);

void exportAttackVectorAnalysis(
    const std::map<std::string, std::map<std::string, MCResult>>& results,
    const std::string& path);

void exportRunMetadata(const SimConfig& config,
                       const SimResult& result,
                       const std::string& path);

}  // namespace netsim
