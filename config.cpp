#include "config.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace netsim {

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool parseBool(const std::string& text, bool& out) {
    const std::string v = toLower(trim(text));
    if (v == "1" || v == "true" || v == "yes" || v == "y" || v == "да") {
        out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "n" || v == "нет") {
        out = false;
        return true;
    }
    return false;
}

bool parseDoubleValue(const std::string& text, double& out) {
    try {
        size_t pos = 0;
        const std::string v = trim(text);
        out = std::stod(v, &pos);
        return pos == v.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

bool parseIntValue(const std::string& text, int& out) {
    try {
        size_t pos = 0;
        const std::string v = trim(text);
        const long long value = std::stoll(v, &pos);
        if (pos != v.size()) return false;
        out = static_cast<int>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseUInt64Value(const std::string& text, uint64_t& out) {
    try {
        size_t pos = 0;
        const std::string v = trim(text);
        out = std::stoull(v, &pos);
        return pos == v.size();
    } catch (...) {
        return false;
    }
}

std::vector<double> linspace(double lo, double hi, int points) {
    std::vector<double> values;
    if (points <= 0) return values;
    values.reserve(points);
    if (points == 1) {
        values.push_back(lo);
        return values;
    }
    for (int i = 0; i < points; ++i)
        values.push_back(lo + (hi - lo) * i / (points - 1));
    return values;
}

std::string joinDoubles(const std::vector<double>& values) {
    std::ostringstream oss;
    oss << std::setprecision(12);
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) oss << ",";
        oss << values[i];
    }
    return oss.str();
}

std::string joinStrings(const std::vector<std::string>& values) {
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) oss << ",";
        oss << values[i];
    }
    return oss.str();
}

bool parseRange(const std::string& text, std::vector<double>& out,
                std::string& error) {
    std::vector<double> raw;
    if (!parseDoubleList(text, raw, error)) return false;
    if (raw.size() != 3) {
        error = "Ожидается формат MIN,MAX,ТОЧЕК";
        return false;
    }
    if (!(raw[0] < raw[1])) {
        error = "MIN должен быть меньше MAX";
        return false;
    }
    const int points = static_cast<int>(std::llround(raw[2]));
    if (points < 2 || points > 100) {
        error = "Число точек должно быть от 2 до 100";
        return false;
    }
    out = linspace(raw[0], raw[1], points);
    return true;
}

bool isKnownAttack(const std::string& value) {
    return value == "targeted" || value == "random" || value == "cascading";
}

}  // namespace

std::string trim(const std::string& text) {
    size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;
    size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(begin, end - begin);
}

bool parseDoubleList(const std::string& text, std::vector<double>& out,
                     std::string& error) {
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == ',' || c == ';' || c == '\t' || c == '\n' || c == '\r')
            c = ' ';
    }
    std::istringstream stream(normalized);
    out.clear();
    double value = 0.0;
    while (stream >> value) out.push_back(value);
    if (!stream.eof()) {
        error = "Не удалось разобрать список чисел: '" + text + "'";
        return false;
    }
    if (out.empty()) {
        error = "Пустой список чисел";
        return false;
    }
    return true;
}

bool parseStringList(const std::string& text, std::vector<std::string>& out,
                     std::string& error) {
    out.clear();
    std::string token;
    for (char c : text) {
        if (c == ',' || c == ';' || std::isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) {
                out.push_back(trim(token));
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) out.push_back(trim(token));
    if (out.empty()) {
        error = "Пустой список значений";
        return false;
    }
    return true;
}

void SimConfig::ensureDefaultSweepRanges() {
    if (!sweep_ranges.count("lambda"))
        sweep_ranges["lambda"] = linspace(0.5, 5.0, 8);
    if (!sweep_ranges.count("resources"))
        sweep_ranges["resources"] = linspace(50.0, 1000.0, 8);
    if (!sweep_ranges.count("mu"))
        sweep_ranges["mu"] = linspace(0.05, 0.80, 8);
    if (!sweep_ranges.count("P_attack"))
        sweep_ranges["P_attack"] = linspace(0.30, 0.95, 8);
    if (sweep_params.empty())
        sweep_params = {"lambda", "resources", "mu", "P_attack"};
}

TopologyParams SimConfig::topologyParams() const {
    TopologyParams params;
    params.topology = topology;
    params.num_nodes = num_nodes;
    params.seed = seed;
    params.er_p = er_p;
    params.ba_m = ba_m;
    params.access_capacity = access_capacity;
    params.core_capacity = core_capacity;
    params.base_latency = base_latency;
    params.latency_jitter = latency_jitter;
    params.heterogeneous = heterogeneous;
    params.base_P_attack = base_P_attack;
    params.base_P_recovery = base_P_recovery;
    params.base_mu = base_mu;
    params.base_C_repair = base_C_repair;
    params.base_weight = base_weight;
    return params;
}

QosParams SimConfig::qosParams() const {
    QosParams params;
    params.qos_class = qosClassFromInt(qos_class);
    params.packet_bits = packet_bits;
    params.buffer_packets = buffer_packets;
    return params;
}

TrafficParams SimConfig::trafficParams() const {
    TrafficParams params;
    params.demand_per_pair_bps = demand_per_pair;
    params.assignment_iterations = traffic_iterations;
    params.hop_penalty_s = hop_penalty;
    params.preserve_total_demand = preserve_total_demand;
    return params;
}

bool SimConfig::applyOption(const std::string& key,
                            const std::string& value,
                            std::string& error) {
    const std::string k = toLower(trim(key));
    const std::string v = trim(value);
    const std::string lower = toLower(v);

    auto fail = [&](const std::string& message) {
        error = message + " (ключ '" + key + "')";
        return false;
    };

    if (k == "scenario") { scenario = lower; return true; }
    if (k == "topology") {
        if (!isKnownTopology(lower)) return fail("Неизвестная топология: " + v);
        topology = lower;
        return true;
    }
    if (k == "topologies") {
        std::vector<std::string> list;
        if (!parseStringList(v, list, error)) return fail(error);
        for (auto& item : list) {
            item = toLower(item);
            if (!isKnownTopology(item)) return fail("Неизвестная топология: " + item);
        }
        topologies = list;
        return true;
    }
    if (k == "num_nodes") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 2 || x > 1000)
            return fail("Ожидается целое 2..1000");
        num_nodes = x;
        return true;
    }
    if (k == "n_sims") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 1 || x > 100000)
            return fail("Ожидается целое 1..100000");
        n_sims = x;
        return true;
    }
    if (k == "t" || k == "horizon") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число");
        T = x;
        return true;
    }
    if (k == "sample_points") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 2 || x > 100000)
            return fail("Ожидается целое 2..100000");
        sample_points = x;
        return true;
    }
    if (k == "seed") {
        uint64_t x = 0;
        if (!parseUInt64Value(v, x)) return fail("Ожидается целое неотрицательное");
        seed = x;
        return true;
    }
    if (k == "attack_lambda" || k == "lambda") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0)
            return fail("Ожидается неотрицательное число");
        attack_lambda = x;
        return true;
    }
    if (k == "attack_strategy") {
        if (!isKnownAttack(lower))
            return fail("Допустимые: targeted, random, cascading");
        attack_strategy = lower;
        return true;
    }
    if (k == "attack_strategies") {
        std::vector<std::string> list;
        if (!parseStringList(v, list, error)) return fail(error);
        for (auto& item : list) {
            item = toLower(item);
            if (!isKnownAttack(item))
                return fail("Допустимые: targeted, random, cascading");
        }
        attack_strategies = list;
        return true;
    }
    if (k == "base_p_attack" || k == "p_attack") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0 || x > 1.0)
            return fail("Ожидается вероятность 0..1");
        base_P_attack = x;
        return true;
    }
    if (k == "gatekeeper") {
        if (lower != "fifo" && lower != "triage")
            return fail("Допустимые: fifo, triage");
        gatekeeper = lower;
        return true;
    }
    if (k == "repair_slots") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 0 || x > 1000)
            return fail("Ожидается целое 0..1000");
        repair_slots = x;
        return true;
    }
    if (k == "global_resources" || k == "resources") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0)
            return fail("Ожидается неотрицательное число");
        global_resources = x;
        return true;
    }
    if (k == "base_p_recovery" || k == "p_recovery") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0 || x > 1.0)
            return fail("Ожидается вероятность 0..1");
        base_P_recovery = x;
        return true;
    }
    if (k == "base_mu" || k == "mu") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число");
        base_mu = x;
        return true;
    }
    if (k == "base_c_repair") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 0)
            return fail("Ожидается целое неотрицательное");
        base_C_repair = x;
        return true;
    }
    if (k == "base_weight") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0)
            return fail("Ожидается неотрицательное число");
        base_weight = x;
        return true;
    }
    if (k == "heterogeneous") {
        bool x = false;
        if (!parseBool(v, x)) return fail("Ожидается да/нет");
        heterogeneous = x;
        return true;
    }
    if (k == "er_p") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0 || x > 1.0)
            return fail("Ожидается вероятность 0..1");
        er_p = x;
        return true;
    }
    if (k == "ba_m") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 1 || x > 1000)
            return fail("Ожидается целое 1..1000");
        ba_m = x;
        return true;
    }
    if (k == "access_capacity") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число, бит/с");
        access_capacity = x;
        return true;
    }
    if (k == "core_capacity") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число, бит/с");
        core_capacity = x;
        return true;
    }
    if (k == "base_latency") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число, с");
        base_latency = x;
        return true;
    }
    if (k == "latency_jitter") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0)
            return fail("Ожидается неотрицательное число, с");
        latency_jitter = x;
        return true;
    }
    if (k == "qos_class") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 0 || x > 7)
            return fail("Ожидается класс 0..7");
        qos_class = x;
        return true;
    }
    if (k == "packet_bits") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x <= 0.0)
            return fail("Ожидается положительное число, бит");
        packet_bits = x;
        return true;
    }
    if (k == "buffer_packets") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 1 || x > 1000000)
            return fail("Ожидается целое 1..1000000");
        buffer_packets = x;
        return true;
    }
    if (k == "demand_per_pair") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0)
            return fail("Ожидается неотрицательное число, бит/с");
        demand_per_pair = x;
        return true;
    }
    if (k == "traffic_iterations") {
        int x = 0;
        if (!parseIntValue(v, x) || x < 1 || x > 100)
            return fail("Ожидается целое 1..100");
        traffic_iterations = x;
        return true;
    }
    if (k == "hop_penalty") {
        double x = 0;
        if (!parseDoubleValue(v, x) || x < 0.0 || x > 1.0)
            return fail("Ожидается 0..1, с");
        hop_penalty = x;
        return true;
    }
    if (k == "preserve_total_demand") {
        bool x = false;
        if (!parseBool(v, x)) return fail("Ожидается да/нет");
        preserve_total_demand = x;
        return true;
    }
    if (k == "out_dir") {
        if (v.empty()) return fail("Пустой путь");
        out_dir = v;
        return true;
    }
    if (k == "resource_levels") {
        std::vector<double> values;
        if (!parseDoubleList(v, values, error)) return fail(error);
        for (double x : values)
            if (x < 0.0) return fail("Уровни ресурсов должны быть неотрицательны");
        resource_levels = values;
        return true;
    }
    if (k == "sweep_params") {
        std::vector<std::string> list;
        if (!parseStringList(v, list, error)) return fail(error);
        sweep_params = list;
        return true;
    }
    if (k == "sweep_lambda" || k == "sweep_resources" ||
        k == "sweep_mu" || k == "sweep_p_attack" || k == "sweep_demand") {
        std::string param = k.substr(6);  // после "sweep_"
        if (param == "p_attack") param = "P_attack";
        std::vector<double> values;
        if (!parseRange(v, values, error)) return fail(error);
        sweep_ranges[param] = values;
        return true;
    }

    return fail("Неизвестный параметр");
}

bool SimConfig::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "Не удалось открыть конфигурацию: " + path;
        return false;
    }

    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
        ++line_no;
        const std::string stripped = trim(line);
        if (stripped.empty() || stripped[0] == '#' || stripped[0] == ';')
            continue;
        const size_t eq = stripped.find('=');
        if (eq == std::string::npos) {
            error = "Строка " + std::to_string(line_no) + ": ожидается key=value";
            return false;
        }
        const std::string key = trim(stripped.substr(0, eq));
        const std::string value = trim(stripped.substr(eq + 1));
        std::string option_error;
        if (!applyOption(key, value, option_error)) {
            error = "Строка " + std::to_string(line_no) + ": " + option_error;
            return false;
        }
    }

    ensureDefaultSweepRanges();
    return validate(error);
}

bool SimConfig::saveToFile(const std::string& path, std::string& error) const {
    std::ofstream file(path);
    if (!file) {
        error = "Не удалось записать конфигурацию: " + path;
        return false;
    }

    file << "# Конфигурация NetSim (ВКР)\n";
    file << "scenario=" << scenario << "\n";
    file << "topology=" << topology << "\n";
    file << "topologies=" << joinStrings(topologies) << "\n";
    file << "num_nodes=" << num_nodes << "\n";
    file << "n_sims=" << n_sims << "\n";
    file << "T=" << std::setprecision(12) << T << "\n";
    file << "sample_points=" << sample_points << "\n";
    file << "seed=" << seed << "\n";
    file << "attack_lambda=" << attack_lambda << "\n";
    file << "attack_strategy=" << attack_strategy << "\n";
    file << "attack_strategies=" << joinStrings(attack_strategies) << "\n";
    file << "base_P_attack=" << base_P_attack << "\n";
    file << "gatekeeper=" << gatekeeper << "\n";
    file << "repair_slots=" << repair_slots << "\n";
    file << "global_resources=" << global_resources << "\n";
    file << "base_P_recovery=" << base_P_recovery << "\n";
    file << "base_mu=" << base_mu << "\n";
    file << "base_C_repair=" << base_C_repair << "\n";
    file << "base_weight=" << base_weight << "\n";
    file << "heterogeneous=" << (heterogeneous ? 1 : 0) << "\n";
    file << "er_p=" << er_p << "\n";
    file << "ba_m=" << ba_m << "\n";
    file << "access_capacity=" << access_capacity << "\n";
    file << "core_capacity=" << core_capacity << "\n";
    file << "base_latency=" << base_latency << "\n";
    file << "latency_jitter=" << latency_jitter << "\n";
    file << "qos_class=" << qos_class << "\n";
    file << "packet_bits=" << packet_bits << "\n";
    file << "buffer_packets=" << buffer_packets << "\n";
    file << "demand_per_pair=" << demand_per_pair << "\n";
    file << "traffic_iterations=" << traffic_iterations << "\n";
    file << "hop_penalty=" << hop_penalty << "\n";
    file << "preserve_total_demand=" << (preserve_total_demand ? 1 : 0) << "\n";
    file << "out_dir=" << out_dir << "\n";
    file << "resource_levels=" << joinDoubles(resource_levels) << "\n";
    file << "sweep_params=" << joinStrings(sweep_params) << "\n";
    for (const auto& [name, values] : sweep_ranges) {
        if (values.size() < 2) continue;
        file << "sweep_" << name << "="
             << values.front() << "," << values.back() << ","
             << values.size() << "\n";
    }
    return true;
}

bool SimConfig::validate(std::string& error) const {
    auto require = [&](bool condition, const std::string& message) {
        if (!condition) {
            error = message;
            return false;
        }
        return true;
    };

    if (!require(num_nodes >= 2 && num_nodes <= 1000,
                 "num_nodes: требуется 2..1000")) return false;
    if (!require(n_sims >= 1 && n_sims <= 100000,
                 "n_sims: требуется 1..100000")) return false;
    if (!require(T > 0.0 && std::isfinite(T), "T: требуется положительное число"))
        return false;
    if (!require(sample_points >= 2 && sample_points <= 100000,
                 "sample_points: требуется 2..100000")) return false;
    if (!require(attack_lambda >= 0.0, "attack_lambda: требуется ≥ 0")) return false;
    if (!require(base_P_attack >= 0.0 && base_P_attack <= 1.0,
                 "base_P_attack: требуется 0..1")) return false;
    if (!require(base_P_recovery >= 0.0 && base_P_recovery <= 1.0,
                 "base_P_recovery: требуется 0..1")) return false;
    if (!require(base_mu > 0.0, "base_mu: требуется > 0")) return false;
    if (!require(repair_slots >= 0, "repair_slots: требуется ≥ 0")) return false;
    if (!require(global_resources >= 0.0,
                 "global_resources: требуется ≥ 0")) return false;
    if (!require(qos_class >= 0 && qos_class <= 7,
                 "qos_class: требуется 0..7")) return false;
    if (!require(packet_bits > 0.0, "packet_bits: требуется > 0")) return false;
    if (!require(buffer_packets >= 1, "buffer_packets: требуется ≥ 1")) return false;
    if (!require(demand_per_pair >= 0.0,
                 "demand_per_pair: требуется ≥ 0")) return false;
    if (!require(traffic_iterations >= 1,
                 "traffic_iterations: требуется ≥ 1")) return false;
    if (!require(hop_penalty >= 0.0, "hop_penalty: требуется ≥ 0")) return false;
    if (!require(access_capacity > 0.0 && core_capacity > 0.0,
                 "Ёмкости каналов должны быть положительны")) return false;
    if (!require(base_latency > 0.0 && latency_jitter >= 0.0,
                 "Задержки должны быть неотрицательны")) return false;
    if (!require(er_p >= 0.0 && er_p <= 1.0, "er_p: требуется 0..1")) return false;
    if (!require(ba_m >= 1 && ba_m < num_nodes,
                 "ba_m: требуется 1..num_nodes-1")) return false;
    if (!require(isKnownTopology(topology),
                 "Неизвестная топология: " + topology)) return false;
    if (!require(!topologies.empty(), "Список топологий пуст")) return false;
    for (const auto& name : topologies)
        if (!require(isKnownTopology(name),
                     "Неизвестная топология в списке: " + name)) return false;
    if (!require(isKnownAttack(attack_strategy),
                 "Неизвестная стратегия атаки: " + attack_strategy)) return false;
    for (const auto& name : attack_strategies)
        if (!require(isKnownAttack(name),
                     "Неизвестная стратегия атаки: " + name)) return false;
    if (!require(gatekeeper == "fifo" || gatekeeper == "triage",
                 "gatekeeper: допустимо fifo или triage")) return false;
    if (!require(!resource_levels.empty(),
                 "resource_levels: список пуст")) return false;
    for (double r : resource_levels)
        if (!require(r >= 0.0, "resource_levels: значения ≥ 0")) return false;

    for (const auto& [name, values] : sweep_ranges) {
        if (!require(values.size() >= 2 && values.size() <= 100,
                     "sweep_" + name + ": требуется 2..100 точек")) return false;
    }

    error.clear();
    return true;
}

std::string SimConfig::summary() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "Сценарий: " << scenario << "\n"
        << "Топология: " << topology << " (N=" << num_nodes << ")\n"
        << "Монте-Карло: " << n_sims << " прогонов, T=" << T
        << ", точек=" << sample_points << "\n"
        << "Атаки: " << attack_strategy << ", λ=" << attack_lambda
        << ", P_attack=" << base_P_attack << "\n"
        << "Восстановление: " << gatekeeper << ", слотов=" << repair_slots
        << ", R=" << global_resources << ", μ=" << base_mu
        << ", P_recovery=" << base_P_recovery << "\n"
        << "QoS: " << qosRequirementText(qosClassFromInt(qos_class)) << "\n"
        << "Трафик: " << demand_per_pair / 1.0e6 << " Мбит/с на пару, "
        << "итераций=" << traffic_iterations
        << ", плата за транзит=" << hop_penalty * 1000.0 << " мс"
        << ", сохранять сумму=" << (preserve_total_demand ? "да" : "нет") << "\n"
        << "Каналы: доступ=" << access_capacity / 1.0e6 << " Мбит/с, ядро="
        << core_capacity / 1.0e9 << " Гбит/с, задержка="
        << base_latency * 1000.0 << " мс\n";
    return oss.str();
}

}  // namespace netsim
