#pragma once
// ---------------------------------------------------------------------------
// config.h — единая конфигурация симуляции, разбор INI-файлов и опций
// командной строки. Один и тот же набор параметров используется
// интерактивным меню, CLI-режимом и графическим интерфейсом.
// ---------------------------------------------------------------------------

#include "qos.h"
#include "topology.h"
#include "traffic.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace netsim {

struct SimConfig {
    // Сценарий и топология
    std::string scenario = "topology";
    std::string topology = "barabasi_albert";
    std::vector<std::string> topologies = {
        "star", "ring", "full_mesh", "erdos_renyi", "barabasi_albert"};

    // Масштаб эксперимента
    int    num_nodes = 50;
    int    n_sims = 30;
    double T = 150.0;
    int    sample_points = 200;
    uint64_t seed = 42;

    // Атаки
    double attack_lambda = 1.0;
    std::string attack_strategy = "targeted";
    std::vector<std::string> attack_strategies = {
        "targeted", "random", "cascading"};
    double base_P_attack = 0.60;

    // Восстановление
    std::string gatekeeper = "fifo";
    int    repair_slots = 3;
    double global_resources = 200.0;
    double base_P_recovery = 0.80;
    double base_mu = 0.20;
    int    base_C_repair = 1;
    double base_weight = 1.0;
    bool   heterogeneous = true;

    // Генератор топологии
    double er_p = 0.10;
    int    ba_m = 2;
    double access_capacity = 1.0e9;
    double core_capacity = 10.0e9;
    double base_latency = 0.002;
    double latency_jitter = 0.001;

    // QoS и трафик
    int    qos_class = 1;
    double packet_bits = 12000.0;
    int    buffer_packets = 100;
    double demand_per_pair = 2.0e6;
    int    traffic_iterations = 3;
    double hop_penalty = 0.005;  // плата за транзит, с
    bool   preserve_total_demand = true;

    // Вывод
    std::string out_dir = "results";
    std::vector<double> resource_levels = {50, 100, 200, 500};
    std::vector<std::string> sweep_params = {
        "lambda", "resources", "mu", "P_attack"};
    std::map<std::string, std::vector<double>> sweep_ranges;

    // Производные параметры для модулей ядра.
    TopologyParams topologyParams() const;
    QosParams qosParams() const;
    TrafficParams trafficParams() const;

    // Проверка корректности. Возвращает false и заполняет error.
    bool validate(std::string& error) const;

    // Применение одной опции (ключ = значение). Используется INI-парсером и CLI.
    bool applyOption(const std::string& key,
                     const std::string& value,
                     std::string& error);

    bool loadFromFile(const std::string& path, std::string& error);
    bool saveToFile(const std::string& path, std::string& error) const;

    // Человекочитаемая сводка (для журнала и GUI).
    std::string summary() const;

    // Заполнение диапазонов sweep значениями по умолчанию.
    void ensureDefaultSweepRanges();
};

// Разбор строки в список чисел (разделители: запятая, точка с запятой,
// пробелы). Возвращает false при неверном формате.
bool parseDoubleList(const std::string& text,
                     std::vector<double>& out,
                     std::string& error);

bool parseStringList(const std::string& text,
                     std::vector<std::string>& out,
                     std::string& error);

std::string trim(const std::string& text);

}  // namespace netsim
