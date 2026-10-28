#include "experiments.h"
#include "engine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <chrono>

// ---------------------------------------------------------------------------
// t-критическое значение через аппроксимацию (для CI без boost/gsl)
// ---------------------------------------------------------------------------
static double tCritical(int df, double alpha = 0.05) {
    if (df <= 0) return std::numeric_limits<double>::quiet_NaN();

    static const double table[] = {
        0, 12.706, 4.303, 3.182, 2.776, 2.571,
        2.447, 2.365, 2.306, 2.262, 2.228,
        2.201, 2.179, 2.160, 2.145, 2.131,
        2.120, 2.110, 2.101, 2.093, 2.086,
        2.080, 2.074, 2.069, 2.064, 2.060,
        2.056, 2.052, 2.048, 2.045, 2.042
    };

    if (df <= 30) return table[df];

    double z = 1.959964;
    double t = z + (z * z * z + z) / (4.0 * df)
        + (5.0 * z * z * z * z * z + 16.0 * z * z * z + 3.0 * z) / (96.0 * df * df);
    return t;
}

// ---------------------------------------------------------------------------
// 95% доверительный интервал (t-распределение)
// ---------------------------------------------------------------------------
std::pair<double, double> confidenceInterval(
    const std::vector<double>& samples, double alpha)
{
    int n = (int)samples.size();
    if (n < 2) return { std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN() };

    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / n;
    double sq = 0.0;
    for (double x : samples) sq += (x - mean) * (x - mean);
    double std_dev = std::sqrt(sq / (n - 1));
    double se = std_dev / std::sqrt((double)n);
    double t_crit = tCritical(n - 1, alpha);
    return { t_crit * se, se };
}

// ---------------------------------------------------------------------------
// Mann-Whitney U-test
// ---------------------------------------------------------------------------
static std::pair<double, double> mannWhitneyU(
    const std::vector<double>& a, const std::vector<double>& b)
{
    int n1 = (int)a.size(), n2 = (int)b.size();
    double U1 = 0.0;
    for (double x : a)
        for (double y : b)
            U1 += (x > y) ? 1.0 : (x == y ? 0.5 : 0.0);

    double U2 = (double)n1 * n2 - U1;
    double U = std::min(U1, U2);

    double mean_U = (double)n1 * n2 / 2.0;
    double std_U = std::sqrt((double)n1 * n2 * (n1 + n2 + 1) / 12.0);
    if (std_U == 0) return { U, 1.0 };

    double z = (U - mean_U) / std_U;
    double p = std::erfc(std::abs(z) / std::sqrt(2.0));
    return { U, p };
}

// ---------------------------------------------------------------------------
// Попарные тесты + поправка Холма
// ---------------------------------------------------------------------------
SigMap pairwiseSignificance(
    const std::map<std::string, std::vector<double>>& raw_samples,
    double alpha)
{
    std::vector<std::string> labels;
    for (auto& [k, v] : raw_samples) labels.push_back(k);

    std::vector<std::pair<std::string, std::string>> pairs;
    for (int i = 0; i < (int)labels.size(); ++i)
        for (int j = i + 1; j < (int)labels.size(); ++j)
            pairs.push_back({ labels[i], labels[j] });

    int n_tests = (int)pairs.size();
    std::vector<double> raw_p(n_tests), U_vals(n_tests);

    for (int k = 0; k < n_tests; ++k) {
        auto [U, p] = mannWhitneyU(raw_samples.at(pairs[k].first),
            raw_samples.at(pairs[k].second));
        U_vals[k] = U;
        raw_p[k] = p;
    }

    // Поправка Холма
    std::vector<int> order(n_tests);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return raw_p[a] < raw_p[b]; });

    std::vector<double> p_holm(raw_p);
    for (int rank = 0; rank < n_tests; ++rank) {
        int idx = order[rank];
        p_holm[idx] = std::min(raw_p[idx] * (n_tests - rank), 1.0);
    }
    for (int rank = 1; rank < n_tests; ++rank) {
        int i_prev = order[rank - 1], i_curr = order[rank];
        p_holm[i_curr] = std::max(p_holm[i_curr], p_holm[i_prev]);
    }

    SigMap output;
    for (int k = 0; k < n_tests; ++k) {
        auto& [a, b] = pairs[k];
        bool sig = p_holm[k] < alpha;
        std::string dir;
        if (sig) {
            auto& sa = raw_samples.at(a);
            auto& sb = raw_samples.at(b);
            double ma = std::accumulate(sa.begin(), sa.end(), 0.0) / sa.size();
            double mb = std::accumulate(sb.begin(), sb.end(), 0.0) / sb.size();
            dir = (ma > mb) ? (a + " > " + b) : (b + " > " + a);
        }
        else {
            dir = "~";
        }
        output[{a, b}] = { U_vals[k], raw_p[k], p_holm[k], sig, dir };
    }
    return output;
}

// ---------------------------------------------------------------------------
// Базовый Monte Carlo прогон
// ---------------------------------------------------------------------------
MCResult runMonteCarlo(
    const std::string& topology,
    int                num_nodes,
    const TopoKwargs& tk,
    double             T,
    double             attack_lambda,
    double             global_resources,
    const std::string& attack_strategy,
    const std::string& gatekeeper,
    int                n_sims,
    int                repair_slots,
    double             ci_alpha)
{
    std::vector<double> lcc_v, eff_v, wsurv_v, ffail_v, res_v;
    lcc_v.reserve(n_sims); eff_v.reserve(n_sims);
    wsurv_v.reserve(n_sims); ffail_v.reserve(n_sims); res_v.reserve(n_sims);
    std::vector<double> exhausted_v;

    auto now_ms = []() -> uint64_t {
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        };

    for (int i = 0; i < n_sims; ++i) {
        uint64_t seed = (now_ms() % (1ULL << 31)) + (uint64_t)i * 7;

        Graph G = createNetworkTopology(
            topology, num_nodes,
            tk.base_P_attack, tk.base_P_recovery, tk.base_mu,
            tk.base_C_repair, tk.base_weight,
            tk.heterogeneous, {}, seed);

        SimResult res = runDetailedSimulation(
            G, T, attack_lambda, global_resources,
            attack_strategy, gatekeeper, repair_slots,
            200, seed);

        auto mean_vec = [](const std::vector<double>& v) {
            return v.empty() ? 0.0
                : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
            };

        lcc_v.push_back(mean_vec(res.lcc_history));
        eff_v.push_back(mean_vec(res.eff_norm_history));
        wsurv_v.push_back(mean_vec(res.weighted_surv_history));
        ffail_v.push_back(res.first_failure_time);
        res_v.push_back(res.resources_consumed);
        exhausted_v.push_back(res.global_resource_exhausted ? 1.0 : 0.0);
    }

    auto aggregate = [&](const std::vector<double>& v,
        double& mean_out, double& std_out,
        double& ci_out, double& se_out) {
            int n = (int)v.size();
            mean_out = std::accumulate(v.begin(), v.end(), 0.0) / n;
            double sq = 0;
            for (double x : v) sq += (x - mean_out) * (x - mean_out);
            std_out = std::sqrt(sq / (n - 1));
            auto [ci, se] = confidenceInterval(v, ci_alpha);
            ci_out = ci; se_out = se;
        };

    MCResult out;
    aggregate(lcc_v, out.lcc_mean, out.lcc_std, out.lcc_ci, out.lcc_se);
    aggregate(eff_v, out.eff_mean, out.eff_std, out.eff_ci, out.eff_se);
    aggregate(wsurv_v, out.wsurv_mean, out.wsurv_std, out.wsurv_ci, out.wsurv_se);
    aggregate(ffail_v, out.ffail_mean, out.ffail_std, out.ffail_ci, out.ffail_se);
    aggregate(res_v, out.res_mean, out.res_std, out.res_ci, out.res_se);

    out.exhaustion_rate = std::accumulate(exhausted_v.begin(), exhausted_v.end(), 0.0)
        / exhausted_v.size();
    out.lcc_samples = lcc_v;
    out.wsurv_samples = wsurv_v;
    return out;
}

// ---------------------------------------------------------------------------
// Сравнение топологий
// ---------------------------------------------------------------------------
TopoCompResult runTopologyComparison(
    const std::vector<std::string>& topologies,
    int num_nodes, const TopoKwargs& tk,
    double T, double attack_lambda, double global_resources,
    const std::string& attack_strategy, const std::string& gatekeeper,
    int n_sims, int repair_slots)
{
    TopoCompResult result;
    std::map<std::string, std::vector<double>> lcc_samples_map;

    for (const auto& topo : topologies) {
        std::cout << "   [" << topo << "] " << n_sims << " симуляций...\n" << std::flush;
        result.results[topo] = runMonteCarlo(
            topo, num_nodes, tk, T, attack_lambda, global_resources,
            attack_strategy, gatekeeper, n_sims, repair_slots);
        lcc_samples_map[topo] = result.results[topo].lcc_samples;
    }
    result.significance = pairwiseSignificance(lcc_samples_map);
    return result;
}

// ---------------------------------------------------------------------------
// Параметрический sweep
// ---------------------------------------------------------------------------
SweepResult runParameterSweep(
    const std::string& param_name,
    const std::vector<double>& param_values,
    const std::map<std::string, double>& fixed_params,
    const std::string& topology,
    int num_nodes,
    const std::string& attack_strategy,
    const std::string& gatekeeper,
    int n_sims, double T, int repair_slots)
{
    SweepResult out;
    out.lcc_means.reserve(param_values.size());
    out.lcc_ci.reserve(param_values.size());
    out.eff_means.reserve(param_values.size());
    out.eff_ci.reserve(param_values.size());
    out.wsurv_means.reserve(param_values.size());
    out.wsurv_ci.reserve(param_values.size());

    for (double val : param_values) {
        double sim_lambda = fixed_params.at("lambda");
        double sim_resources = fixed_params.at("resources");

        TopoKwargs tk;
        tk.base_P_attack = fixed_params.at("P_attack");
        tk.base_P_recovery = fixed_params.count("P_recovery")
            ? fixed_params.at("P_recovery") : 0.80;
        tk.base_mu = fixed_params.at("mu");
        tk.heterogeneous = fixed_params.count("heterogeneous")
            ? (fixed_params.at("heterogeneous") > 0.5) : false;

        if (param_name == "lambda")    sim_lambda = val;
        else if (param_name == "resources") sim_resources = val;
        else if (param_name == "mu")        tk.base_mu = val;
        else if (param_name == "P_attack")  tk.base_P_attack = val;
        else throw std::invalid_argument("Неизвестный параметр: " + param_name);

        MCResult r = runMonteCarlo(topology, num_nodes, tk,
            T, sim_lambda, sim_resources,
            attack_strategy, gatekeeper,
            n_sims, repair_slots);

        out.lcc_means.push_back(r.lcc_mean);
        out.lcc_ci.push_back(r.lcc_ci);
        out.eff_means.push_back(r.eff_mean);
        out.eff_ci.push_back(r.eff_ci);
        out.wsurv_means.push_back(r.wsurv_mean);
        out.wsurv_ci.push_back(r.wsurv_ci);
    }
    return out;
}

// ---------------------------------------------------------------------------
// FIFO vs Triage
// ---------------------------------------------------------------------------
StrategyResults runStrategyComparison(
    const std::vector<std::string>& topologies,
    int num_nodes, const TopoKwargs& tk,
    double T, double attack_lambda,
    const std::vector<double>& resource_levels,
    const std::string& attack_strategy,
    int n_sims, int repair_slots)
{
    StrategyResults output;

    for (const auto& topo : topologies) {
        for (const auto& gk : std::vector<std::string>{ "fifo", "triage" }) {
            for (double r : resource_levels) {
                std::cout << "   [" << topo << "] gk=" << gk
                    << " R=" << r << " → " << n_sims << " сим.\n" << std::flush;

                MCResult mc = runMonteCarlo(
                    topo, num_nodes, tk, T, attack_lambda, r,
                    attack_strategy, gk, n_sims, repair_slots);
                output[topo][gk][r].mc = mc;
            }
        }

        // Тесты значимости при каждом R
        for (double r : resource_levels) {
            const auto& sf = output[topo]["fifo"][r].mc.lcc_samples;
            const auto& st = output[topo]["triage"][r].mc.lcc_samples;

            auto [U, p] = mannWhitneyU(sf, st);
            bool sig = p < 0.05;
            double mf = std::accumulate(sf.begin(), sf.end(), 0.0) / sf.size();
            double mt = std::accumulate(st.begin(), st.end(), 0.0) / st.size();

            std::string dir;
            if (sig && mt > mf) dir = "triage > fifo";
            else if (sig && mf > mt) dir = "fifo > triage";
            else                     dir = "~";

            auto& sp = output[topo]["fifo"][r].sig;
            sp.U = U; sp.p = p; sp.significant = sig; sp.direction = dir;
            output[topo]["triage"][r].sig = sp;
        }
    }
    return output;
}