/**
 main.cpp — точка входа NetSim (ВКР).
 Режимы:
   netsim                     — интерактивное меню
   netsim run --config f.ini  — пакетный запуск (используется GUI)
   netsim selftest            — самопроверка ядра
   netsim version             — версия
*/
#include "config.h"
#include "csv_export.h"
#include "engine.h"
#include "experiments.h"
#include "qos.h"
#include "topology.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace netsim;

namespace {

constexpr const char* kVersion = "2.0.0";

#ifdef _WIN32
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kGreen = "\033[92m";
constexpr const char* kYellow = "\033[93m";
constexpr const char* kRed = "\033[91m";
constexpr const char* kDim = "\033[90m";
constexpr const char* kCyan = "\033[96m";
#else
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kGreen = "\033[92m";
constexpr const char* kYellow = "\033[93m";
constexpr const char* kRed = "\033[91m";
constexpr const char* kDim = "\033[90m";
constexpr const char* kCyan = "\033[96m";
#endif

void hr(char c = '-', int width = 78) {
    for (int i = 0; i < width; ++i) std::cout << c;
    std::cout << '\n';
}

void header(const std::string& title) {
    hr('=');
    std::cout << "  " << kBold << title << kReset << '\n';
    hr('=');
}

void progressPrinter(double fraction, const std::string& stage) {
    std::cout << "PROGRESS " << std::fixed << std::setprecision(4)
              << fraction << " " << stage << "\n"
              << std::flush;
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

// ---------------------------------------------------------------------------
// Интерактивные помощники
// ---------------------------------------------------------------------------
std::string askString(const std::string& prompt, const std::string& def) {
    std::cout << "  " << kBold << prompt << kReset << " [" << def << "]: ";
    std::string line;
    std::getline(std::cin, line);
    return line.empty() ? def : line;
}

int askInt(const std::string& prompt, int lo, int hi, int def) {
    while (true) {
        const std::string raw = askString(prompt, std::to_string(def));
        try {
            const int value = std::stoi(raw);
            if (value >= lo && value <= hi) return value;
        } catch (...) {
        }
        std::cout << "  " << kRed << "Ожидается целое " << lo << ".." << hi
                  << kReset << '\n';
    }
}

double askDouble(const std::string& prompt, double lo, double hi, double def) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << def;
    while (true) {
        const std::string raw = askString(prompt, oss.str());
        try {
            std::string normalized = raw;
            std::replace(normalized.begin(), normalized.end(), ',', '.');
            const double value = std::stod(normalized);
            if (value >= lo && value <= hi) return value;
        } catch (...) {
        }
        std::cout << "  " << kRed << "Ожидается число " << lo << ".." << hi
                  << kReset << '\n';
    }
}

bool askBool(const std::string& prompt, bool def) {
    const std::string raw =
        lower(askString(prompt + " (да/нет)", def ? "да" : "нет"));
    return raw == "да" || raw == "yes" || raw == "y" || raw == "1" ||
           raw == "true";
}

std::string askChoice(const std::string& prompt,
                      const std::vector<std::string>& options,
                      const std::string& def) {
    std::cout << "  Варианты: ";
    for (size_t i = 0; i < options.size(); ++i)
        std::cout << (i ? " | " : "") << options[i];
    std::cout << '\n';
    while (true) {
        const std::string raw = lower(askString(prompt, def));
        for (const auto& option : options)
            if (raw == option) return option;
        std::cout << "  " << kRed << "Выберите вариант из списка" << kReset
                  << '\n';
    }
}

std::vector<std::string> askMultiChoice(
    const std::string& prompt, const std::vector<std::string>& options,
    const std::vector<std::string>& defaults) {
    std::cout << "  Доступно: ";
    for (size_t i = 0; i < options.size(); ++i)
        std::cout << (i ? ", " : "") << options[i];
    std::cout << '\n';

    std::string default_text;
    for (size_t i = 0; i < defaults.size(); ++i)
        default_text += (i ? "," : "") + defaults[i];

    while (true) {
        const std::string raw = askString(prompt, default_text);
        std::vector<std::string> result;
        std::string token;
        std::istringstream stream(raw);
        bool ok = true;
        while (std::getline(stream, token, ',')) {
            token = trim(token);
            if (token.empty()) continue;
            const std::string value = lower(token);
            if (std::find(options.begin(), options.end(), value) ==
                options.end()) {
                ok = false;
                break;
            }
            result.push_back(value);
        }
        if (ok && !result.empty()) return result;
        std::cout << "  " << kRed << "Неизвестный вариант" << kReset << '\n';
    }
}

// ---------------------------------------------------------------------------
// Печать таблиц
// ---------------------------------------------------------------------------
void printTopologyTable(const TopoCompResult& comparison) {
    hr();
    std::cout << kBold << "  Сравнение топологий\n" << kReset;
    std::cout << "  " << std::left << std::setw(20) << "Топология"
              << std::setw(12) << "E_QoS"
              << std::setw(12) << "Q-доля"
              << std::setw(12) << "LCC"
              << std::setw(12) << "φ_w"
              << std::setw(12) << "IPTD,мс"
              << std::setw(12) << "IPLR" << '\n';
    hr('-', 92);
    std::cout << std::fixed << std::setprecision(3);
    for (const auto& [topo, mc] : comparison.results) {
        std::cout << "  " << std::left << std::setw(20) << topo
                  << std::setw(12) << mc.e_qos.mean
                  << std::setw(12) << mc.qos_ratio.mean
                  << std::setw(12) << mc.lcc.mean
                  << std::setw(12) << mc.phi_w.mean
                  << std::setw(12) << mc.iptd_ms.mean
                  << std::setw(12) << mc.iplr.mean << '\n';
    }
    hr();
}

void printStrategyTable(const StrategyResults& results,
                        const std::vector<double>& levels) {
    hr();
    std::cout << kBold << "  FIFO vs Triage (E_QoS)\n" << kReset;
    for (const auto& [topo, gk_map] : results) {
        std::cout << "\n  [" << topo << "]\n  " << std::left << std::setw(10)
                  << "R" << std::setw(12) << "FIFO" << std::setw(12)
                  << "Triage" << std::setw(10) << "p" << "Значимость\n";
        for (double r : levels) {
            auto it = gk_map.at("fifo").find(r);
            if (it == gk_map.at("fifo").end()) continue;
            const auto& fifo = it->second;
            const auto& triage = gk_map.at("triage").at(r);
            std::cout << std::fixed << std::setprecision(3) << "  "
                      << std::left << std::setw(10) << r
                      << std::setw(12) << fifo.mc.e_qos.mean
                      << std::setw(12) << triage.mc.e_qos.mean
                      << std::setw(10) << fifo.sig.p_raw
                      << fifo.sig.direction << '\n';
        }
    }
    hr();
}

// ---------------------------------------------------------------------------
// Запуск сценариев
// ---------------------------------------------------------------------------
int runScenario(SimConfig config, const std::string& scenario,
                const std::string& out_dir, bool show_progress = false) {
    config.scenario = scenario;
    config.out_dir = out_dir;
    std::filesystem::create_directories(out_dir);

    std::string error;
    if (!config.validate(error)) {
        std::cerr << "Ошибка конфигурации: " << error << "\n";
        return 2;
    }
    if (!config.saveToFile(out_dir + "/config.ini", error)) {
        std::cerr << "Предупреждение: " << error << "\n";
    }

    const ProgressFn progress =
        show_progress ? ProgressFn(progressPrinter) : ProgressFn{};

    try {
        if (scenario == "topology") {
            const auto comparison = runTopologyComparison(
                config, config.topologies, config.n_sims, progress);
            printTopologyTable(comparison);
            for (const auto& [topo, mc] : comparison.results) {
                if (mc.baseline_qos_rate < 0.999) {
                    std::cout << kYellow << "  Внимание: для топологии " << topo
                              << " доля прогонов с E0_QoS > 0 составляет "
                              << std::fixed << std::setprecision(3)
                              << mc.baseline_qos_rate
                              << ". Проверьте класс QoS и нагрузку." << kReset
                              << "\n";
                }
            }
            exportTopologyComparison(comparison,
                                     out_dir + "/topology_comparison.csv");
            exportTopologySignificance(
                comparison, out_dir + "/topology_significance.csv");
        } else if (scenario == "sweep") {
            config.ensureDefaultSweepRanges();
            std::map<std::string, std::map<std::string, SweepResult>> data;
            const int total = std::max(
                1, static_cast<int>(config.sweep_params.size() *
                                    config.topologies.size()));
            int done = 0;
            for (const auto& param : config.sweep_params) {
                auto range_it = config.sweep_ranges.find(param);
                if (range_it == config.sweep_ranges.end()) continue;
                for (const auto& topo : config.topologies) {
                    const int offset = done;
                    data[param][topo] = runParameterSweep(
                        config, param, range_it->second, topo, config.n_sims,
                        [&, offset, total](double f, const std::string& stage) {
                            progressPrinter((offset + f) / total,
                                            param + " / " + topo + ": " + stage);
                        });
                    ++done;
                }
            }
            exportParameterSweep(data, config.sweep_ranges, out_dir);
        } else if (scenario == "strategy") {
            const auto results = runStrategyComparison(
                config, config.topologies, config.resource_levels,
                config.n_sims, progress);
            printStrategyTable(results, config.resource_levels);
            exportStrategyComparison(results, config.resource_levels,
                                     out_dir + "/strategy_comparison.csv");
        } else if (scenario == "attack") {
            const auto results = runAttackVectorAnalysis(
                config, config.attack_strategies, config.topologies,
                config.n_sims, progress);
            exportAttackVectorAnalysis(results,
                                       out_dir + "/attack_vector_analysis.csv");
        } else if (scenario == "timeseries") {
            const auto& topos = config.topologies.empty()
                                    ? std::vector<std::string>{config.topology}
                                    : config.topologies;
            for (size_t i = 0; i < topos.size(); ++i) {
                config.topology = topos[i];
                const Graph graph = createNetworkTopology(config.topologyParams());
                const SimResult result = runSimulation(graph, config);
                if (!result.baseline_qos_ok) {
                    std::cout << kYellow << "  Внимание: начальная сеть не "
                              << "удовлетворяет требованиям класса QoS; "
                              << "E_QoS_norm будет равен 0." << kReset << "\n";
                }
                exportTimeSeries(result,
                                 out_dir + "/timeseries_" + topos[i] + ".csv");
                exportRunMetadata(config, result,
                                  out_dir + "/run_meta_" + topos[i] + ".csv");
                progress((i + 1.0) / topos.size(), "timeseries " + topos[i]);
            }
        } else if (scenario == "all") {
            const std::vector<std::string> parts = {
                "topology", "sweep", "strategy", "attack", "timeseries"};
            for (size_t i = 0; i < parts.size(); ++i) {
                std::cout << "\n>>> Сценарий: " << parts[i] << "\n";
                if (runScenario(config, parts[i], out_dir, show_progress) != 0)
                    return 1;
                progress((i + 1.0) / parts.size(), "all: " + parts[i]);
            }
        } else {
            std::cerr << "Неизвестный сценарий: " << scenario << "\n";
            return 2;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Ошибка выполнения: " << ex.what() << "\n";
        return 1;
    }

    progress(1.0, "готово");
    return 0;
}

// ---------------------------------------------------------------------------
// Самопроверка
// ---------------------------------------------------------------------------
int runSelfTest() {
    std::cout << "NetSim self-test " << kVersion << "\n";
    int failures = 0;

    auto check = [&](bool condition, const std::string& name) {
        std::cout << (condition ? "  [OK]   " : "  [FAIL] ") << name << "\n";
        if (!condition) ++failures;
    };

    // 1. Таблица Y.1541.
    check(std::abs(qosRequirement(QosClass::Class0).iptd_s - 0.1) < 1e-12,
          "Y.1541: класс 0, IPTD = 100 мс");
    check(std::abs(qosRequirement(QosClass::Class4).iptd_s - 1.0) < 1e-12,
          "Y.1541: класс 4, IPTD = 1 с");
    check(std::abs(qosRequirement(QosClass::Class6).iplr - 1e-5) < 1e-15,
          "Y.1541: класс 6, IPLR = 1e-5");

    // 2. M/M/1: при ρ = 0.5 задержка = base + 2/μ.
    Edge edge;
    edge.capacity = 1.0e9;
    edge.base_latency = 0.001;
    edge.load = 0.5e9;
    QosParams qp;
    qp.packet_bits = 12000.0;
    const EdgeQos eq = evaluateEdgeQos(edge, qp);
    const double mu = edge.capacity / qp.packet_bits;
    check(std::abs(eq.delay_s - (edge.base_latency + 2.0 / mu)) < 1e-9,
          "M/M/1: IPTD при ρ = 0.5");
    check(eq.loss >= 0.0 && eq.loss < 1.0, "M/M/1: IPLR в допустимом диапазоне");

    // 3. Топология и связность.
    TopologyParams tp;
    tp.topology = "barabasi_albert";
    tp.num_nodes = 40;
    tp.seed = 7;
    const Graph graph = createNetworkTopology(tp);
    check(graph.num_nodes == 40, "BA: число вершин");
    check(graph.num_edges > 0, "BA: число дуг");
    check(computeLccRatio(graph) > 0.999, "BA: граф связен");

    // 4. Симуляция.
    SimConfig cfg;
    cfg.topology = "barabasi_albert";
    cfg.num_nodes = 30;
    cfg.n_sims = 1;
    cfg.T = 30.0;
    cfg.sample_points = 60;
    cfg.attack_lambda = 0.8;
    cfg.global_resources = 80.0;
    cfg.repair_slots = 2;
    cfg.seed = 11;
    cfg.qos_class = 1;
    const Graph sim_graph = createNetworkTopology(cfg.topologyParams());
    const SimResult result = runSimulation(sim_graph, cfg);
    check(result.metrics.size() == 60, "DES: число точек временного ряда");
    check(result.e0_phys > 0.0, "DES: E0_phys > 0");
    check(std::isfinite(result.metrics.back().e_qos_norm),
          "DES: E_QoS_norm конечно");

    // 5. Воспроизводимость.
    const SimResult repeat = runSimulation(sim_graph, cfg);
    check(std::abs(result.metrics.back().lcc -
                   repeat.metrics.back().lcc) < 1e-12,
          "DES: воспроизводимость при одинаковом seed");

    std::cout << (failures == 0 ? kGreen : kRed)
              << (failures == 0 ? "\nВсе проверки пройдены."
                                : "\nПровалено проверок: " +
                                      std::to_string(failures))
              << kReset << "\n";
    return failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------
void printUsage() {
    std::cout
        << "NetSim " << kVersion << " — симулятор живучести и QoS IP-сетей\n\n"
        << "Использование:\n"
        << "  netsim                          интерактивное меню\n"
        << "  netsim run --config cfg.ini     пакетный запуск\n"
        << "  netsim selftest                 самопроверка\n"
        << "  netsim version                  версия\n\n"
        << "Опции run:\n"
        << "  --scenario topology|sweep|strategy|attack|timeseries|all\n"
        << "  --config <path>                 загрузить INI\n"
        << "  --out <dir>                     каталог результатов\n"
        << "  --topology <name>               одна топология\n"
        << "  --topologies a,b,c              список топологий\n"
        << "  --num-nodes N --sims M --T X\n"
        << "  --lambda L --resources R --slots S\n"
        << "  --attack targeted|random|cascading\n"
        << "  --gatekeeper fifo|triage\n"
        << "  --qos-class 0..7 --demand BPS\n"
        << "  --buffer K --access BPS --core BPS --hop-penalty S\n"
        << "  --seed N --progress\n"
        << "  --quick                         повтор последней конфигурации\n";
}

int runCli(int argc, char** argv) {
    int index = 2;
    SimConfig config;
    config.ensureDefaultSweepRanges();

    bool progress = false;
    bool topologies_set = false;
    bool topology_set = false;

    if (argc > 2 && std::string(argv[2]) == "--quick") {
        std::string error;
        if (!config.loadFromFile("results/last_config.ini", error)) {
            std::cerr << "Не удалось загрузить последнюю конфигурацию: "
                      << error << "\n";
            return 2;
        }
        index = 3;
    }

    for (int i = index; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--progress") {
            progress = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        }
        if (arg.rfind("--", 0) != 0) {
            std::cerr << "Неизвестный аргумент: " << arg << "\n";
            return 2;
        }

        std::string key = arg.substr(2);
        std::string value;
        const size_t eq = key.find('=');
        if (eq != std::string::npos) {
            value = key.substr(eq + 1);
            key = key.substr(0, eq);
        } else {
            if (i + 1 >= argc) {
                std::cerr << "Для --" << key << " требуется значение\n";
                return 2;
            }
            value = argv[++i];
        }

        std::replace(key.begin(), key.end(), '-', '_');
        const std::string normalized = lower(key);

        if (normalized == "config") {
            std::string error;
            if (!config.loadFromFile(value, error)) {
                std::cerr << "Ошибка конфигурации: " << error << "\n";
                return 2;
            }
            continue;
        }
        if (normalized == "progress") {
            progress = true;
            continue;
        }

        std::string option = normalized;
        if (option == "sims" || option == "mc") option = "n_sims";
        if (option == "horizon") option = "T";
        if (option == "out") option = "out_dir";
        if (option == "nodes") option = "num_nodes";
        if (option == "slot" || option == "slots") option = "repair_slots";
        if (option == "demand") option = "demand_per_pair";
        if (option == "packet") option = "packet_bits";
        if (option == "buffer") option = "buffer_packets";
        if (option == "access") option = "access_capacity";
        if (option == "core") option = "core_capacity";
        if (option == "latency") option = "base_latency";
        if (option == "qos") option = "qos_class";
        if (option == "topo") option = "topology";

        if (option == "topologies") topologies_set = true;
        if (option == "topology") topology_set = true;

        std::string error;
        if (!config.applyOption(option, value, error)) {
            std::cerr << "Ошибка: " << error << "\n";
            return 2;
        }
    }

    if (topology_set && !topologies_set)
        config.topologies = {config.topology};

    config.ensureDefaultSweepRanges();
    std::string error;
    if (!config.validate(error)) {
        std::cerr << "Ошибка конфигурации: " << error << "\n";
        return 2;
    }

    if (!progress) {
        std::cout << config.summary();
    }

    const std::string out_dir =
        config.out_dir.empty() ? "results" : config.out_dir;
    const int code = runScenario(config, config.scenario, out_dir, progress);

    // Сохраняем последнюю конфигурацию для --quick.
    std::filesystem::create_directories("results");
    config.out_dir = out_dir;
    config.saveToFile("results/last_config.ini", error);

    return code;
}

// ---------------------------------------------------------------------------
// Интерактивное меню
// ---------------------------------------------------------------------------
void configureAdvanced(SimConfig& cfg) {
    std::cout << "\n" << kCyan << "  Расширенные параметры (Enter — по умолчанию)"
              << kReset << "\n";
    cfg.heterogeneous = askBool("Гетерогенная сеть (server/switch/host)",
                                cfg.heterogeneous);
    if (!cfg.heterogeneous) {
        cfg.base_P_attack =
            askDouble("P_attack (вероятность успеха атаки)", 0.01, 0.99,
                      cfg.base_P_attack);
        cfg.base_P_recovery =
            askDouble("P_recovery (вероятность восстановления)", 0.01, 0.99,
                      cfg.base_P_recovery);
        cfg.base_mu = askDouble("μ (интенсивность восстановления)", 0.01, 1.0,
                                cfg.base_mu);
    }
    cfg.access_capacity =
        askDouble("Пропускная способность доступа, бит/с", 1e6, 1e12,
                  cfg.access_capacity);
    cfg.core_capacity =
        askDouble("Пропускная способность ядра, бит/с", 1e6, 1e12,
                  cfg.core_capacity);
    cfg.base_latency = askDouble("Базовая задержка канала, с", 0.0001, 0.1,
                                 cfg.base_latency);
    cfg.demand_per_pair = askDouble("Трафик на пару, бит/с", 0.0, 1e9,
                                    cfg.demand_per_pair);
    cfg.packet_bits = askDouble("Размер пакета, бит", 8000.0, 16000.0,
                                cfg.packet_bits);
    cfg.buffer_packets =
        askInt("Ёмкость буфера, пакетов", 1, 10000, cfg.buffer_packets);
    cfg.er_p = askDouble("ER: вероятность ребра p", 0.0, 1.0, cfg.er_p);
    cfg.ba_m = askInt("BA: связей на новый узел m", 1, cfg.num_nodes - 1,
                      cfg.ba_m);
    cfg.seed = static_cast<uint64_t>(
        askInt("Seed генератора", 0, 1000000000, static_cast<int>(cfg.seed)));
}

int interactive() {
    header("NetSim — модель живучести информационной инфраструктуры");
    std::cout << "  " << kDim << "ВКР: Y.1541 + M/M/1 + дискретно-событийное ядро"
              << kReset << "\n\n";

    std::cout << "  1. topology    Сравнение топологий\n"
              << "  2. sweep       Параметрический анализ\n"
              << "  3. strategy    FIFO vs Triage\n"
              << "  4. attack      Векторы атак\n"
              << "  5. timeseries  Временные ряды\n"
              << "  6. all         Все сценарии\n"
              << "  7. selftest    Самопроверка\n"
              << "  0. exit        Выход\n";

    const std::string choice = askString("Выбор", "1");
    if (choice == "0" || lower(choice) == "exit") return 0;
    if (choice == "7" || lower(choice) == "selftest") return runSelfTest();

    SimConfig cfg;
    const std::map<std::string, std::string> scenarios = {
        {"1", "topology"}, {"2", "sweep"},    {"3", "strategy"},
        {"4", "attack"},   {"5", "timeseries"}, {"6", "all"},
        {"topology", "topology"}, {"sweep", "sweep"},
        {"strategy", "strategy"}, {"attack", "attack"},
        {"timeseries", "timeseries"}, {"all", "all"}};

    auto it = scenarios.find(lower(choice));
    const std::string scenario = (it == scenarios.end()) ? "topology" : it->second;

    header("Настройка: " + scenario);
    cfg.num_nodes = askInt("Количество узлов N", 5, 500, cfg.num_nodes);
    cfg.n_sims = askInt("Прогонов Монте-Карло", 1, 1000, cfg.n_sims);
    cfg.T = askDouble("Горизонт моделирования T", 1.0, 5000.0, cfg.T);
    cfg.attack_lambda =
        askDouble("Интенсивность атак λ", 0.0, 50.0, cfg.attack_lambda);
    cfg.global_resources =
        askDouble("Глобальные ресурсы R", 0.0, 100000.0, cfg.global_resources);
    cfg.repair_slots = askInt("Бригад восстановления", 0, 100, cfg.repair_slots);

    if (scenario == "topology" || scenario == "sweep" || scenario == "attack" ||
        scenario == "strategy" || scenario == "all" || scenario == "timeseries") {
        cfg.topologies = askMultiChoice(
            "Топологии (через запятую)", availableTopologies(),
            scenario == "timeseries" ? std::vector<std::string>{cfg.topology}
                                     : cfg.topologies);
        cfg.topology = cfg.topologies.front();
    }

    if (scenario == "attack" || scenario == "all") {
        cfg.attack_strategies = askMultiChoice(
            "Векторы атак", {"targeted", "random", "cascading"},
            cfg.attack_strategies);
        cfg.attack_strategy = cfg.attack_strategies.front();
    } else {
        cfg.attack_strategy = askChoice(
            "Стратегия атаки", {"targeted", "random", "cascading"},
            cfg.attack_strategy);
    }

    cfg.gatekeeper =
        askChoice("Политика восстановления", {"fifo", "triage"}, cfg.gatekeeper);

    std::cout << "\n  " << kCyan << "Классы QoS Y.1541:" << kReset << "\n"
              << qosTableText();
    cfg.qos_class = askInt("Класс QoS (0..7)", 0, 7, cfg.qos_class);

    if (scenario == "strategy") {
        std::cout << "  Уровни ресурсов, например: 50 100 200 500\n";
        const std::string raw = askString("Уровни R", "50 100 200 500");
        std::istringstream stream(raw);
        std::vector<double> levels;
        double value = 0.0;
        while (stream >> value) levels.push_back(value);
        if (levels.size() >= 2) cfg.resource_levels = levels;
    }

    if (askBool("Настроить расширенные параметры", false))
        configureAdvanced(cfg);

    std::cout << "\n" << cfg.summary();
    if (!askBool("Запустить", true)) return 0;

    return runScenario(cfg, scenario, cfg.out_dir, true);
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        if (argc <= 1) return interactive();

        const std::string command = lower(argv[1]);
        if (command == "version" || command == "--version" || command == "-v") {
            std::cout << "NetSim " << kVersion << "\n";
            return 0;
        }
        if (command == "help" || command == "--help" || command == "-h") {
            printUsage();
            return 0;
        }
        if (command == "selftest" || command == "--selftest") return runSelfTest();
        if (command == "run" || command.rfind("--", 0) == 0)
            return runCli(argc, argv);
        if (command == "interactive") return interactive();

        std::cerr << "Неизвестная команда: " << argv[1] << "\n";
        printUsage();
        return 2;
    } catch (const std::exception& ex) {
        std::cerr << "Критическая ошибка: " << ex.what() << "\n";
        return 1;
    }
}
