#pragma once
// ---------------------------------------------------------------------------
// topology_metrics.h — структурный анализ графа сети.
//
// Модуль предназначен для содержательного сравнения топологий ER и BA:
// распределение степеней, коэффициент кластеризации, средняя длина пути,
// диаметр, ассортативность, число компонент и оценка показателя степенного
// закона по CCDF.
// ---------------------------------------------------------------------------

#include "topology.h"

#include <string>
#include <utility>
#include <vector>

namespace netsim {

struct TopologyStructure {
    std::string topology;
    std::string purpose;

    int num_nodes = 0;
    int num_edges = 0;

    double avg_degree = 0.0;
    int    min_degree = 0;
    int    max_degree = 0;
    double degree_cv = 0.0;

    double global_clustering = 0.0;
    double avg_path_length = 0.0;
    int    diameter = 0;
    double assortativity = 0.0;

    int    components = 0;
    double lcc_fraction = 0.0;

    double expected_avg_degree = 0.0;  // NaN, если модель не предполагает
    double ccdf_slope = 0.0;           // наклон ln P(K≥k) от ln k
    double gamma_est = 0.0;            // 1 − наклон, оценка показателя γ

    std::vector<std::pair<int, int>> degree_histogram;  // (степень, число узлов)
};

TopologyStructure analyzeTopology(const Graph& graph,
                                  const TopologyParams& params);

std::string topologyPurpose(const std::string& topology);

}  // namespace netsim
