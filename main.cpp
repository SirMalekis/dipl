/**
main.cpp — Интерактивное меню управления симуляцией.
Запуск:
./netsim          — интерактивное меню
./netsim --quick  — повтор последних настроек
*/
#include "topology.h"
#include "engine.h"
#include "experiments.h"
#include "csv_export.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#define ANSI_RESET    "\033[0m"
#define ANSI_BOLD     "\033[1m"
#define ANSI_GREEN    "\033[92m"
#define ANSI_YELLOW   "\033[93m"
#define ANSI_RED      "\033[91m"
#define ANSI_DIM      "\033[90m"
#define ANSI_CYAN     "\033[96m"
#else
#define ANSI_RESET    "\033[0m"
#define ANSI_BOLD     "\033[1m"
#define ANSI_GREEN    "\033[92m"
#define ANSI_YELLOW   "\033[93m"
#define ANSI_RED      "\033[91m"
#define ANSI_DIM      "\033[90m"
#define ANSI_CYAN     "\033[96m"
#endif

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Форматирование
// ────────────────────────────────────────────────────────────────────────────
static void hr(char c = '-', int w = 68) {
    for (int i = 0; i < w; ++i) std::cout << c;
    std::cout << '\n';
}

static void header(const std::string& title) {
    hr('=');
    int title_len = (int)title.size();
    int pad = 68 - title_len - 2;
    if (pad < 0) pad = 0;
    pad /= 2;
    std::cout << "   " << std::string(pad, ' ')
        << ANSI_BOLD << title << ANSI_RESET << '\n';
    hr('=');
}

static void section(const std::string& title) {
    std::cout << "\n  ┌─ " << title << '\n';
}

static void hint(const std::string& msg) {
    std::cout << "  │   " << ANSI_DIM << msg << ANSI_RESET << '\n';
}

// ─────────────────────────────────────────────────────────────────────────────
// Функции ввода с валидацией
// ─────────────────────────────────────────────────────────────────────────────
static int askInt(const std::string& prompt, int lo, int hi, int def) {
    hint("Тип: int  |  Диапазон: [" + std::to_string(lo) + "…" + std::to_string(hi) +
        "]  |  По умолчанию: " + std::to_string(def));
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято: " << def << ANSI_RESET << '\n';
            return def;
        }
        try {
            int val = std::stoi(raw);
            if (val >= lo && val <= hi) {
                std::cout << "  │   " << ANSI_GREEN << "Принято: " << val << ANSI_RESET << '\n';
                return val;
            }
            std::cout << "  │   " << ANSI_RED << "Ошибка: введите от " << lo << " до " << hi << ANSI_RESET << '\n';
        }
        catch (...) {
            std::cout << "  │   " << ANSI_RED << "Ошибка: введите целое число." << ANSI_RESET << '\n';
        }
    }
}

static double askFloat(const std::string& prompt, double lo, double hi, double def) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "Тип: float  |  Диапазон: [" << lo << "…" << hi << "]  |  По умолчанию: " << def;
    hint(oss.str());
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято: " << def << ANSI_RESET << '\n';
            return def;
        }
        std::replace(raw.begin(), raw.end(), ',', '.');
        try {
            double val = std::stod(raw);
            if (val >= lo && val <= hi) {
                std::cout << "  │   " << ANSI_GREEN << "Принято: " << val << ANSI_RESET << '\n';
                return val;
            }
            std::cout << "  │   " << ANSI_RED << "Ошибка: значение от " << lo << " до " << hi << ANSI_RESET << '\n';
        }
        catch (...) {
            std::cout << "  │   " << ANSI_RED << "Ошибка: введите число." << ANSI_RESET << '\n';
        }
    }
}

static std::string askChoice(const std::string& prompt,
    const std::vector<std::string>& options,
    const std::string& def,
    const std::map<std::string, std::string>& descs = {}) {
    std::ostringstream oss;
    for (int i = 0; i < (int)options.size(); ++i) oss << (i ? "|" : "") << options[i];
    hint("Варианты: " + oss.str() + "  |  По умолчанию: " + def);
    for (auto& o : options) {
        std::string mark = (o == def) ? "  ← " : " ";
        std::cout << "  │     " << ANSI_CYAN << std::left << std::setw(15) << o << ANSI_RESET;
        if (descs.count(o)) std::cout << descs.at(o);
        std::cout << mark << '\n';
    }
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято: " << def << ANSI_RESET << '\n';
            return def;
        }
        for (auto& o : options)
            if (raw == o) {
                std::cout << "  │   " << ANSI_GREEN << "Принято: " << raw << ANSI_RESET << '\n';
                return raw;
            }
        std::cout << "  │   " << ANSI_RED << "Выберите одно из приведённых вариантов." << ANSI_RESET << '\n';
    }
}

static std::vector<std::string> askMultiChoice(const std::string& prompt,
    const std::vector<std::string>& options,
    const std::vector<std::string>& def) {
    std::ostringstream d;
    for (int i = 0; i < (int)def.size(); ++i) d << (i ? ", " : "") << def[i];
    hint("Введите через запятую. Варианты: " + [&]() {
        std::string s; for (auto& o : options) s += o + ", "; return s;
        }());
    hint("По умолчанию: " + d.str());
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято: " << d.str() << ANSI_RESET << '\n';
            return def;
        }
        std::vector<std::string> chosen;
        std::istringstream ss(raw);
        std::string token;
        bool ok = true;
        while (std::getline(ss, token, ',')) {
            while (!token.empty() && token.front() == ' ') token.erase(token.begin());
            while (!token.empty() && token.back() == ' ')  token.pop_back();
            if (std::find(options.begin(), options.end(), token) == options.end()) {
                std::cout << "  │   " << ANSI_RED << "Неизвестный вариант: " << token << ANSI_RESET << '\n';
                ok = false; break;
            }
            chosen.push_back(token);
        }
        if (!ok || chosen.empty()) continue;
        std::cout << "  │   " << ANSI_GREEN << "Принято." << ANSI_RESET << '\n';
        return chosen;
    }
}

static bool askBool(const std::string& prompt, bool def) {
    hint(std::string("Тип: bool  |  Варианты: да/нет  |  По умолчанию: ") + (def ? "да" : "нет"));
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято: " << (def ? "да" : "нет") << ANSI_RESET << '\n';
            return def;
        }
        std::transform(raw.begin(), raw.end(), raw.begin(), ::tolower);
        if (raw == "да" || raw == "yes" || raw == "y" || raw == "1" || raw == "true") {
            std::cout << "  │   " << ANSI_GREEN << "Принято: да" << ANSI_RESET << '\n';
            return true;
        }
        if (raw == "нет" || raw == "no" || raw == "n" || raw == "0" || raw == "false") {
            std::cout << "  │   " << ANSI_GREEN << "Принято: нет" << ANSI_RESET << '\n';
            return false;
        }
        std::cout << "  │   " << ANSI_RED << "Введите: да / нет" << ANSI_RESET << '\n';
    }
}

static std::vector<double> askLinspace(const std::string& prompt,
    double lo, double hi,
    double def_lo, double def_hi, int def_n) {
    std::ostringstream oss;
    oss << "Формат: MIN MAX [ТОЧЕК]  (например: " << def_lo << " " << def_hi << " " << def_n << ")";
    hint(oss.str());
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::vector<double> r;
            for (int i = 0; i < def_n; ++i)
                r.push_back(def_lo + (def_hi - def_lo) * i / (def_n - 1));
            std::cout << "  │   " << ANSI_GREEN << "Принято: linspace(" << def_lo << ", " << def_hi << ", " << def_n << ")" << ANSI_RESET << '\n';
            return r;
        }
        std::replace(raw.begin(), raw.end(), ',', '.');
        std::istringstream ss(raw);
        double v_min, v_max; int n_pts = def_n;
        if (!(ss >> v_min >> v_max)) {
            std::cout << "  │   " << ANSI_RED << "Формат: MIN MAX  или  MIN MAX ТОЧЕК" << ANSI_RESET << '\n';
            continue;
        }
        ss >> n_pts;
        if (v_min >= v_max || v_min < lo || v_max > hi) {
            std::cout << "  │   " << ANSI_RED << "MIN < MAX, оба в [" << lo << ", " << hi << "]" << ANSI_RESET << '\n';
            continue;
        }
        if (n_pts < 2 || n_pts > 50) {
            std::cout << "  │   " << ANSI_RED << "Точек: 2–50" << ANSI_RESET << '\n';
            continue;
        }
        std::vector<double> r;
        for (int i = 0; i < n_pts; ++i)
            r.push_back(v_min + (v_max - v_min) * i / (n_pts - 1));
        std::cout << "  │   " << ANSI_GREEN << "Принято." << ANSI_RESET << '\n';
        return r;
    }
}

static std::vector<double> askFloatList(const std::string& prompt,
    double lo, double hi,
    std::vector<double> def) {
    std::ostringstream d;
    for (auto v : def) d << v << " ";
    hint("Введите числа через пробел. Диапазон: [" + std::to_string(lo) + "…" + std::to_string(hi) + "]");
    hint("По умолчанию: " + d.str());
    while (true) {
        std::cout << "  │   " << ANSI_BOLD << prompt << ANSI_RESET << '\n' << "  │   > ";
        std::string raw; std::getline(std::cin, raw);
        if (raw.empty()) {
            std::cout << "  │   " << ANSI_GREEN << "Принято." << ANSI_RESET << '\n';
            return def;
        }
        std::istringstream ss(raw);
        std::vector<double> vals;
        double v;
        bool bad = false;
        while (ss >> v) {
            if (v < lo || v > hi) { bad = true; break; }
            vals.push_back(v);
        }
        std::sort(vals.begin(), vals.end());
        vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
        if (bad || vals.size() < 2 || vals.size() > 20) {
            std::cout << "  │   " << ANSI_RED << "Минимум 2, максимум 20 значений в диапазоне." << ANSI_RESET << '\n';
            continue;
        }
        std::cout << "  │   " << ANSI_GREEN << "Принято." << ANSI_RESET << '\n';
        return vals;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Структура конфигурации
// ─────────────────────────────────────────────────────────────────────────────
struct Config {
    int         num_nodes = 50;
    int         n_sims = 30;
    double      T = 150.0;
    double      attack_lambda = 1.0;
    double      global_resources = 200.0;
    std::string attack_strategy = "targeted";
    std::string gatekeeper = "fifo";
    int         repair_slots = 1;
    bool        heterogeneous = false;
    double      base_P_attack = 0.60;
    double      base_P_recovery = 0.80;
    double      base_mu = 0.20;
    std::string out_dir = "results";
    int         sample_points = 200;
    std::vector<std::string> topologies = { "star", "ring", "full_mesh", "erdos_renyi", "barabasi_albert" };
    std::vector<std::string> attack_strategies = { "targeted", "random", "cascading" };
    std::vector<double>      resource_levels = { 50, 100, 200, 500 };
    std::map<std::string, std::vector<double>> sweep_ranges;
    std::vector<std::string> sweep_params;
};

static TopoKwargs cfgToTK(const Config& c) {
    TopoKwargs tk;
    tk.base_P_attack = c.base_P_attack;
    tk.base_P_recovery = c.base_P_recovery;
    tk.base_mu = c.base_mu;
    tk.heterogeneous = c.heterogeneous;
    return tk;
}

// ─────────────────────────────────────────────────────────────────────────────
// Базовые общие параметры
// ─────────────────────────────────────────────────────────────────────────────
static void setupCommon(Config& c) {
    section("Базовые параметры");
    c.num_nodes = askInt("Количество узлов N", 5, 200, c.num_nodes);
    c.n_sims = askInt("Симуляций Монте-Карло (n_sims)", 1, 500, c.n_sims);
    c.T = askFloat("Горизонт моделирования T", 10.0, 1000.0, c.T);
    c.attack_lambda = askFloat("Интенсивность атак λ (атак/ед.вр.)", 0.01, 20.0, c.attack_lambda);
    c.global_resources = askFloat("Глобальный ресурс R_total", 0.0, 10000.0, c.global_resources);
    c.repair_slots = askInt("Слотов ремонта за шаг (repair_slots)", 1, 20, c.repair_slots);
    section("Параметры узлов");
    c.heterogeneous = askBool("Гетерогенная сеть? (server/switch/host)", c.heterogeneous);
    if (!c.heterogeneous) {
        c.base_P_attack = askFloat("P_attack (вер-ть успеха атаки)", 0.01, 0.99, c.base_P_attack);
        c.base_P_recovery = askFloat("P_recovery (вер-ть успешного ремонта)", 0.01, 0.99, c.base_P_recovery);
        c.base_mu = askFloat("μ (интенсивность восстановления)", 0.01, 1.0, c.base_mu);
    }
    c.out_dir = "results";
}

// ─────────────────────────────────────────────────────────────────────────────
// Отображение сводки
// ─────────────────────────────────────────────────────────────────────────────
static void printSummary(const std::string& scenario, const Config& c) {
    hr();
    std::cout << "  Сценарий: " << ANSI_BOLD << scenario << ANSI_RESET << '\n';
    std::cout << "  N=" << c.num_nodes << "  n_sims=" << c.n_sims << "  T=" << c.T
        << "  λ=" << c.attack_lambda << "  R=" << c.global_resources << '\n';
    hr();
}

// ─────────────────────────────────────────────────────────────────────────────
// Консольные отчёты
// ─────────────────────────────────────────────────────────────────────────────
static void printTopoReport(const TopoCompResult& comp) {
    hr();
    std::cout << ANSI_BOLD << "  Результаты сравнения топологий:\n" << ANSI_RESET;
    std::cout << "   " << std::left << std::setw(12) << "Топология"
        << std::setw(14) << "LCC (±CI)"
        << std::setw(14) << "E_norm (±CI)"
        << std::setw(14) << "φ_w (±CI)" << '\n';
    hr('-', 56);
    for (auto& [topo, mc] : comp.results) {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "   " << std::left << std::setw(12) << topo
            << std::setw(14) << (std::to_string(mc.lcc_mean).substr(0, 5) + "±" + std::to_string(mc.lcc_ci).substr(0, 5))
            << std::setw(14) << (std::to_string(mc.eff_mean).substr(0, 5) + "±" + std::to_string(mc.eff_ci).substr(0, 5))
            << std::setw(14) << (std::to_string(mc.wsurv_mean).substr(0, 5) + "±" + std::to_string(mc.wsurv_ci).substr(0, 5))
            << '\n';
    }
    hr();
}

// ── НОВОЕ: Вывод статистики временных рядов ──────────────────────────────────
static void printTimeSeriesStats(const std::string& topo, const SimResult& res) {
    hr();
    std::cout << ANSI_BOLD << "  Временные ряды — " << topo << ANSI_RESET << '\n';
    hr('-', 60);

    auto mean_vec = [](const std::vector<double>& v) {
        return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        };

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "    LCC (среднее)            : " << mean_vec(res.lcc_history) << '\n';
    std::cout << "    E_norm (среднее)         : " << mean_vec(res.eff_norm_history) << '\n';
    std::cout << "    Взвеш. живучесть (сред.) : " << mean_vec(res.weighted_surv_history) << '\n';
    std::cout << "    Первый отказ (время)     : " << res.first_failure_time << '\n';
    std::cout << "    Потреблено ресурсов      : " << res.resources_consumed << '\n';
    std::cout << "    Истощение ресурсов       : " << (res.global_resource_exhausted ? "ДА" : "НЕТ") << '\n';
    std::cout << "    Успешных восстановлений  : " << res.recoveries_success << '\n';
    std::cout << "    Неудачных попыток        : " << res.recoveries_failed << '\n';
    hr();
}

// ─────────────────────────────────────────────────────────────────────────────
// Сценарии
// ─────────────────────────────────────────────────────────────────────────────
static void runTopology(const Config& c) {
    fs::create_directories(c.out_dir);
    TopoKwargs tk = cfgToTK(c);
    auto comp = runTopologyComparison(
        c.topologies, c.num_nodes, tk, c.T,
        c.attack_lambda, c.global_resources,
        c.attack_strategy, c.gatekeeper, c.n_sims, c.repair_slots);
    printTopoReport(comp);
    exportTopologyComparison(comp, c.out_dir + "/topology_comparison.csv");
}

static void runSweep(Config& c) {
    fs::create_directories(c.out_dir);  // ← ИСПРАВЛЕНО: было std::system("mkdir -p ...")
    if (c.sweep_ranges.empty()) {
        c.sweep_ranges["lambda"] = askLinspace("λ: MIN MAX [ТОЧЕК]", 0.1, 10.0, 0.5, 5.0, 8);
        c.sweep_ranges["resources"] = askLinspace("R: MIN MAX [ТОЧЕК]", 10, 2000, 50, 1000, 8);
        c.sweep_ranges["mu"] = askLinspace("μ: MIN MAX [ТОЧЕК]", 0.05, 0.8, 0.05, 0.8, 8);
        c.sweep_ranges["P_attack"] = askLinspace("P_attack: MIN MAX [ТОЧЕК]", 0.1, 0.95, 0.3, 0.95, 8);
        c.sweep_params = { "lambda", "resources", "mu", "P_attack" };
    }
    std::map<std::string, double> fixed;
    fixed["lambda"] = c.attack_lambda;
    fixed["resources"] = c.global_resources;
    fixed["P_attack"] = c.base_P_attack;
    fixed["P_recovery"] = c.base_P_recovery;
    fixed["mu"] = c.base_mu;
    fixed["heterogeneous"] = c.heterogeneous ? 1.0 : 0.0;

    std::map<std::string, std::map<std::string, SweepResult>> sweep_data;
    for (const auto& param : c.sweep_params) {
        for (const auto& topo : c.topologies) {
            std::cout << "   sweep: " << param << " / " << topo << '\n' << std::flush;
            sweep_data[param][topo] = runParameterSweep(
                param, c.sweep_ranges[param], fixed,
                topo, c.num_nodes,
                c.attack_strategy, c.gatekeeper,
                c.n_sims, c.T, c.repair_slots);
        }

        hr();
        std::cout << ANSI_BOLD << "  Sweep: " << param << " (LCC)" << ANSI_RESET << '\n';
        std::vector<std::string> ordered_topos = { "star", "ring", "full_mesh" };
        std::cout << "   " << std::left << std::setw(10) << "X_value"
            << std::setw(15) << "STAR"
            << std::setw(15) << "RING"
            << std::setw(15) << "FULL_MESH" << '\n';
        hr('-', 58);
        const auto& xs = c.sweep_ranges[param];
        for (int i = 0; i < (int)xs.size(); ++i) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "   " << std::left << std::setw(10) << xs[i];
            for (const auto& topo : ordered_topos) {
                if (sweep_data[param].count(topo)) {
                    double lcc = sweep_data[param][topo].lcc_means[i];
                    std::cout << std::setw(15) << std::setprecision(3) << lcc;
                }
            }
            std::cout << '\n';
        }
        hr();

        std::cout << "\n" << ANSI_BOLD << "  Sweep: " << param << " (φ_w)" << ANSI_RESET << '\n';
        std::cout << "   " << std::left << std::setw(10) << "X_value"
            << std::setw(15) << "STAR"
            << std::setw(15) << "RING"
            << std::setw(15) << "FULL_MESH" << '\n';
        hr('-', 58);
        for (int i = 0; i < (int)xs.size(); ++i) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "   " << std::left << std::setw(10) << xs[i];
            for (const auto& topo : ordered_topos) {
                if (sweep_data[param].count(topo)) {
                    double wsurv = sweep_data[param][topo].wsurv_means[i];
                    std::cout << std::setw(15) << std::setprecision(3) << wsurv;
                }
            }
            std::cout << '\n';
        }
        hr();
        
        std::cout << "\n" << ANSI_BOLD << "  Sweep: " << param << " (E_norm)" << ANSI_RESET << '\n';
        std::cout << "   " << std::left << std::setw(10) << "X_value"
            << std::setw(15) << "STAR"
            << std::setw(15) << "RING"
            << std::setw(15) << "FULL_MESH" << '\n';
        hr('-', 58);

        for (int i = 0; i < (int)xs.size(); ++i) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "   " << std::left << std::setw(10) << xs[i];
            for (const auto& topo : ordered_topos) {
                if (sweep_data[param].count(topo)) {
                    double eff = sweep_data[param][topo].eff_means[i];
                    std::cout << std::setw(15) << std::setprecision(3) << eff;
                }
            }
            std::cout << '\n';
        }
        hr();
        std::cout << "\n";
    }
    exportParameterSweep(sweep_data, c.sweep_ranges, c.out_dir);
}

static void runStrategy(Config c) {
    fs::create_directories(c.out_dir);
    c.heterogeneous = true;
    TopoKwargs tk = cfgToTK(c);
    auto res = runStrategyComparison(
        c.topologies, c.num_nodes, tk, c.T,
        c.attack_lambda, c.resource_levels,
        c.attack_strategy, c.n_sims, c.repair_slots);
    exportStrategyComparison(res, c.resource_levels,
        c.out_dir + "/strategy_comparison.csv");
    hr();
    std::cout << ANSI_BOLD << "  FIFO vs Triage — результаты:\n" << ANSI_RESET;

    for (auto& [topo, gk_map] : res) {
        std::cout << "\n   [" << topo << "]\n";
        std::cout << "   " << std::left << std::setw(8) << "R"
            << std::setw(12) << "LCC(F)" << std::setw(12) << "LCC(T)"
            << std::setw(12) << "Enorm(F)" << std::setw(12) << "Enorm(T)"
            << std::setw(12) << "φw(F)" << std::setw(12) << "φw(T)"
            << std::setw(10) << "Знач." << '\n';
        hr('-', 88);

        for (double r : c.resource_levels) {
            auto& fifo_pt = gk_map.at("fifo").at(r);
            auto& triage_pt = gk_map.at("triage").at(r);
            auto& sig = fifo_pt.sig;

            std::cout << std::fixed << std::setprecision(3);
            std::cout << "   " << std::setw(8) << r
                << std::setw(12) << fifo_pt.mc.lcc_mean
                << std::setw(12) << triage_pt.mc.lcc_mean
                << std::setw(12) << fifo_pt.mc.eff_mean
                << std::setw(12) << triage_pt.mc.eff_mean
                << std::setw(12) << fifo_pt.mc.wsurv_mean
                << std::setw(12) << triage_pt.mc.wsurv_mean
                << std::setw(10) << (sig.significant ? ANSI_GREEN : ANSI_DIM)
                << sig.direction << ANSI_RESET << '\n';
        }
    }
    hr();
}

// ── ИСПРАВЛЕНО: добавлен вывод таблицы результатов ───────────────────────────
static void runAttack(const Config& c) {
    fs::create_directories(c.out_dir);
    TopoKwargs tk = cfgToTK(c);
    std::map<std::string, std::map<std::string, MCResult>> res_by_strategy;

    for (const auto& atk : c.attack_strategies) {
        std::cout << "\n  Стратегия атаки: " << ANSI_BOLD << atk << ANSI_RESET << '\n';
        auto comp = runTopologyComparison(
            c.topologies, c.num_nodes, tk, c.T,
            c.attack_lambda, c.global_resources,
            atk, c.gatekeeper, c.n_sims, c.repair_slots);

        printTopoReport(comp);  // ← вывод таблицы

        for (auto& [topo, mc] : comp.results)
            res_by_strategy[atk][topo] = mc;
    }
    exportAttackVectorAnalysis(res_by_strategy, c.out_dir + "/attack_vector_analysis.csv");
}

// ── ИСПРАВЛЕНО: добавлен вывод статистики временных рядов ────────────────────
static void runTimeSeries(const Config& c) {
    fs::create_directories(c.out_dir);
    TopoKwargs tk = cfgToTK(c);
    for (const auto& topo : c.topologies) {
        Graph G = createNetworkTopology(
            topo, c.num_nodes,
            tk.base_P_attack, tk.base_P_recovery, tk.base_mu,
            1, 1.0, tk.heterogeneous, {}, 42);
        SimResult res = runDetailedSimulation(
            G, c.T, c.attack_lambda, c.global_resources,
            c.attack_strategy, c.gatekeeper,
            c.repair_slots, c.sample_points, 42);

        printTimeSeriesStats(topo, res);  // ← вывод статистики

        exportTimeSeries(res, c.out_dir + "/timeseries_" + topo + ".csv");
    }
}

static void runAll(Config c) {
    std::cout << "\n  ▶ TOPOLOGY\n";  runTopology(c);
    std::cout << "\n  ▶ SWEEP\n";     runSweep(c);
    std::cout << "\n  ▶ STRATEGY\n";  runStrategy(c);
    std::cout << "\n  ▶ ATTACK\n";    runAttack(c);
    std::cout << "\n  ▶ TIMESERIES\n"; runTimeSeries(c);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup-функции
// ─────────────────────────────────────────────────────────────────────────────
static Config setupTopology() {
    Config c;
    setupCommon(c);
    section("Параметры сценария");
    c.topologies = askMultiChoice("Топологии для сравнения",
        { "star", "ring", "full_mesh", "erdos_renyi", "barabasi_albert" }, c.topologies);
    c.attack_strategy = askChoice("Стратегия атаки",
        { "targeted", "random", "cascading" }, c.attack_strategy,
        { {"targeted", "Атака на хаб"}, {"random", "Случайная"}, {"cascading", "Эпидемическая"} });
    c.gatekeeper = askChoice("Политика Gatekeeper",
        { "fifo", "triage" }, c.gatekeeper,
        { {"fifo", "FIFO"}, {"triage", "Triage по критичности"} });
    return c;
}

static Config setupSweep() {
    Config c;
    setupCommon(c);
    section("Параметры сценария");
    c.topologies = askMultiChoice("Топологии для sweep",
        { "star", "ring", "full_mesh" }, c.topologies);
    c.attack_strategy = askChoice("Стратегия атаки",
        { "targeted", "random", "cascading" }, c.attack_strategy);
    c.gatekeeper = askChoice("Gatekeeper", { "fifo", "triage" }, c.gatekeeper);
    c.sweep_params = { "lambda", "resources", "mu", "P_attack" };
    return c;
}

static Config setupStrategy() {
    Config c;
    setupCommon(c);
    c.heterogeneous = true;
    section("Параметры сценария");
    c.topologies = askMultiChoice("Топологии",
        { "star", "ring", "full_mesh" }, c.topologies);
    c.attack_strategy = askChoice("Стратегия атаки",
        { "targeted", "random", "cascading" }, c.attack_strategy);
    c.resource_levels = askFloatList("Уровни ресурсов R через пробел",
        0, 10000, c.resource_levels);
    return c;
}

static Config setupAttack() {
    Config c;
    setupCommon(c);
    section("Параметры сценария");
    c.topologies = askMultiChoice("Топологии",
        { "star", "ring", "full_mesh" }, c.topologies);
    c.attack_strategies = askMultiChoice("Векторы атак",
        { "targeted", "random", "cascading" }, c.attack_strategies);
    c.gatekeeper = askChoice("Gatekeeper", { "fifo", "triage" }, c.gatekeeper);
    return c;
}

static Config setupTimeSeries() {
    Config c;
    setupCommon(c);
    section("Параметры сценария");
    c.topologies = askMultiChoice("Топологии",
        { "star", "ring", "full_mesh" }, { "star" });
    c.attack_strategy = askChoice("Стратегия атаки",
        { "targeted", "random", "cascading" }, c.attack_strategy);
    c.gatekeeper = askChoice("Gatekeeper", { "fifo", "triage" }, c.gatekeeper);
    c.sample_points = askInt("Точек временного ряда", 50, 2000, c.sample_points);
    return c;
}

static Config setupAll() {
    Config c;
    setupCommon(c);
    c.topologies = { "star", "ring", "full_mesh" };
    c.attack_strategies = { "targeted", "random", "cascading" };
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
// Главное меню
// ─────────────────────────────────────────────────────────────────────────────
struct ScenarioInfo {
    std::string key;
    std::string desc;
};

static const std::vector<ScenarioInfo> SCENARIOS = {
    {"topology",    "Сравнение топологий по всем метрикам"},
    {"sweep",       "Однофакторный параметрический анализ"},
    {"strategy",    "FIFO vs Triage при разных уровнях ресурсов"},
    {"attack",      "Сравнение векторов атак: targeted/random/cascading"},
    {"timeseries",  "Временные ряды одной симуляции (отладка)"},
    {"all",         "Все сценарии последовательно"},
};

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bool quick = (argc > 1 && std::string(argv[1]) == "--quick");

    header("Модель устойчивости информационной инфраструктуры");
    std::cout << '\n';

    if (!quick) {
        for (int i = 0; i < (int)SCENARIOS.size(); ++i)
            std::cout << "     " << ANSI_BOLD << (i + 1) << ANSI_RESET << ".   "
            << std::left << std::setw(14) << SCENARIOS[i].key
            << SCENARIOS[i].desc << '\n';
        std::cout << '\n';
        hint("Введите номер (1–6) или название. Enter = 1 (topology).");
        std::cout << '\n' << "   > ";

        std::string raw; std::getline(std::cin, raw);
        std::string scenario = "topology";
        if (!raw.empty()) {
            bool found = false;
            for (auto& s : SCENARIOS) if (raw == s.key) { scenario = s.key; found = true; break; }
            if (!found) {
                try {
                    int idx = std::stoi(raw) - 1;
                    if (idx >= 0 && idx < (int)SCENARIOS.size())
                        scenario = SCENARIOS[idx].key;
                }
                catch (...) {}
            }
        }

        header("Настройка: " + scenario);
        Config cfg;
        if (scenario == "topology")   cfg = setupTopology();
        else if (scenario == "sweep")      cfg = setupSweep();
        else if (scenario == "strategy")   cfg = setupStrategy();
        else if (scenario == "attack")     cfg = setupAttack();
        else if (scenario == "timeseries") cfg = setupTimeSeries();
        else                               cfg = setupAll();

        fs::create_directories(cfg.out_dir);
        printSummary(scenario, cfg);
        hr('=');
        std::cout << "  ЗАПУСК: " << ANSI_BOLD << scenario << ANSI_RESET << '\n';
        hr('=');

        if (scenario == "topology")   runTopology(cfg);
        else if (scenario == "sweep")      runSweep(cfg);
        else if (scenario == "strategy")   runStrategy(cfg);
        else if (scenario == "attack")     runAttack(cfg);
        else if (scenario == "timeseries") runTimeSeries(cfg);
        else                               runAll(cfg);

        hr('=');
        std::cout << "  " << ANSI_GREEN << "✓ Готово." << ANSI_RESET
            << "  CSV-файлы: " << cfg.out_dir
            << "\n  Для построения графиков: python plot_results.py " << cfg.out_dir << '\n';
        hr('=');

        std::cout << "\n  ⏳ Генерация графиков (вызов Python)...\n";
        std::string plot_cmd = "python plot_results.py " + cfg.out_dir;
        int ret = std::system(plot_cmd.c_str());
        if (ret == 0) {
            std::cout << ANSI_GREEN << "  ✓ Графики успешно построены!\n" << ANSI_RESET;
        }
        else {
            std::cout << ANSI_RED << "  ✗ Не удалось построить графики.\n" << ANSI_RESET;
        }
        hr('=');
    }
    else {
        std::cout << ANSI_RED << "  --quick: нет сохранённых настроек. Запустите без флага.\n" << ANSI_RESET;
        return 1;
    }
    return 0;
}