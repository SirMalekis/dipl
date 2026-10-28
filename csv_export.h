#pragma once
#include "experiments.h"
#include "engine.h"
#include <string>
#include <vector>
#include <map>

// Экспорт временных рядов одной симуляции
void exportTimeSeries(const SimResult& res,
                      const std::string& path);

// Экспорт сравнения топологий
void exportTopologyComparison(const TopoCompResult& comp,
                               const std::string& path);

// Экспорт параметрического sweep
// sweep_data: {param -> {topo -> SweepResult}}, x_ranges: {param -> vector<double>}
void exportParameterSweep(
    const std::map<std::string, std::map<std::string, SweepResult>>& sweep_data,
    const std::map<std::string, std::vector<double>>& x_ranges,
    const std::string& out_dir);

// Экспорт FIFO vs Triage
void exportStrategyComparison(const StrategyResults& res,
                               const std::vector<double>& resource_levels,
                               const std::string& path);

// Экспорт анализа векторов атак
void exportAttackVectorAnalysis(
    const std::map<std::string, std::map<std::string, MCResult>>& res_by_strategy,
    const std::string& path);
