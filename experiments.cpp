#include "experiments.h"

#include "qos.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace netsim {

namespace {

// t-критическое значение: таблица для малых df и нормальное приближение
// (поправка Корниша–Фишера) для больших.
double tCritical(int df, double alpha = 0.05) {
    (void)alpha;
    if (df <= 0) return std::numeric_limits<double>::quiet_NaN();

    static const double table[] = {
        0.0, 12.706, 4.303, 3.182, 2.776, 2.571,
        2.447, 2.365, 2.306, 2.262, 2.228,
        2.201, 2.179, 2.160, 2.145, 2.131,
        2.120, 2.110, 2.101, 2.093, 2.086,
        2.080, 2.074, 2.069, 2.064, 2.060,
        2.056, 2.052, 2.048, 2.045, 2.042};

    if (df <= 30) return table[df];

    const double z = 1.959964;
    return z + (z * z * z + z) / (4.0 * df) +
           (5.0 * z * z * z * z * z + 16.0 * z * z * z + 3.0 * z) /
               (96.0 * df * df);
}

Stat makeStat(const std::vector<double>& samples) {
    Stat out;
    const int n = static_cast<int>(samples.size());
    if (n == 0) return out;

    out.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / n;
    if (n < 2) return out;

    double sq = 0.0;
    for (double x : samples) sq += (x - out.mean) * (x - out.mean);
    out.std = std::sqrt(sq / (n - 1));
    out.se = out.std / std::sqrt(static_cast<double>(n));
    out.ci = tCritical(n - 1) * out.se;
    return out;
}

std::pair<double, double> mannWhitneyU(const std::vector<double>& a,
                                       const std::vector<double>& b) {
    const int n1 = static_cast<int>(a.size());
    const int n2 = static_cast<int>(b.size());
    double U1 = 0.0;
    for (double x : a)
        for (double y : b) U1 += (x > y) ? 1.0 : (x == y ? 0.5 : 0.0);

    const double U2 = static_cast<double>(n1) * n2 - U1;
    const double U = std::min(U1, U2);

    const double mean_U = static_cast<double>(n1) * n2 / 2.0;
    const double std_U =
        std::sqrt(static_cast<double>(n1) * n2 * (n1 + n2 + 1) / 12.0);
    if (std_U <= 0.0) return {U, 1.0};

    const double z = (U - mean_U) / std_U;
    const double p = std::erfc(std::abs(z) / std::sqrt(2.0));
    return {U, p};
}

template <typename Member>
double timeAverage(const SimResult& result, Member member) {
    if (result.metrics.size() < 2) {
        return result.metrics.empty() ? 0.0 : result.metrics.front().*member;
    }
    double area = 0.0;
    for (size_t i = 0; i + 1 < result.metrics.size(); ++i) {
        const double dt = result.time_points[i + 1] - result.time_points[i];
        area += 0.5 * ((result.metrics[i].*member) +
                       (result.metrics[i + 1].*member)) *
                dt;
    }
    const double horizon =
        result.time_points.back() - result.time_points.front();
    return horizon > 0.0 ? area / horizon : 0.0;
}

void report(const ProgressFn& progress, double fraction,
            const std::string& stage) {
    if (!progress) return;
    progress(std::max(0.0, std::min(1.0, fraction)), stage);
}

}  // namespace

std::pair<double, double> confidenceInterval(
    const std::vector<double>& samples, double alpha) {
    const int n = static_cast<int>(samples.size());
    if (n < 2)
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};

    const double mean =
        std::accumulate(samples.begin(), samples.end(), 0.0) / n;
    double sq = 0.0;
    for (double x : samples) sq += (x - mean) * (x - mean);
    const double std_dev = std::sqrt(sq / (n - 1));
    const double se = std_dev / std::sqrt(static_cast<double>(n));
    return {tCritical(n - 1, alpha) * se, se};
}

SigMap pairwiseSignificance(
    const std::map<std::string, std::vector<double>>& raw_samples,
    double alpha) {
    std::vector<std::string> labels;
    for (const auto& [key, _] : raw_samples) labels.push_back(key);

    std::vector<std::pair<std::string, std::string>> pairs;
    for (size_t i = 0; i < labels.size(); ++i)
        for (size_t j = i + 1; j < labels.size(); ++j)
            pairs.push_back({labels[i], labels[j]});

    const int n_tests = static_cast<int>(pairs.size());
    std::vector<double> raw_p(n_tests), U_vals(n_tests);

    for (int k = 0; k < n_tests; ++k) {
        auto [U, p] = mannWhitneyU(raw_samples.at(pairs[k].first),
                                   raw_samples.at(pairs[k].second));
        U_vals[k] = U;
        raw_p[k] = p;
    }

    std::vector<int> order(n_tests);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return raw_p[a] < raw_p[b]; });

    std::vector<double> p_holm(raw_p);
    for (int rank = 0; rank < n_tests; ++rank) {
        const int idx = order[rank];
        p_holm[idx] = std::min(raw_p[idx] * (n_tests - rank), 1.0);
    }
    for (int rank = 1; rank < n_tests; ++rank) {
        const int prev = order[rank - 1];
        const int curr = order[rank];
        p_holm[curr] = std::max(p_holm[curr], p_holm[prev]);
    }

    SigMap output;
    for (int k = 0; k < n_tests; ++k) {
        const auto& [a, b] = pairs[k];
        const bool sig = p_holm[k] < alpha;
        std::string dir = "~";
        if (sig) {
            const auto& sa = raw_samples.at(a);
            const auto& sb = raw_samples.at(b);
            const double ma =
                std::accumulate(sa.begin(), sa.end(), 0.0) / sa.size();
            const double mb =
                std::accumulate(sb.begin(), sb.end(), 0.0) / sb.size();
            dir = (ma > mb) ? (a + " > " + b) : (b + " > " + a);
        }
        output[{a, b}] = {U_vals[k], raw_p[k], p_holm[k], sig, dir};
    }
    return output;
}

MCResult runMonteCarlo(const SimConfig& base, int n_sims,
                       const ProgressFn& progress) {
    if (n_sims < 1) n_sims = 1;

    std::vector<double> lcc_v, phi_v, ephys_v, eqos_v, qos_v;
    std::vector<double> iptd_v, iplr_v, ipdv_v, queue_v, ffail_v, res_v;
    int exhausted = 0;
    int baseline_ok = 0;

    for (int i = 0; i < n_sims; ++i) {
        SimConfig cfg = base;
        cfg.seed = base.seed + static_cast<uint64_t>(i) * 1000003ULL;

        Graph graph = createNetworkTopology(cfg.topologyParams());
        const SimResult result = runSimulation(graph, cfg);

        lcc_v.push_back(timeAverage(result, &NetworkMetrics::lcc));
        phi_v.push_back(timeAverage(result, &NetworkMetrics::phi_w));
        ephys_v.push_back(timeAverage(result, &NetworkMetrics::e_phys_norm));
        eqos_v.push_back(timeAverage(result, &NetworkMetrics::e_qos_norm));
        qos_v.push_back(timeAverage(result, &NetworkMetrics::qos_ratio));
        iptd_v.push_back(timeAverage(result, &NetworkMetrics::iptd_mean_ms));
        iplr_v.push_back(timeAverage(result, &NetworkMetrics::iplr_mean));
        ipdv_v.push_back(timeAverage(result, &NetworkMetrics::ipdv_mean_ms));
        queue_v.push_back(result.mean_queue_depth);
        ffail_v.push_back(result.first_failure_time);
        res_v.push_back(result.resources_consumed);
        if (result.global_resource_exhausted) ++exhausted;
        if (result.baseline_qos_ok) ++baseline_ok;

        if (progress)
            progress(static_cast<double>(i + 1) / n_sims,
                     "симуляция " + std::to_string(i + 1) + "/" +
                         std::to_string(n_sims));
    }

    MCResult out;
    out.n_sims = n_sims;
    out.lcc = makeStat(lcc_v);
    out.phi_w = makeStat(phi_v);
    out.e_phys = makeStat(ephys_v);
    out.e_qos = makeStat(eqos_v);
    out.qos_ratio = makeStat(qos_v);
    out.iptd_ms = makeStat(iptd_v);
    out.iplr = makeStat(iplr_v);
    out.ipdv_ms = makeStat(ipdv_v);
    out.queue_depth = makeStat(queue_v);
    out.first_failure = makeStat(ffail_v);
    out.resources = makeStat(res_v);
    out.exhaustion_rate = static_cast<double>(exhausted) / n_sims;
    out.baseline_qos_rate = static_cast<double>(baseline_ok) / n_sims;
    out.lcc_samples = std::move(lcc_v);
    out.eqos_samples = std::move(eqos_v);
    out.qos_samples = std::move(qos_v);
    out.phi_samples = std::move(phi_v);
    return out;
}

TopoCompResult runTopologyComparison(const SimConfig& base,
                                     const std::vector<std::string>& topologies,
                                     int n_sims,
                                     const ProgressFn& progress) {
    TopoCompResult result;
    const int total = std::max(1, static_cast<int>(topologies.size()) * n_sims);
    int done = 0;

    std::map<std::string, std::vector<double>> qos_samples;
    std::map<std::string, std::vector<double>> lcc_samples;

    for (const auto& topo : topologies) {
        SimConfig cfg = base;
        cfg.topology = topo;
        cfg.topologies = {topo};

        const int offset = done;
        result.results[topo] = runMonteCarlo(
            cfg, n_sims, [&, offset, total](double f, const std::string& stage) {
                report(progress, (offset + f * n_sims) / total,
                       topo + ": " + stage);
            });
        done += n_sims;
        qos_samples[topo] = result.results[topo].eqos_samples;
        lcc_samples[topo] = result.results[topo].lcc_samples;
    }

    result.significance_qos = pairwiseSignificance(qos_samples);
    result.significance_lcc = pairwiseSignificance(lcc_samples);
    return result;
}

SweepResult runParameterSweep(const SimConfig& base,
                              const std::string& param_name,
                              const std::vector<double>& param_values,
                              const std::string& topology,
                              int n_sims,
                              const ProgressFn& progress) {
    SweepResult out;
    const int total = std::max(
        1, static_cast<int>(param_values.size()) * n_sims);

    const std::string param = [&]() {
        std::string lower;
        for (char c : param_name)
            lower.push_back(static_cast<char>(std::tolower(
                static_cast<unsigned char>(c))));
        return lower;
    }();

    for (size_t i = 0; i < param_values.size(); ++i) {
        SimConfig cfg = base;
        cfg.topology = topology;
        cfg.topologies = {topology};
        const double value = param_values[i];

        if (param == "lambda")
            cfg.attack_lambda = value;
        else if (param == "resources")
            cfg.global_resources = value;
        else if (param == "mu")
            cfg.base_mu = value;
        else if (param == "p_attack")
            cfg.base_P_attack = value;
        else if (param == "demand")
            cfg.demand_per_pair = value;
        else
            throw std::invalid_argument("Неизвестный параметр sweep: " +
                                        param_name);

        const int offset = static_cast<int>(i) * n_sims;
        const MCResult mc = runMonteCarlo(
            cfg, n_sims, [&, offset, total](double f, const std::string& stage) {
                report(progress, (offset + f * n_sims) / total,
                       topology + " " + param_name + "=" +
                           std::to_string(value) + ": " + stage);
            });

        out.lcc_means.push_back(mc.lcc.mean);
        out.lcc_ci.push_back(mc.lcc.ci);
        out.phi_means.push_back(mc.phi_w.mean);
        out.phi_ci.push_back(mc.phi_w.ci);
        out.e_phys_means.push_back(mc.e_phys.mean);
        out.e_phys_ci.push_back(mc.e_phys.ci);
        out.e_qos_means.push_back(mc.e_qos.mean);
        out.e_qos_ci.push_back(mc.e_qos.ci);
        out.qos_means.push_back(mc.qos_ratio.mean);
        out.qos_ci.push_back(mc.qos_ratio.ci);
        out.iptd_means.push_back(mc.iptd_ms.mean);
        out.iptd_ci.push_back(mc.iptd_ms.ci);
        out.iplr_means.push_back(mc.iplr.mean);
        out.iplr_ci.push_back(mc.iplr.ci);
    }
    return out;
}

StrategyResults runStrategyComparison(
    const SimConfig& base,
    const std::vector<std::string>& topologies,
    const std::vector<double>& resource_levels,
    int n_sims,
    const ProgressFn& progress) {
    StrategyResults output;
    const int total = std::max(
        1, static_cast<int>(topologies.size() * 2 * resource_levels.size() * n_sims));
    int done = 0;

    for (const auto& topo : topologies) {
        for (const std::string gk : {"fifo", "triage"}) {
            for (double r : resource_levels) {
                SimConfig cfg = base;
                cfg.topology = topo;
                cfg.topologies = {topo};
                cfg.gatekeeper = gk;
                cfg.global_resources = r;

                const int offset = done;
                MCResult mc = runMonteCarlo(
                    cfg, n_sims,
                    [&, offset, total](double f, const std::string& stage) {
                        report(progress, (offset + f * n_sims) / total,
                               topo + " " + std::string(gk) + " R=" +
                                   std::to_string(r) + ": " + stage);
                    });
                output[topo][gk][r].mc = std::move(mc);
                done += n_sims;
            }
        }

        for (double r : resource_levels) {
            auto compare = [](const std::vector<double>& fifo,
                              const std::vector<double>& triage) {
                auto [U, p] = mannWhitneyU(fifo, triage);
                const bool sig = p < 0.05;
                const double mf =
                    std::accumulate(fifo.begin(), fifo.end(), 0.0) / fifo.size();
                const double mt =
                    std::accumulate(triage.begin(), triage.end(), 0.0) /
                    triage.size();
                std::string dir = "~";
                if (sig && mt > mf) dir = "triage > fifo";
                else if (sig && mf > mt) dir = "fifo > triage";
                return SigResult{U, p, p, sig, dir};
            };

            const auto& fifo = output[topo]["fifo"][r].mc;
            const auto& triage = output[topo]["triage"][r].mc;
            const SigResult sig_qos =
                compare(fifo.eqos_samples, triage.eqos_samples);
            const SigResult sig_phi =
                compare(fifo.phi_samples, triage.phi_samples);

            output[topo]["fifo"][r].sig = sig_qos;
            output[topo]["triage"][r].sig = sig_qos;
            output[topo]["fifo"][r].sig_phi = sig_phi;
            output[topo]["triage"][r].sig_phi = sig_phi;
        }
    }
    return output;
}

std::map<std::string, std::map<std::string, MCResult>>
runAttackVectorAnalysis(const SimConfig& base,
                        const std::vector<std::string>& strategies,
                        const std::vector<std::string>& topologies,
                        int n_sims,
                        const ProgressFn& progress) {
    std::map<std::string, std::map<std::string, MCResult>> output;
    const int total = std::max(
        1, static_cast<int>(strategies.size() * topologies.size() * n_sims));
    int done = 0;

    for (const auto& strategy : strategies) {
        for (const auto& topo : topologies) {
            SimConfig cfg = base;
            cfg.topology = topo;
            cfg.topologies = {topo};
            cfg.attack_strategy = strategy;

            const int offset = done;
            output[strategy][topo] = runMonteCarlo(
                cfg, n_sims,
                [&, offset, total](double f, const std::string& stage) {
                    report(progress,
                           (offset + f * n_sims) / total,
                           strategy + " / " + topo + ": " + stage);
                });
            done += n_sims;
        }
    }
    return output;
}

}  // namespace netsim
