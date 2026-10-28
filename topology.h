#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

// ---------------------------------------------------------------------------
// Профили типов узлов
// ---------------------------------------------------------------------------
struct NodeProfile {
    double P_attack;
    double P_recovery;
    double mu;
    int    C_repair;
    double weight;
};

extern const std::unordered_map<std::string, NodeProfile> NODE_PROFILES;
extern const std::unordered_map<std::string, double>      DEFAULT_NODE_MIX;

// ---------------------------------------------------------------------------
// Узел графа
// ---------------------------------------------------------------------------
struct Node {
    int         id;
    int         state;              // 0=отказ, 1=работает
    std::string node_type;
    double      P_attack;
    double      P_recovery;
    double      mu;
    int         C_repair;
    double      weight;
    double      recovery_end_time;  // -1=не в ремонте, inf=запрещён
};

// ---------------------------------------------------------------------------
// Структура ребра
// ---------------------------------------------------------------------------
struct Edge {
    int target;
    double capacity;      // Пропускная способность (бит/с)
    double base_latency;  // Базовая физическая задержка (с)
    double load;          // Текущая загрузка (бит/с) - можно динамически менять
};

struct Graph {
    std::string              topology;
    int                      num_nodes;
    std::vector<Node>        nodes;
    // Заменяем int на Edge
    std::vector<std::vector<Edge>> adj;

    int degree(int n) const { return (int)adj[n].size(); }

    // Обновляем метод addEdge
    void addEdge(int u, int v, double cap = 1e9, double lat = 0.001) {
        adj[u].push_back({ v, cap, lat, 0.0 });
        adj[v].push_back({ u, cap, lat, 0.0 });
    }
};

// ---------------------------------------------------------------------------
// Функция создания топологии
// ---------------------------------------------------------------------------
Graph createNetworkTopology(
    const std::string& topology_type,
    int                num_nodes,
    double             base_P_attack = 0.60,
    double             base_P_recovery = 0.80,
    double             base_mu = 0.20,
    int                base_C_repair = 1,
    double             base_weight = 1.0,
    bool               heterogeneous = false,
    const std::unordered_map<std::string, double>& node_mix = {},
    uint64_t           seed = 0
);