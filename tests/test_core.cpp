// ---------------------------------------------------------------------------
// test_core.cpp — модульные и интеграционные проверки ядра NetSim.
// Сборка: tests\build_tests.ps1
// ---------------------------------------------------------------------------

#include "config.h"
#include "csv_export.h"
#include "engine.h"
#include "metrics.h"
#include "qos.h"
#include "topology.h"
#include "topology_metrics.h"
#include "traffic.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace netsim;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& name) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  [FAIL] " << name << "\n";
    } else {
        std::cout << "  [OK]   " << name << "\n";
    }
}

void checkNear(double actual, double expected, double tolerance,
               const std::string& name) {
    check(std::abs(actual - expected) <= tolerance,
          name + " (получено " + std::to_string(actual) + ", ожидалось " +
              std::to_string(expected) + ")");
}

Graph makeLine(int n, double capacity, double latency) {
    Graph g;
    g.topology = "line";
    g.num_nodes = n;
    g.nodes.resize(n);
    g.adj.resize(n);
    for (int i = 0; i < n; ++i) {
        g.nodes[i].id = i;
        g.nodes[i].state = 1;
        g.nodes[i].weight = 1.0;
        g.nodes[i].C_repair = 1;
        g.nodes[i].mu = 1.0;
        g.nodes[i].P_attack = 1.0;
        g.nodes[i].P_recovery = 1.0;
    }
    for (int i = 0; i + 1 < n; ++i)
        g.addBidirectional(i, i + 1, capacity, latency);
    g.rebuildEdgeIndex();
    return g;
}

}  // namespace

int main() {
    std::cout << "NetSim core tests\n";

    // ---------------------------------------------------------------------
    // Y.1541
    // ---------------------------------------------------------------------
    checkNear(qosRequirement(QosClass::Class0).iptd_s, 0.100, 1e-12,
              "Y.1541 класс 0 IPTD");
    checkNear(qosRequirement(QosClass::Class1).iptd_s, 0.400, 1e-12,
              "Y.1541 класс 1 IPTD");
    checkNear(qosRequirement(QosClass::Class2).iptd_s, 0.100, 1e-12,
              "Y.1541 класс 2 IPTD");
    checkNear(qosRequirement(QosClass::Class3).iptd_s, 0.400, 1e-12,
              "Y.1541 класс 3 IPTD");
    checkNear(qosRequirement(QosClass::Class4).iptd_s, 1.000, 1e-12,
              "Y.1541 класс 4 IPTD");
    check(!std::isfinite(qosRequirement(QosClass::Class5).iptd_s),
          "Y.1541 класс 5 IPTD не определён");
    checkNear(qosRequirement(QosClass::Class6).iplr, 1e-5, 1e-18,
              "Y.1541 класс 6 IPLR");
    checkNear(qosRequirement(QosClass::Class7).iptd_s, 0.400, 1e-12,
              "Y.1541 класс 7 IPTD");

    QosClass parsed;
    check(parseQosClass("class3", parsed) && parsed == QosClass::Class3,
          "Разбор класса QoS");

    // ---------------------------------------------------------------------
    // M/M/1
    // ---------------------------------------------------------------------
    QosParams qp;
    qp.packet_bits = 12000.0;
    qp.buffer_packets = 100;

    Edge edge;
    edge.capacity = 1.0e9;
    edge.base_latency = 0.001;
    edge.load = 0.5e9;
    const EdgeQos half = evaluateEdgeQos(edge, qp);
    const double mu = edge.capacity / qp.packet_bits;
    checkNear(half.delay_s, edge.base_latency + 2.0 / mu, 1e-9,
              "M/M/1 IPTD при ρ=0.5");
    check(std::abs(half.utilization - 0.5) < 1e-12, "M/M/1 ρ=0.5");

    edge.load = 0.9e9;
    const EdgeQos heavy = evaluateEdgeQos(edge, qp);
    check(heavy.delay_s > half.delay_s, "M/M/1 задержка растёт с нагрузкой");
    check(heavy.loss >= half.loss, "M/M/1 потери растут с нагрузкой");

    edge.load = 1.0e9;
    const EdgeQos saturated = evaluateEdgeQos(edge, qp);
    check(saturated.saturated, "M/M/1 насыщение при ρ=1");
    check(saturated.delay_s >= kSaturatedDelay, "M/M/1 задержка насыщения");

    // ---------------------------------------------------------------------
    // Топологии
    // ---------------------------------------------------------------------
    for (const auto& name : availableTopologies()) {
        TopologyParams tp;
        tp.topology = name;
        tp.num_nodes = 30;
        tp.seed = 123;
        tp.ba_m = 2;
        tp.er_p = 0.15;
        const Graph g = createNetworkTopology(tp);
        check(g.num_nodes == 30, name + ": число вершин");
        check(g.num_edges > 0, name + ": число дуг");
        check(computeLccRatio(g) > 0.999, name + ": связность");
        int degree_sum = 0;
        for (const auto& list : g.adj)
            degree_sum += static_cast<int>(list.size());
        check(g.num_edges == degree_sum, name + ": индекс дуг корректен");
    }

    // ---------------------------------------------------------------------
    // Структурный анализ ER/BA
    // ---------------------------------------------------------------------
    {
        Graph line5 = makeLine(5, 10.0e6, 0.001);
        TopologyParams line_params;
        line_params.topology = "line";
        line_params.num_nodes = 5;
        const TopologyStructure line_structure =
            analyzeTopology(line5, line_params);
        check(line_structure.components == 1, "Структура: линия связна");
        checkNear(line_structure.avg_path_length, 2.0, 1e-12,
                  "Структура: средняя длина пути линии");
        check(line_structure.diameter == 4, "Структура: диаметр линии");
        checkNear(line_structure.global_clustering, 0.0, 1e-12,
                  "Структура: кластеризация линии");

        Graph triangle = makeLine(3, 10.0e6, 0.001);
        triangle.addBidirectional(2, 0, 10.0e6, 0.001);
        triangle.rebuildEdgeIndex();
        const TopologyStructure triangle_structure =
            analyzeTopology(triangle, line_params);
        checkNear(triangle_structure.global_clustering, 1.0, 1e-12,
                  "Структура: кластеризация треугольника");
        check(triangle_structure.diameter == 1,
              "Структура: диаметр треугольника");
    }

    {
        TopologyParams er;
        er.topology = "erdos_renyi";
        er.num_nodes = 100;
        er.er_p = 0.05;
        er.seed = 17;
        const Graph er_graph = createNetworkTopology(er);
        const TopologyStructure er_structure =
            analyzeTopology(er_graph, er);
        check(std::abs(er_structure.avg_degree -
                       er_structure.expected_avg_degree) < 1.5,
              "ER: средняя степень близка к p(N−1)");
        check(er_structure.components == 1, "ER: граф связен");
        check(er_structure.degree_cv < 0.6,
              "ER: умеренный разброс степеней");

        TopologyParams ba;
        ba.topology = "barabasi_albert";
        ba.num_nodes = 100;
        ba.ba_m = 2;
        ba.seed = 17;
        const Graph ba_graph = createNetworkTopology(ba);
        const TopologyStructure ba_structure =
            analyzeTopology(ba_graph, ba);
        check(ba_structure.max_degree >= 8, "BA: есть хабы");
        check(ba_structure.degree_cv > 0.4,
              "BA: разброс степеней выше ER");
        check(ba_structure.gamma_est > 1.5 &&
                  ba_structure.gamma_est < 6.0,
              "BA: оценка γ в разумном диапазоне");
        check(std::abs(ba_structure.avg_degree -
                       ba_structure.expected_avg_degree) < 1.0,
              "BA: средняя степень близка к 2m");
    }

    // ---------------------------------------------------------------------
    // Трафик и метрики
    // ---------------------------------------------------------------------
    Graph line = makeLine(3, 10.0e6, 0.001);
    TrafficParams traffic;
    traffic.demand_per_pair_bps = 1.0e6;
    traffic.assignment_iterations = 3;
    traffic.preserve_total_demand = false;

    const TrafficAssignment assignment =
        assignTraffic(line, traffic, qp);
    check(assignment.routing.reachable_pairs == 6,
          "All-to-All: 6 достижимых пар");
    check(assignment.routing.pair[0][2].reachable,
          "All-to-All: путь 0→2 существует");
    check(line.adj[0][0].load > 1.5e6,
          "All-to-All: дуга 0→1 загружена двумя потоками");

    MetricsContext context;
    context.qos = qp;
    context.calibrate(assignment.routing);
    const NetworkMetrics healthy =
        computeMetrics(line, assignment.routing, context, 0.0);
    checkNear(healthy.lcc, 1.0, 1e-12, "Метрики: LCC здоровой сети");
    checkNear(healthy.phi_w, 1.0, 1e-12, "Метрики: φ_w здоровой сети");
    checkNear(healthy.e_qos_norm, 1.0, 1e-12,
              "Метрики: E_QoS_norm здоровой сети");

    line.nodes[2].state = 0;
    const TrafficAssignment broken =
        assignTraffic(line, traffic, qp);
    const NetworkMetrics degraded =
        computeMetrics(line, broken.routing, context, 0.0);
    checkNear(degraded.lcc, 2.0 / 3.0, 1e-12, "Метрики: LCC при отказе узла");
    checkNear(degraded.phi_w, 2.0 / 3.0, 1e-12,
              "Метрики: φ_w при отказе узла");
    check(degraded.e_qos_norm < 1.0, "Метрики: E_QoS падает при отказе");

    // Затор без отказов: 90 % загрузки, буфер 20 пакетов.
    TopologyParams mesh_params;
    mesh_params.topology = "full_mesh";
    mesh_params.num_nodes = 10;
    mesh_params.heterogeneous = false;
    mesh_params.access_capacity = 100.0e6;
    mesh_params.core_capacity = 100.0e6;
    mesh_params.base_latency = 0.001;
    mesh_params.latency_jitter = 0.0;
    Graph mesh = createNetworkTopology(mesh_params);

    TrafficParams heavy_traffic;
    heavy_traffic.demand_per_pair_bps = 90.0e6;
    heavy_traffic.assignment_iterations = 5;
    heavy_traffic.hop_penalty_s = 0.005;
    heavy_traffic.preserve_total_demand = false;
    QosParams loss_qos;
    loss_qos.buffer_packets = 20;

    const TrafficAssignment heavy_assignment =
        assignTraffic(mesh, heavy_traffic, loss_qos);
    double max_util = 0.0;
    for (const auto& list : mesh.adj)
        for (const auto& edge : list)
            max_util = std::max(max_util, edge.load / edge.capacity);
    check(max_util <= 1.01, "Затор: MSA устойчива, ρ ≤ 1");
    check(heavy_assignment.routing.conforming_pairs == 0,
          "Затор: класс 1 не выполняется по IPLR");

    MetricsContext loss_context;
    loss_context.qos = loss_qos;
    loss_context.calibrate(heavy_assignment.routing);
    const NetworkMetrics loss_metrics =
        computeMetrics(mesh, heavy_assignment.routing, loss_context, 0.0);
    check(loss_metrics.iplr_mean > 1.0e-3,
          "Затор: IPLR превышает порог 10⁻³");
    check(loss_metrics.e_qos_norm == 0.0,
          "Затор: E_QoS_norm = 0 при полном нарушении SLA");

    // ---------------------------------------------------------------------
    // Конфигурация
    // ---------------------------------------------------------------------
    std::filesystem::create_directories("test_output");
    SimConfig config;
    std::string error;
    check(config.applyOption("num_nodes", "25", error), "Конфиг: num_nodes");
    check(config.applyOption("qos_class", "3", error), "Конфиг: qos_class");
    check(config.applyOption("heterogeneous", "нет", error),
          "Конфиг: heterogeneous");
    check(!config.applyOption("qos_class", "9", error),
          "Конфиг: отклонение класса 9");

    config.out_dir = "test_output";
    check(config.saveToFile("test_output/test_config.ini", error),
          "Конфиг: сохранение");

    SimConfig loaded;
    check(loaded.loadFromFile("test_output/test_config.ini", error),
          "Конфиг: загрузка");
    check(loaded.num_nodes == 25 && loaded.qos_class == 3 &&
              !loaded.heterogeneous,
          "Конфиг: round-trip");

    // ---------------------------------------------------------------------
    // Дискретно-событийный движок
    // ---------------------------------------------------------------------
    SimConfig simulation;
    simulation.topology = "star";
    simulation.num_nodes = 12;
    simulation.T = 20.0;
    simulation.sample_points = 40;
    simulation.attack_lambda = 0.0;
    simulation.global_resources = 100.0;
    simulation.repair_slots = 2;
    simulation.seed = 5;
    simulation.qos_class = 1;

    const Graph star = createNetworkTopology(simulation.topologyParams());
    const SimResult quiet = runSimulation(star, simulation);
    check(quiet.attacks == 0, "DES: без атак нет событий атак");
    check(!quiet.had_failure, "DES: без атак нет отказов");
    checkNear(quiet.metrics.back().lcc, 1.0, 1e-12,
              "DES: сеть остаётся связной");

    simulation.attack_lambda = 5.0;
    simulation.T = 15.0;
    simulation.repair_slots = 0;
    simulation.global_resources = 0.0;
    const SimResult attacked = runSimulation(star, simulation);
    check(attacked.attacks > 0, "DES: атаки генерируются");
    check(attacked.had_failure, "DES: атаки вызывают отказы");
    check(attacked.recoveries_success == 0,
          "DES: без бригад нет восстановлений");

    simulation.attack_lambda = 1.0;
    simulation.T = 80.0;
    simulation.repair_slots = 4;
    simulation.global_resources = 1000.0;
    simulation.base_P_recovery = 1.0;
    simulation.base_mu = 1.0;
    const SimResult recovered = runSimulation(star, simulation);
    check(recovered.recoveries_success > 0,
          "DES: восстановление выполняется");

    const SimResult repeat = runSimulation(star, simulation);
    check(std::abs(recovered.metrics.back().lcc -
                   repeat.metrics.back().lcc) < 1e-12,
          "DES: воспроизводимость");

    // ---------------------------------------------------------------------
    // CSV
    // ---------------------------------------------------------------------
    exportTimeSeries(recovered, "test_output/timeseries_test.csv");
    check(std::filesystem::exists("test_output/timeseries_test.csv"),
          "CSV: файл временного ряда создан");
    std::ifstream csv("test_output/timeseries_test.csv");
    std::string header;
    std::getline(csv, header);
    check(header.find("e_qos_norm") != std::string::npos &&
              header.find("qos_ratio") != std::string::npos,
          "CSV: заголовок временного ряда");

    std::cout << "\nПроверок: " << g_checks
              << ", провалено: " << g_failures << "\n";
    return g_failures == 0 ? 0 : 1;
}
