#include "csv_export.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace netsim {

namespace {

void ensureParentDir(const std::string& path) {
    const std::filesystem::path p(path);
    const auto parent = p.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
}

std::string escape(const std::string& text) {
    if (text.find_first_of(",\"\n\r") == std::string::npos) return text;
    std::string out = "\"";
    for (char c : text) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

std::ofstream openCsv(const std::string& path) {
    ensureParentDir(path);
    std::ofstream file(path);
    if (!file) std::cerr << "Ошибка открытия: " << path << "\n";
    else file << std::fixed << std::setprecision(6);
    return file;
}

void writeP(std::ostream& out, double value) {
    const auto flags = out.flags();
    const auto precision = out.precision();
    out << std::scientific << std::setprecision(3) << value;
    out.flags(flags);
    out.precision(precision);
}

}  // namespace

void exportTimeSeries(const SimResult& result, const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "time,lcc,phi_w,phi_active,e_phys,e_phys_norm,e_qos,e_qos_norm,"
            "qos_ratio,reachable_ratio,qos_reachable_ratio,"
            "iptd_mean_ms,iptd_p95_ms,iplr_mean,ipdv_mean_ms,"
            "avg_utilization,max_utilization,total_load_gbps,active_nodes,"
            "reachable_pairs,conforming_pairs\n";

    for (size_t i = 0; i < result.metrics.size(); ++i) {
        const auto& m = result.metrics[i];
        file << result.time_points[i] << ","
             << m.lcc << "," << m.phi_w << "," << m.phi_active << ","
             << m.e_phys << "," << m.e_phys_norm << ","
             << m.e_qos << "," << m.e_qos_norm << ","
             << m.qos_ratio << "," << m.reachable_ratio << ","
             << m.qos_reachable_ratio << ","
             << m.iptd_mean_ms << ","
             << m.iptd_p95_ms << "," << m.iplr_mean << ","
             << m.ipdv_mean_ms << "," << m.avg_utilization << ","
             << m.max_utilization << "," << m.total_load_gbps << ","
             << m.active_nodes << "," << m.reachable_pairs << ","
             << m.conforming_pairs << "\n";
    }
    std::cout << "Сохранено: " << path << "\n";
}

void exportTopologyComparison(const TopoCompResult& comparison,
                              const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "topology,lcc_mean,lcc_ci,phi_w_mean,phi_w_ci,"
            "eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,"
            "qos_ratio_mean,qos_ratio_ci,iptd_mean_ms,iptd_ci,"
            "iplr_mean,iplr_ci,ipdv_mean_ms,ipdv_ci,"
            "queue_mean,queue_ci,exhaustion_rate,baseline_qos_rate,"
            "wsurv_mean,wsurv_ci\n";

    for (const auto& [topo, mc] : comparison.results) {
        file << escape(topo) << ","
             << mc.lcc.mean << "," << mc.lcc.ci << ","
             << mc.phi_w.mean << "," << mc.phi_w.ci << ","
             << mc.e_phys.mean << "," << mc.e_phys.ci << ","
             << mc.e_qos.mean << "," << mc.e_qos.ci << ","
             << mc.qos_ratio.mean << "," << mc.qos_ratio.ci << ","
             << mc.iptd_ms.mean << "," << mc.iptd_ms.ci << ","
             << mc.iplr.mean << "," << mc.iplr.ci << ","
             << mc.ipdv_ms.mean << "," << mc.ipdv_ms.ci << ","
             << mc.queue_depth.mean << "," << mc.queue_depth.ci << ","
             << mc.exhaustion_rate << "," << mc.baseline_qos_rate << ","
             << mc.phi_w.mean << "," << mc.phi_w.ci << "\n";
    }
    std::cout << "Сохранено: " << path << "\n";
}

void exportTopologySignificance(const TopoCompResult& comparison,
                                const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "metric,topology_a,topology_b,U,p_raw,p_holm,significant,direction\n";
    auto write = [&](const std::string& metric, const SigMap& map) {
        for (const auto& [pair, sig] : map) {
            file << metric << "," << escape(pair.first) << ","
                 << escape(pair.second) << "," << sig.U << ",";
            writeP(file, sig.p_raw);
            file << ",";
            writeP(file, sig.p_holm);
            file << "," << (sig.significant ? 1 : 0) << ","
                 << escape(sig.direction) << "\n";
        }
    };
    write("e_qos_norm", comparison.significance_qos);
    write("lcc", comparison.significance_lcc);
    std::cout << "Сохранено: " << path << "\n";
}

void exportParameterSweep(
    const std::map<std::string, std::map<std::string, SweepResult>>& sweep_data,
    const std::map<std::string, std::vector<double>>& x_ranges,
    const std::string& out_dir) {
    std::filesystem::create_directories(out_dir);

    for (const auto& [param, topo_map] : sweep_data) {
        auto it = x_ranges.find(param);
        if (it == x_ranges.end()) continue;
        const auto& xs = it->second;

        const std::string path = out_dir + "/sweep_" + param + ".csv";
        auto file = openCsv(path);
        if (!file) continue;

        std::vector<std::string> topos;
        for (const auto& [topo, _] : topo_map) topos.push_back(topo);

        file << "x_value";
        for (const auto& topo : topos) {
            file << "," << topo << "_lcc_mean," << topo << "_lcc_ci"
                 << "," << topo << "_phi_mean," << topo << "_phi_ci"
                 << "," << topo << "_eff_mean," << topo << "_eff_ci"
                 << "," << topo << "_eff_qos_mean," << topo << "_eff_qos_ci"
                 << "," << topo << "_qos_mean," << topo << "_qos_ci"
                 << "," << topo << "_iptd_mean," << topo << "_iptd_ci"
                 << "," << topo << "_iplr_mean," << topo << "_iplr_ci"
                 << "," << topo << "_wsurv_mean," << topo << "_wsurv_ci";
        }
        file << "\n";

        for (size_t i = 0; i < xs.size(); ++i) {
            file << xs[i];
            for (const auto& topo : topos) {
                const SweepResult& sr = topo_map.at(topo);
                if (i >= sr.lcc_means.size()) {
                    file << ",,,,,,,,,,,,,,,,";
                    continue;
                }
                file << "," << sr.lcc_means[i] << "," << sr.lcc_ci[i]
                     << "," << sr.phi_means[i] << "," << sr.phi_ci[i]
                     << "," << sr.e_phys_means[i] << "," << sr.e_phys_ci[i]
                     << "," << sr.e_qos_means[i] << "," << sr.e_qos_ci[i]
                     << "," << sr.qos_means[i] << "," << sr.qos_ci[i]
                     << "," << sr.iptd_means[i] << "," << sr.iptd_ci[i]
                     << "," << sr.iplr_means[i] << "," << sr.iplr_ci[i]
                     << "," << sr.phi_means[i] << "," << sr.phi_ci[i];
            }
            file << "\n";
        }
        std::cout << "Сохранено: " << path << "\n";
    }
}

void exportStrategyComparison(const StrategyResults& results,
                              const std::vector<double>& resource_levels,
                              const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "topology,gatekeeper,R,lcc_mean,lcc_ci,phi_mean,phi_ci,"
            "eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,qos_mean,qos_ci,"
            "iptd_mean_ms,iptd_ci,iplr_mean,iplr_ci,sig_direction,p_raw,p_holm,"
            "sig_phi_direction,p_phi,exhaustion_rate\n";

    for (const auto& [topo, gk_map] : results) {
        for (const auto& [gk, r_map] : gk_map) {
            for (double r : resource_levels) {
                auto it = r_map.find(r);
                if (it == r_map.end()) continue;
                const auto& point = it->second;
                const auto& mc = point.mc;
                file << escape(topo) << "," << escape(gk) << "," << r << ","
                     << mc.lcc.mean << "," << mc.lcc.ci << ","
                     << mc.phi_w.mean << "," << mc.phi_w.ci << ","
                     << mc.e_phys.mean << "," << mc.e_phys.ci << ","
                     << mc.e_qos.mean << "," << mc.e_qos.ci << ","
                     << mc.qos_ratio.mean << "," << mc.qos_ratio.ci << ","
                     << mc.iptd_ms.mean << "," << mc.iptd_ms.ci << ","
                     << mc.iplr.mean << "," << mc.iplr.ci << ","
                     << escape(point.sig.direction) << ",";
                writeP(file, point.sig.p_raw);
                file << ",";
                writeP(file, point.sig.p_holm);
                file << "," << escape(point.sig_phi.direction) << ",";
                writeP(file, point.sig_phi.p_raw);
                file << "," << mc.exhaustion_rate << "\n";
            }
        }
    }
    std::cout << "Сохранено: " << path << "\n";
}

void exportAttackVectorAnalysis(
    const std::map<std::string, std::map<std::string, MCResult>>& results,
    const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "attack_strategy,topology,lcc_mean,lcc_ci,phi_mean,phi_ci,"
            "eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,qos_mean,qos_ci,"
            "iptd_mean_ms,iptd_ci,iplr_mean,iplr_ci\n";

    for (const auto& [strategy, topo_map] : results) {
        for (const auto& [topo, mc] : topo_map) {
            file << escape(strategy) << "," << escape(topo) << ","
                 << mc.lcc.mean << "," << mc.lcc.ci << ","
                 << mc.phi_w.mean << "," << mc.phi_w.ci << ","
                 << mc.e_phys.mean << "," << mc.e_phys.ci << ","
                 << mc.e_qos.mean << "," << mc.e_qos.ci << ","
                 << mc.qos_ratio.mean << "," << mc.qos_ratio.ci << ","
                 << mc.iptd_ms.mean << "," << mc.iptd_ms.ci << ","
                 << mc.iplr.mean << "," << mc.iplr.ci << "\n";
        }
    }
    std::cout << "Сохранено: " << path << "\n";
}

void exportRunMetadata(const SimConfig& config,
                       const SimResult& result,
                       const std::string& path) {
    auto file = openCsv(path);
    if (!file) return;

    file << "key,value\n";
    auto row = [&](const std::string& key, const std::string& value) {
        file << escape(key) << "," << escape(value) << "\n";
    };
    auto number = [&](const std::string& key, double value) {
        std::ostringstream oss;
        oss << std::setprecision(12) << value;
        row(key, oss.str());
    };

    row("scenario", config.scenario);
    row("topology", config.topology);
    row("num_nodes", std::to_string(config.num_nodes));
    row("n_sims", std::to_string(config.n_sims));
    row("T", std::to_string(config.T));
    row("attack_strategy", config.attack_strategy);
    row("attack_lambda", std::to_string(config.attack_lambda));
    row("gatekeeper", config.gatekeeper);
    row("repair_slots", std::to_string(config.repair_slots));
    row("global_resources", std::to_string(config.global_resources));
    row("qos_class", std::to_string(config.qos_class));
    row("qos_requirement", qosRequirementText(qosClassFromInt(config.qos_class)));
    row("seed", std::to_string(config.seed));
    row("demand_per_pair", std::to_string(config.demand_per_pair));
    row("packet_bits", std::to_string(config.packet_bits));
    row("buffer_packets", std::to_string(config.buffer_packets));

    number("e0_phys", result.e0_phys);
    number("e0_qos", result.e0_qos);
    row("baseline_qos_ok", result.baseline_qos_ok ? "1" : "0");
    number("first_failure_time", result.first_failure_time);
    row("had_failure", result.had_failure ? "1" : "0");
    row("attacks", std::to_string(result.attacks));
    row("attacks_successful", std::to_string(result.attacks_successful));
    row("recoveries_success", std::to_string(result.recoveries_success));
    row("recoveries_failed", std::to_string(result.recoveries_failed));
    number("resources_consumed", result.resources_consumed);
    row("resource_exhausted", result.global_resource_exhausted ? "1" : "0");
    number("mean_queue_depth", result.mean_queue_depth);
    number("mean_slots_used", result.mean_slots_used);
    number("max_queue_depth", result.max_queue_depth);

    std::cout << "Сохранено: " << path << "\n";
}

}  // namespace netsim
