#pragma once
// ---------------------------------------------------------------------------
// topology.h — направленный взвешенный граф информационной инфраструктуры
// и генераторы сложных сетевых топологий.
//
// Вершина хранит профиль узла (вероятности атаки/восстановления, стоимость
// ремонта, вес критичности). Дуга хранит физические параметры канала:
// пропускную способность (бит/с), базовую задержку распространения (с) и
// текущую загрузку (бит/с). Граф направленный: для дуплексного канала
// создаются две независимые дуги с собственными счётчиками загрузки.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace netsim {

// ---------------------------------------------------------------------------
// Профили типов узлов (значения по умолчанию для гетерогенной сети)
// ---------------------------------------------------------------------------
struct NodeProfile {
    double P_attack;    // вероятность успеха атаки на узел
    double P_recovery;  // вероятность успешного восстановления
    double mu;          // интенсивность восстановления, 1/ед. времени
    int    C_repair;    // стоимость ремонта, ед. ресурса
    double weight;      // вес критичности
};

extern const std::unordered_map<std::string, NodeProfile> NODE_PROFILES;
extern const std::unordered_map<std::string, double>      DEFAULT_NODE_MIX;

// ---------------------------------------------------------------------------
// Узел графа
// ---------------------------------------------------------------------------
struct Node {
    int         id = 0;
    int         state = 1;        // 0 = отказ, 1 = работает
    std::string node_type = "host";
    double      P_attack = 0.6;
    double      P_recovery = 0.8;
    double      mu = 0.2;
    int         C_repair = 1;
    double      weight = 1.0;
};

// ---------------------------------------------------------------------------
// Дуга графа
// ---------------------------------------------------------------------------
struct Edge {
    int    id = -1;             // сквозной индекс дуги (заполняется rebuildEdgeIndex)
    int    target = -1;         // конечная вершина
    double capacity = 1.0e9;    // пропускная способность, бит/с
    double base_latency = 0.001;// базовая задержка распространения, с
    double load = 0.0;          // текущая загрузка, бит/с
};

struct Graph {
    std::string              topology;
    int                      num_nodes = 0;
    int                      num_edges = 0;
    std::vector<Node>        nodes;
    std::vector<std::vector<Edge>> adj;

    int degree(int n) const { return static_cast<int>(adj[n].size()); }

    // Добавляет направленную дугу u → v.
    void addEdge(int u, int v, double cap = 1.0e9, double lat = 0.001) {
        adj[u].push_back(Edge{-1, v, cap, lat, 0.0});
        ++num_edges;
    }

    // Добавляет пару независимых дуг u → v и v → u (дуплексный канал).
    void addBidirectional(int u, int v, double cap = 1.0e9, double lat = 0.001) {
        addEdge(u, v, cap, lat);
        addEdge(v, u, cap, lat);
    }

    // Присваивает дугам сквозные индексы 0..E-1. Вызывается после генерации.
    void rebuildEdgeIndex() {
        int k = 0;
        for (auto& list : adj)
            for (auto& e : list) e.id = k++;
        num_edges = k;
    }

    int activeNodes() const {
        int n = 0;
        for (const auto& node : nodes) n += (node.state == 1) ? 1 : 0;
        return n;
    }

    double totalCapacity() const {
        double sum = 0.0;
        for (const auto& list : adj)
            for (const auto& e : list) sum += e.capacity;
        return sum;
    }

    double totalLoad() const {
        double sum = 0.0;
        for (const auto& list : adj)
            for (const auto& e : list) sum += e.load;
        return sum;
    }
};

// ---------------------------------------------------------------------------
// Параметры генерации топологии
// ---------------------------------------------------------------------------
struct TopologyParams {
    std::string topology = "barabasi_albert";
    int    num_nodes = 50;
    uint64_t seed = 42;

    // Модель Эрдёша–Реньи: вероятность существования ребра.
    double er_p = 0.10;
    // Модель Барабаши–Альберта: число связей нового узла.
    int    ba_m = 2;

    // Физические параметры каналов.
    double access_capacity = 1.0e9;   // доступ, бит/с
    double core_capacity   = 10.0e9;  // ядро/магистраль, бит/с
    double base_latency    = 0.002;   // с
    double latency_jitter  = 0.001;   // с (равномерное отклонение ±)

    // Профилирование узлов.
    bool   heterogeneous  = true;
    double base_P_attack  = 0.60;
    double base_P_recovery = 0.80;
    double base_mu        = 0.20;
    int    base_C_repair  = 1;
    double base_weight    = 1.0;
    std::unordered_map<std::string, double> node_mix;
};

// Генерация топологии. Гарантирует связность ER-графа (добавляет рёбра
// остовного дерева) и корректные степени BA-графа.
Graph createNetworkTopology(const TopologyParams& params);

std::vector<std::string> availableTopologies();
bool isKnownTopology(const std::string& name);

// Человекочитаемое описание топологии (для GUI/CLI).
std::string topologyDescription(const std::string& name);

}  // namespace netsim
