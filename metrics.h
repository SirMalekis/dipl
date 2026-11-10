#pragma once
// ---------------------------------------------------------------------------
// metrics.h — метрики живучести и качества обслуживания.
//
// Помимо классических LCC и взвешенной живучести φ_w, модуль вычисляет
// физическую эффективность E_phys (нормированную глобальную эффективность
// через обратные задержки M/M/1) и QoS-эффективность E_QoS, в которой
// учитываются только пары, прошедшие проверку по порогам Y.1541.
// ---------------------------------------------------------------------------

#include "qos.h"
#include "topology.h"
#include "traffic.h"

namespace netsim {

struct MetricsContext {
    QosParams qos;
    double e0_phys = 1.0;   // E_phys полностью исправной сети
    double e0_qos = 1.0;    // E_QoS полностью исправной сети
    int    baseline_reachable_pairs = 0;
    int    baseline_conforming_pairs = 0;
    bool   baseline_qos_ok = false;

    void calibrate(const RoutingMatrix& routing);
};

struct NetworkMetrics {
    double time = 0.0;

    int active_nodes = 0;
    int active_edges = 0;

    double lcc = 0.0;        // |LCC| / N
    double phi_w = 0.0;      // взвешенная живучесть по LCC (формула 2.3 НИР)
    double phi_active = 0.0; // доля веса работоспособных узлов

    double e_phys = 0.0;       // абсолютная физическая эффективность, 1/с
    double e_phys_norm = 0.0;  // E_phys(t) / E_phys(0)
    double e_qos = 0.0;        // QoS-эффективность, 1/с
    double e_qos_norm = 0.0;   // E_QoS(t) / E_QoS(0)
    double qos_ratio = 0.0;    // доля пар, прошедших QoS, от N(N−1)
    double reachable_ratio = 0.0;      // доля достижимых пар от N(N−1)
    double qos_reachable_ratio = 0.0;  // доля прошедших QoS среди достижимых

    double iptd_mean_ms = 0.0;
    double iptd_p95_ms = 0.0;
    double iplr_mean = 0.0;
    double ipdv_mean_ms = 0.0;

    double avg_utilization = 0.0;
    double max_utilization = 0.0;
    double total_load_gbps = 0.0;

    int reachable_pairs = 0;
    int conforming_pairs = 0;
};

NetworkMetrics computeMetrics(const Graph& graph,
                              const RoutingMatrix& routing,
                              const MetricsContext& context,
                              double time);

// Классические метрики (экспортируются отдельно для тестов).
double computeLccRatio(const Graph& graph);
double computeWeightedSurvivability(const Graph& graph);
double computeActiveWeightRatio(const Graph& graph);

}  // namespace netsim
