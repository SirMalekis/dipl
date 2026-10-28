#pragma once
#include "topology.h"

// φ_LCC = |LCC_t| / N
double computeLccRatio(const Graph& G, int N_total);

// Абсолютная E_glob на подграфе активных узлов
double computeGlobalEfficiency(const Graph& G);

// E_norm = E_glob(t) / E_glob(0)
double computeNormalizedEfficiency(const Graph& G, double E0);

// φ_w = Σ(w_i·z_i) / Σw_i
double computeWeightedSurvivability(const Graph& G);

// E_glob(t=0) — вызывается один раз до симуляции
double computeInitialEfficiency(const Graph& G);

// Перерасчет трафика между всеми живыми узлами
void recalculateTraffic(Graph& G, double traffic_per_pair = 500000.0);