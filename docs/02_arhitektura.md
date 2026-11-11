# Архитектура программного комплекса

## 1. Общая схема

Комплекс разделён на независимое вычислительное ядро на C++ и визуальную
оболочку на Python. Такое разделение позволяет запускать тяжёлые
Монте-Карло-эксперименты без участия интерпретатора и использовать ядро как
в пакетном режиме, так и из GUI.

```text
┌─────────────────────────────────────────────────────────────┐
│                     Python / CustomTkinter                  │
│  gui/app.py        формы, навигация, карточки метрик         │
│  gui/core.py       поиск exe, INI, subprocess, progress      │
│  gui/data.py       чтение CSV, сводки                        │
│  gui/plots.py      Matplotlib: деградация, бары, sweep       │
│  gui/table.py      таблицы, копирование и экспорт CSV        │
└───────────────┬─────────────────────────────────────────────┘
                │  config.ini + --progress (stdout)
┌───────────────▼─────────────────────────────────────────────┐
│                        C++ core                             │
│  main.cpp        CLI / интерактивное меню / selftest        │
│  config.cpp      INI, валидация, производные параметры      │
│  topology.cpp    направленный граф, ER, BA, star, ring      │
│  topology_metrics.cpp  степени, CCDF, кластеризация, γ      │
│  traffic.cpp     All-to-All, Дейкстра, MSA, матрица путей   │
│  qos.cpp         Y.1541, M/M/1(K), проверка пути            │
│  metrics.cpp     E_phys, E_QoS, LCC, φ_w, IPTD/IPLR         │
│  engine.cpp      дискретно-событийный Монте-Карло           │
│  experiments.cpp статистика, CI, Манн–Уитни, Холм           │
│  csv_export.cpp  CSV-отчёты                                 │
└─────────────────────────────────────────────────────────────┘
```

## 2. Структуры данных

### 2.1 Граф

```cpp
struct Node {
    int id;
    int state;              // 0 — отказ, 1 — работает
    std::string node_type;  // server / switch / host
    double P_attack, P_recovery, mu;
    int C_repair;
    double weight;
};

struct Edge {
    int id;                 // сквозной индекс для массивов нагрузок
    int target;
    double capacity;        // бит/с
    double base_latency;    // с
    double load;            // бит/с
};

struct Graph {
    int num_nodes, num_edges;
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> adj;
};
```

Граф **направленный**: дуплексный канал представлен двумя дугами с
независимыми нагрузками. Это принципиально для All-to-All: поток `i → j`
нагружает только прямое направление.

### 2.2 Матрица путей

```cpp
struct PairState {
    double delay_s, loss, ipdv_s;
    bool reachable, saturated;
    bool iptd_ok, iplr_ok, ipdv_ok, conforms;
};

struct RoutingMatrix {
    int n, active_nodes, reachable_pairs, conforming_pairs;
    double sum_inv_delay, sum_inv_delay_qos;
    std::vector<std::vector<PairState>> pair;
};
```

`sum_inv_delay` и `sum_inv_delay_qos` — заранее агрегированные суммы
`Σ 1/IPTD`, из которых метрики вычисляются за O(1).

### 2.3 Метрики

```cpp
struct NetworkMetrics {
    double time;
    int active_nodes, active_edges;
    double lcc, phi_w, phi_active;
    double e_phys, e_phys_norm, e_qos, e_qos_norm;
    double qos_ratio, reachable_ratio, qos_reachable_ratio;
    double iptd_mean_ms, iptd_p95_ms, iplr_mean, ipdv_mean_ms;
    double avg_utilization, max_utilization, total_load_gbps;
    int reachable_pairs, conforming_pairs;
};
```

`iptd_mean_ms`, `iplr_mean`, `ipdv_mean_ms` считаются по достижимым путям без
перегруженных звеньев; пары с `saturated = true` исключаются, чтобы
штрафная задержка `10⁶ с` не искажала средние.

## 3. Алгоритм распределения трафика

```text
assignTraffic(G, traffic, qos):
    active ← работоспособные узлы
    demand ← traffic.demand_per_pair
    если preserve_total_demand:
        demand ← demand · N(N−1) / (A(A−1))
    load ← 0
    для k = 0 .. K−1:
        для каждой дуги: edge.load ← load
        new_load ← 0
        для src ∈ active:
            Дейкстра(src): стоимость = IPTD_e + hop_penalty
            для dst ∈ active, dst ≠ src:
                проследить путь dst → src по parent[]
                new_load[e] += demand
        load ← (1−α)load + α·new_load,  α = 1/(k+1)
    для каждой дуги: edge.load ← load
    построить RoutingMatrix на финальных нагрузках
```

Особенности:

- стоимость Дейкстры и фактическая IPTD хранятся раздельно: плата за
  транзит влияет на выбор пути, но не на метрику задержки;
- потери по пути накапливаются в логарифмическом виде
  `ln Π(1 − IPLR_e)`, что численно устойчиво;
- IPDV накапливается как сумма квадратов и затем извлекается корень;
- при `ρ ≥ 1` дуга получает штрафную задержку и потерю 1, но остаётся в
  графе — это позволяет отличить «нет маршрута» от «маршрут перегружен».

Сложность одной итерации — `O(N (V + E) log V + N²·L_path)`, где `L_path` —
средняя длина пути. Для `N = 50`, `E ≈ 200` и 3 итераций пересчёт занимает
доли миллисекунды.

## 4. Дискретно-событийный движок

### 4.1 События

```cpp
enum class EventType { Attack, RepairComplete };
struct Event { double time; EventType type; int node; uint64_t seq; };
```

Очередь — `std::priority_queue` с упорядочением по времени и seq. При
одинаковом времени события обрабатываются в порядке планирования, что
обеспечивает детерминизм.

### 4.2 Состояние

- `waiting` — очередь отказавших узлов, ожидающих бригаду;
- `in_repair` — узел занят бригадой;
- `used_slots` — число занятых бригад;
- `resources` — остаток глобального пула;
- `failure_time[node]` — время отказа (ключ FIFO/Triage);
- `dirty` — флаг изменения состояния для пересчёта трафика.

### 4.3 Цикл

```text
t0: калибровка E0 (assignTraffic + RoutingMatrix)
    запланировать первую атаку Exp(λ)
    для каждой точки сетки t:
        пока event.time ≤ t:
            обновить интегралы очереди/бригад
            обработать событие
        если dirty: пересчитать трафик и метрики
        записать метрики
```

Атака:

```text
attacks++
target ← выбрать по стратегии
если U(0,1) < P_attack[target]:
    state[target] = 0
    waiting.push(target)
    dispatch(now)          // попытаться занять бригаду
запланировать следующую атаку Exp(λ)
```

Завершение ремонта:

```text
used_slots--
если U(0,1) < P_recovery: state[node] = 1
иначе: waiting.push(node); failure_time[node] = now
dirty = true
dispatch(now)
```

`dispatch` выбирает узел, проверяет стоимость, списывает ресурсы, занимает
бригаду и планирует `RepairComplete` через `Exp(μ)`.

### 4.4 Интегралы

Средняя глубина очереди и среднее число занятых бригад вычисляются точно
по времени:

```text
queue_area += |waiting| · (t_event − t_prev)
slots_area += used_slots · (t_event − t_prev)
mean = area / T
```

Это устойчивее, чем усреднение по точкам сетки, когда события редки.

## 5. Конфигурация

Единый INI-файл используется CLI и GUI. Основные ключи:

| Ключ | Смысл | По умолчанию |
|---|---|---|
| `scenario` | `topology`, `sweep`, `strategy`, `attack`, `timeseries`, `structure`, `demo`, `all` | `topology` |
| `topologies` | список через запятую | все пять |
| `num_nodes` | число узлов | 50 |
| `n_sims` | число прогонов МК | 30 |
| `T` | горизонт | 150 |
| `attack_lambda` | интенсивность атак | 1.0 |
| `attack_strategy` | `targeted` / `random` / `cascading` | `targeted` |
| `gatekeeper` | `fifo` / `triage` | `fifo` |
| `repair_slots` | число бригад | 3 |
| `global_resources` | пул ресурсов | 200 |
| `qos_class` | класс Y.1541 | 1 |
| `demand_per_pair` | бит/с на пару | 2·10⁶ |
| `access_capacity`, `core_capacity` | ёмкости | 10⁹ / 10¹⁰ |
| `buffer_packets` | K в M/M/1/K | 100 |
| `hop_penalty` | плата за транзит, с | 0.005 |

Полный список — в `config.cpp` (`applyOption`).

## 6. CSV-отчёты

| Файл | Содержимое |
|---|---|
| `timeseries_<topo>.csv` | временные ряды LCC, φ_w, E_phys, E_QoS, Q, IPTD, IPLR |
| `topology_comparison.csv` | средние и 95% ДИ по топологиям |
| `topology_significance.csv` | попарные тесты по E_QoS и LCC |
| `sweep_<param>.csv` | зависимости метрик от λ, R, μ, P_attack |
| `strategy_comparison.csv` | FIFO/Triage, значимость по E_QoS и φ_w |
| `attack_vector_analysis.csv` | векторы атак × топологии |
| `run_meta_<topo>.csv` | E0, число атак, ремонтов, очередь |
| `topology_structure.csv` | структурные метрики всех топологий |
| `topology_degrees_<topo>.csv` | гистограмма степеней |
| `config.ini` | полная конфигурация прогона |

Все файлы пишутся в UTF-8 с заголовком; числа — с фиксированной точностью
6 знаков (кроме научных значений IPLR).

## 7. GUI

### 7.1 Слои

- `App` (наследник `CTk`) — представление и обработка событий;
- `CoreRunner` — запуск `netsim.exe` через `subprocess.Popen`, чтение stdout в
  отдельном потоке, очередь событий `queue.Queue`;
- `data` — чтение CSV в `pandas.DataFrame`, определение сценария, сводка;
- `plots` — построение `Figure` и встраивание через `FigureCanvasTkAgg`.

Поток выполнения:

```text
кнопка «Запустить»
  → собрать и валидировать параметры
  → создать runs/<timestamp>/config.ini
  → Popen(netsim.exe run --config ... --out ... --progress)
  → поток читает stdout → queue
  → Tk-цикл (after 80 мс) обновляет прогресс и журнал
  → код 0 → загрузить CSV → карточки и вкладки
```

### 7.2 Вкладки результатов

- **Динамика** — E_QoS, E_phys, LCC, Q, IPTD, IPLR во времени.
- **Топологии** — группированные бары E_QoS / Q / LCC / φ_w с ДИ.
- **Sweep** — кривые E_QoS от параметра для топологий.
- **Атаки** — E_QoS по векторам и топологиям.
- **FIFO/Triage** — переключение метрики E_QoS / φ_w, звёздочки значимых
  точек.
- **Сводка** — компактные таблицы ключевых метрик по типу прогона.
- **Структура** — таблица метрик ER/BA и CCDF распределения степеней.
- **Данные** — все CSV прогона в виде таблицы, копирование и экспорт.

### 7.3 Отказоустойчивость

- если `netsim.exe` не найден, GUI показывает путь сборки и не падает;
- ошибка валидации параметров — диалог с конкретным ключом;
- ошибка ядра — код возврата и журнал;
- остановка — `terminate()` процесса;
- отсутствие CSV — заглушка «Нет данных» вместо исключения.

## 8. Тестирование

- `tests/test_core.cpp` — 78 проверок: таблица Y.1541, формулы M/M/1,
  генераторы всех топологий, структурный анализ ER/BA, All-to-All, метрики,
  конфигурация, DES, воспроизводимость, CSV.
- `netsim.exe selftest` — быстрая проверка на встроенных данных.
- `run_gui.py --selftest` — сквозной прогон GUI: форма → ядро → CSV →
  график → снимок окна.

## 9. Ограничения и направления развития

1. M/M/1 не учитывает самоподобность трафика и корреляцию задержек;
   перспективно перейти к M/G/1 или сетевому исчислению.
2. Маршрутизация только по кратчайшему пути; нет политик BGP/OSPF,
   балансировки ECMP и DiffServ-приоритетов.
3. IPER и IPRR не моделируются; предполагается отсутствие битовых ошибок.
4. Восстановление описывается экспоненциальным временем; реальные SLA
   ремонта могут иметь детерминированные нормативы.
5. Triage учитывает только вес узла, но не его структурную роль; развитие —
   комбинированный приоритет (вес + центральность + фаза инцидента).
