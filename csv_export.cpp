#include "csv_export.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>

static void ensureDir(const std::string& path) {
    std::filesystem::create_directories(path);
}

void exportTimeSeries(const SimResult& res, const std::string& path) {
    std::ofstream f(path);
    if (!f) { std::cerr << "Ошибка открытия: " << path << "\n"; return; }

    f << std::fixed << std::setprecision(6);
    f << "time,lcc,eff_norm,weighted_surv\n";
    for (int i = 0; i < (int)res.time_points.size(); ++i)
        f << res.time_points[i] << ","
        << res.lcc_history[i] << ","
        << res.eff_norm_history[i] << ","
        << res.weighted_surv_history[i] << "\n";

    std::cout << "Сохранено: " << path << "\n";
}

void exportTopologyComparison(const TopoCompResult& comp, const std::string& path) {
    std::ofstream f(path);
    if (!f) { std::cerr << "Ошибка открытия: " << path << "\n"; return; }

    f << std::fixed << std::setprecision(6);
    f << "topology,lcc_mean,lcc_ci,eff_mean,eff_ci,wsurv_mean,wsurv_ci\n";
    for (auto& [topo, mc] : comp.results)
        f << topo << ","
        << mc.lcc_mean << "," << mc.lcc_ci << ","
        << mc.eff_mean << "," << mc.eff_ci << ","
        << mc.wsurv_mean << "," << mc.wsurv_ci << "\n";

    std::cout << "Сохранено: " << path << "\n";
}

void exportParameterSweep(
    const std::map<std::string, std::map<std::string, SweepResult>>& sweep_data,
    const std::map<std::string, std::vector<double>>& x_ranges,
    const std::string& out_dir)
{
    ensureDir(out_dir);

    for (auto& [param, topo_map] : sweep_data) {
        std::string path = out_dir + "/sweep_" + param + ".csv";
        std::ofstream f(path);
        if (!f) { std::cerr << "Ошибка открытия: " << path << "\n"; continue; }

        f << std::fixed << std::setprecision(6);

        // Заголовок
        f << "x_value";
        for (auto& [topo, _] : topo_map)
            f << "," << topo << "_lcc_mean," << topo << "_lcc_ci"
            << "," << topo << "_eff_mean," << topo << "_eff_ci"
            << "," << topo << "_wsurv_mean," << topo << "_wsurv_ci";  // ✓ ДОБАВЛЕНО
        f << "\n";

        const auto& xs = x_ranges.at(param);
        for (int i = 0; i < (int)xs.size(); ++i) {
            f << xs[i];
            for (auto& [topo, sr] : topo_map) {
                f << "," << sr.lcc_means[i] << "," << sr.lcc_ci[i]
                    << "," << sr.eff_means[i] << "," << sr.eff_ci[i]
                    << "," << sr.wsurv_means[i] << "," << sr.wsurv_ci[i];  // ✓ ДОБАВЛЕНО
            }
            f << "\n";
        }
        std::cout << "Сохранено: " << path << "\n";
    }
}

void exportStrategyComparison(const StrategyResults& res,
    const std::vector<double>& resource_levels,
    const std::string& path) {
    std::ofstream f(path);
    if (!f) { std::cerr << "Ошибка открытия: " << path << "\n"; return; }

    f << std::fixed << std::setprecision(6);
    // ✓ ДОБАВЛЕНО: eff_mean и eff_ci
    f << "topology,gatekeeper,R,lcc_mean,lcc_ci,eff_mean,eff_ci,wsurv_mean,wsurv_ci,sig_direction\n";

    for (auto& [topo, gk_map] : res) {
        for (auto& [gk, r_map] : gk_map) {
            for (double r : resource_levels) {
                if (!r_map.count(r)) continue;
                const auto& sp = r_map.at(r);
                f << topo << "," << gk << "," << r << ","
                    << sp.mc.lcc_mean << "," << sp.mc.lcc_ci << ","
                    << sp.mc.eff_mean << "," << sp.mc.eff_ci << ","  // ← ДОБАВЛЕНО
                    << sp.mc.wsurv_mean << "," << sp.mc.wsurv_ci << ","
                    << sp.sig.direction << "\n";
            }
        }
    }
    std::cout << "Сохранено: " << path << "\n";
}

void exportAttackVectorAnalysis(
    const std::map<std::string, std::map<std::string, MCResult>>& res_by_strategy,
    const std::string& path)
{
    std::ofstream f(path);
    if (!f) { std::cerr << "Ошибка открытия: " << path << "\n"; return; }

    f << std::fixed << std::setprecision(6);
    // ✓ ДОБАВЛЕНО: eff_mean и eff_ci
    f << "attack_strategy,topology,lcc_mean,lcc_ci,eff_mean,eff_ci,wsurv_mean,wsurv_ci\n";

    for (auto& [strat, topo_map] : res_by_strategy)
        for (auto& [topo, mc] : topo_map)
            f << strat << "," << topo << ","
            << mc.lcc_mean << "," << mc.lcc_ci << ","
            << mc.eff_mean << "," << mc.eff_ci << ","   // ← ДОБАВЛЕНО
            << mc.wsurv_mean << "," << mc.wsurv_ci << "\n";

    std::cout << "Сохранено: " << path << "\n";
}
