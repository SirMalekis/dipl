#pragma once
// ---------------------------------------------------------------------------
// traffic.h — модуль перерасчёта трафика All-to-All и оценки путей.
//
// Реализует итеративное равновесие по загрузке: на каждой итерации
// кратчайшие пути строятся алгоритмом Дейкстры с весами, зависящими от
// текущей загрузки каналов (M/M/1), затем нагрузки пересчитываются методом
// последовательных средних (MSA). Это позволяет моделировать перегрузку
// (congestion) при выходе узлов из строя, когда трафик перераспределяется
// на оставшиеся каналы.
// ---------------------------------------------------------------------------

#include "qos.h"
#include "topology.h"

#include <vector>

namespace netsim {

struct TrafficParams {
    double demand_per_pair_bps = 2.0e6;  // интенсивность пары «источник–получатель»
    int    assignment_iterations = 3;       // число итераций MSA
    // Плата за транзит (с): добавка к стоимости каждого промежуточного узла.
    // Подавляет нефизичные обходные пути при насыщении каналов и стабилизирует MSA.
    double hop_penalty_s = 0.005;
    // Сохранять суммарную нагрузку сети при отказах: интенсивность пары
    // масштабируется как N(N−1)/(A(A−1)), где A — число активных узлов.
    // Это моделирует перераспределение трафика выбывших узлов на выжившие.
    bool   preserve_total_demand = true;
};

// Результат оценки одного направленного пути.
struct PairState {
    double delay_s = kUndefined;
    double loss = 1.0;
    double ipdv_s = kUndefined;
    bool   reachable = false;
    bool   saturated = false;  // путь содержит перегруженное звено (ρ ≥ 1)
    bool   iptd_ok = false;
    bool   iplr_ok = false;
    bool   ipdv_ok = false;
    bool   conforms = false;  // путь удовлетворяет требованиям класса QoS
};

// Матрица «источник → получатель» для всех пар узлов.
struct RoutingMatrix {
    int n = 0;
    int active_nodes = 0;
    int reachable_pairs = 0;   // число достижимых упорядоченных пар (i≠j)
    int conforming_pairs = 0;  // число пар, прошедших проверку QoS
    double sum_inv_delay = 0.0;      // Σ 1/IPTD по достижимым парам
    double sum_inv_delay_qos = 0.0;  // Σ 1/IPTD по парам, прошедшим QoS

    std::vector<std::vector<PairState>> pair;  // [src][dst]
};

struct TrafficAssignment {
    RoutingMatrix routing;
    double total_demand_bps = 0.0;
    int    iterations = 0;
};

// Пересчитывает загрузку дуг графа и возвращает матрицу путей.
// Граф модифицируется: поле Edge::load заполняется результатом MSA.
TrafficAssignment assignTraffic(Graph& graph,
                                const TrafficParams& traffic,
                                const QosParams& qos);

}  // namespace netsim
