# Технический аудит программного комплекса NetSim

**Версия ПО:** `2.0.0` (`main.cpp:41`)  
**Аудит выполнен по состоянию репозитория:** commit `58f8db0`  
**Рабочий каталог:** `D:\ddipl`  
**Предмет:** C++-ядро, Python/CustomTkinter GUI, документация `docs/`, результаты `results/experiments/`.

Аудит выполнен чтением исходников, конфигураций, CSV и документации, а также
контрольными запусками `netsim.exe selftest`, `tests/test_core.exe` и
`netsim.exe run` без флага `--progress`. В рамках аудита код не изменялся.
Все числа ниже взяты из файлов CSV и кода; доверительные интервалы — это
полуширина 95 % ДИ Стьюдента (`ci` в CSV), если не указано иное.

---

## 0. Краткие выводы аудита

1. Ядро реализует направленный граф, M/M/1(K), Y.1541-проверку пути,
   All-to-All с Дейкстрой и MSA, дискретно-событийные атаки/ремонт и
   Монте-Карло со статистикой. Архитектурно проект целостен.
2. **Критический дефект:** при запуске CLI без `--progress` вызывается пустой
   `std::function` и процесс завершается с `bad function call` (код 1).
   Это воспроизведено на `--scenario structure`; логика затрагивает все
   сценарии `runScenario`.
3. **Существенное ограничение модели:** при `heterogeneous=1` (значение по
   умолчанию) параметры `base_P_recovery`, `base_C_repair`, `base_weight`
   не применяются: используются фиксированные профили `server/switch/host`.
4. **E_QoS в базовой временной серии численно совпадает с E_phys:** во всех
   300 × 5 точках `base_timeseries` разность `e_phys_norm − e_qos_norm`
   равна 0. В `base_topology` `eff_mean == eff_qos_mean` для BA, full_mesh,
   ring и star; у ER есть малое различие: 0,406025 против 0,405890
   (1,35·10⁻⁴). Причина — в базовой временной серии все достижимые пути
   проходят класс 1 (`qos_reachable_ratio = 1`), поэтому QoS-фильтр не
   отсекает ни одной достижимой пары. В 30-прогонном сравнении ER
   небольшое число пар уже не проходит фильтр, но эффект остаётся
   пренебрежимо малым. Это не ошибка формулы, но базовая серия
   экспериментов почти не проверяет QoS-фильтр как отдельный механизм.
5. IPER/IPRR в таблице Y.1541 присутствуют, но **не проверяются**; класс QoS
   фиксирован на прогон, автоматических переходов класса нет.
6. Атакуются только узлы; рёбра не имеют состояния отказа и не атакуются.
   Triage использует статический вес узла, а не загрузку/центральность.
7. В GUI **нет визуализации графа** (node-link): «Топологии» — это bar chart,
   «Структура» — таблица + CCDF; состояния узлов/рёбер на графе не
   отображаются.
8. Документация в целом подробная, но есть расхождения с кодом: имя каталога
   прогона, назначение кнопки «Таблицы», список вкладок `--tab`, остаточные
   «НИР» в `docs/01_teoriya.md`, а README приписывает проверку IPER.
9. Все 7 экспериментальных конфигураций используют `qos_class=1`; распределения
   по классам QoS в результатах нет.
10. Статистика: 95 % ДИ и U-критерий Манна–Уитни с поправкой Холма для
    топологий; для strategy p-значения не проходят поправку на множественность,
    для attack статистических тестов нет; U-тест без поправки на связи.

---

## 1. Архитектура и стек проекта

### 1.1 Дерево проекта

```text
D:\ddipl
├── main.cpp                  CLI, интерактивное меню, selftest, запуск сценариев
├── config.h / config.cpp     SimConfig, INI, CLI-опции, валидация, sweep-диапазоны
├── topology.h / topology.cpp направленный граф, Node/Edge/Graph, star/ring/full_mesh/ER/BA
├── topology_metrics.h/.cpp   степени, CCDF, кластеризация, L, D, r, компоненты, γ
├── qos.h / qos.cpp           Y.1541 0–7, M/M/1(K), EdgeQos/PathQos, проверка пути
├── traffic.h / traffic.cpp   All-to-All, Дейкстра, MSA, RoutingMatrix/PairState
├── metrics.h / metrics.cpp   LCC, φ_w, E_phys, E_QoS, Q, R, Q|R, IPTD/IPLR/IPDV
├── engine.h / engine.cpp     дискретно-событийный Монте-Карло, атаки, ремонт
├── experiments.h/.cpp        Монте-Карло, CI, U-критерий, Холм, сценарии
├── csv_export.h/.cpp         экспорт timeseries/topology/sweep/strategy/attack/meta/structure
├── build.ps1                 поиск MSBuild и сборка netsim.vcxproj
├── netsim.vcxproj            MSVC-проект, C++20, Win32/x64, Debug/Release
├── netsim.slnx               решение Visual Studio
├── tests/build_tests.ps1     сборка тестов через cl /std:c++20
├── tests/test_core.cpp       78 проверок ядра
├── run_gui.py                точка входа GUI, --selftest/--open-run/--tab
├── plot_results.py           автономное построение PNG по CSV
├── gui/
│   ├── app.py                App(CTk), страницы, вкладки, запуск, графики, таблицы
│   ├── core.py               поиск netsim.exe, INI, CoreRunner(subprocess), progress
│   ├── data.py               чтение CSV, список прогонов, сводка
│   ├── plots.py              Matplotlib: timeseries, bars, sweep, CCDF, strategy
│   ├── table.py              DataTable: Treeview, копирование TSV, экспорт CSV
│   └── summary.py            компактные сводные таблицы по сценариям
├── docs/
│   ├── 01_teoriya.md         теория, Y.1541, M/M/1, E_QoS, DES, статистика
│   ├── 02_arhitektura.md     архитектура, структуры, CSV, GUI, ограничения
│   ├── 03_rukovodstvo.md     руководство пользователя
│   ├── 04_rezultaty.md       результаты и выводы
│   ├── 05_topolologii.md     ER/BA, структурные метрики
│   ├── 06_zapuski_i_dannye.md имена прогонов, сценарии, таблицы
│   ├── Teoreticheskoe_obosnovanie.docx/.pdf
│   └── Topologii_ER_BA.docx/.pdf
├── results/
│   ├── experiments/          7 архивных прогонов, CSV и PNG
│   ├── last_config.ini       последняя конфигурация для --quick
│   ├── topology_comparison.csv  устаревший CSV со старой схемой
│   └── plot_topology.png     устаревший PNG
└── scripts/
    ├── run_experiments.ps1   пакетный запуск 7 экспериментов
    ├── make_stage1_docx.py   генерация Teoreticheskoe_obosnovanie.docx
    └── make_topologies_docx.py генерация Topologii_ER_BA.docx
```

### 1.2 C++ и сборка

- Стандарт: **C++20**. Во всех конфигурациях `netsim.vcxproj` задано
  `<LanguageStandard>stdcpp20</LanguageStandard>` (строки 79, 97, 113, 131).
- Компилятор/IDE в текущем окружении: **MSVC 19.51.36248 x64**,
  Visual Studio Community 2026, MSBuild **18.7.11911.148**.
- Toolset в проекте: **v145**; Windows SDK: `10.0`.
- Конфигурации: `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`.
- `build.ps1` ищет MSBuild через `vswhere`, вызывает:
  `msbuild netsim.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:minimal`
  и ожидает `x64\Release\netsim.exe`.
- Внешних C++-библиотек нет: используются STL (`std::vector`,
  `std::priority_queue`, `std::mt19937_64`, `std::filesystem`,
  `std::exponential_distribution` и т. д.).
- Тесты собираются отдельно:
  `cl /nologo /std:c++20 /EHsc /O2 /I. ... /Fe:tests\test_core.exe`.
  Заявлено и подтверждено **78 проверок, 0 провалов**.
- **Несоответствие:** README/руководство допускают Visual Studio 2022,
  но проект жёстко задаёт toolset `v145` (VS 2026). Для VS 2022 нужен
  `v143` или перенастройка toolset.

### 1.3 Python-стек

Фактическое окружение аудита:

| Компонент | Версия/роль |
|---|---|
| Python | 3.14.4 |
| customtkinter | 6.0.0 — GUI |
| matplotlib | 3.10.8, backend `TkAgg` — графики |
| pandas | 3.0.3 — чтение CSV и таблицы |
| numpy | 2.4.4 — массивы, CCDF, loglog |
| Pillow | 12.2.0 — `ImageGrab` для скриншотов |
| tkinter | стандартная библиотека — Treeview/диалоги |
| python-docx | используется скриптами генерации DOCX, не GUI |
| LibreOffice Kit | конвертация DOCX → PDF в скриптах |

`requirements.txt`/lock-файла нет; README перечисляет только
`customtkinter`, `matplotlib`, `pandas`, `Pillow`, без фиксации версий.

### 1.4 Интеграция GUI и C++-ядра

Интеграция — **через файлы и отдельный процесс**, без общей памяти и без
встраивания C++ в Python.

```text
GUI (App)
  → _collect_config() формирует dict
  → core.write_config(config, run_dir/config.ini)
  → CoreRunner.start([
        netsim.exe, "run",
        "--config", run_dir/config.ini,
        "--out", run_dir,
        "--progress"
    ], cwd=ROOT)
  → subprocess.Popen(stdout=PIPE, stderr=STDOUT, text=True,
                     encoding="utf-8", bufsize=1,
                     creationflags=CREATE_NO_WINDOW)
  → отдельный поток читает stdout построчно
  → queue.Queue: ("line", text) / ("done", code)
  → Tk-цикл after(80 ms) обновляет журнал и прогресс
  → код 0 → _refresh_runs() → _load_run(run_dir)
  → pandas.read_csv(...) → карточки, вкладки, таблицы
```

- Параметры передаются **только через INI-файл**; CLI-аргументы задают
  `--config`, `--out`, `--progress`.
- Результаты передаются **только через CSV-файлы** в каталоге прогона.
- `PROGRESS_RE = ^PROGRESS\s+([0-9]*\.?[0-9]+)\s+(.*)$`; ядро печатает
  `PROGRESS <fraction> <stage>` (`main.cpp:72–76`). GUI обновляет
  `CTkProgressBar` и статус, но **не строит промежуточные графики**.
- Мониторинг — потоковый, по строкам stdout; **пошагового режима, паузы,
  точки останова и интерактивного вмешательства нет**. Остановка —
  `process.terminate()`.
- `stderr` объединён с `stdout`; ошибки ядра видны в журнале.
- `NETSIM_EXE` позволяет указать путь к `netsim.exe`; иначе поиск идёт по
  `x64/Release`, `x64/Debug`, `build/Release`, `build`, `bin`.

---

## 2. Математическая модель узлов, рёбер и динамических весов

### 2.1 Точные структуры данных

`topology.h`:

```cpp
struct NodeProfile {
    double P_attack;    // вероятность успеха атаки на узел
    double P_recovery;  // вероятность успешного восстановления
    double mu;          // интенсивность восстановления, 1/ед. времени
    int    C_repair;    // стоимость ремонта, ед. ресурса
    double weight;      // вес критичности
};

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

struct Edge {
    int    id = -1;             // сквозной индекс дуги
    int    target = -1;         // конечная вершина
    double capacity = 1.0e9;    // бит/с
    double base_latency = 0.001;// базовая задержка, с
    double load = 0.0;          // текущая загрузка, бит/с
};

struct Graph {
    std::string topology;
    int num_nodes = 0;
    int num_edges = 0;
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> adj;
};
```

`Graph` — **список смежности направленного графа**. Дуплексный канал
представлен двумя независимыми дугами `u→v` и `v→u` с отдельными `load`.
`Graph::num_edges` считает **дуги**, а `TopologyStructure::num_edges` —
**уникальные неориентированные рёбра**; для звезды это 98 дуг против 49 рёбер
в `topology_structure.csv`.

`NodeProfile`/`DEFAULT_NODE_MIX` (`topology.cpp:12–22`):

| Тип | P_attack | P_recovery | μ | C_repair | weight | Доля в миксе |
|---|---:|---:|---:|---:|---:|---:|
| server | 0,55 | 0,85 | 0,15 | 3 | 10 | 0,15 |
| switch | 0,65 | 0,90 | 0,30 | 2 | 5 | 0,20 |
| host | 0,70 | 0,75 | 0,20 | 1 | 1 | 0,65 |

`TopologyParams` по умолчанию: `topology="barabasi_albert"`, `N=50`,
`seed=42`, `er_p=0,10`, `ba_m=2`, `access_capacity=1,0·10⁹`, `core_capacity=10·10⁹`,
`base_latency=0,002 с`, `latency_jitter=0,001 с`, `heterogeneous=true`,
`base_P_attack=0,60`, `base_P_recovery=0,80`, `base_mu=0,20`,
`base_C_repair=1`, `base_weight=1,0`.

`SimConfig` (ядро) по умолчанию: `scenario="topology"`,
`topology="barabasi_albert"`, `topologies={star,ring,full_mesh,erdos_renyi,barabasi_albert}`,
`N=50`, `n_sims=30`, `T=150`, `sample_points=200`, `seed=42`,
`attack_lambda=1,0`, `attack_strategy="targeted"`, `P_attack=0,60`,
`gatekeeper="fifo"`, `repair_slots=3`, `R=200`, `P_recovery=0,80`,
`μ=0,20`, `C_repair=1`, `weight=1`, `heterogeneous=true`,
`er_p=0,10`, `ba_m=2`, `access=1,0·10⁹`, `core=10·10⁹`,
`base_latency=0,002`, `jitter=0,001`, `qos_class=1`, `packet_bits=12000`,
`buffer=100`, `demand=2,0·10⁶`, `traffic_iterations=3`, `hop_penalty=0,005`,
`preserve_total_demand=true`.
GUI-дефолт отличается: `n_sims=20` (`gui/core.py:45`) против `30` в ядре.

### 2.2 Характеристики рёбер и значения по умолчанию

У ребра есть только три физических параметра: `capacity` (бит/с),
`base_latency` (с), `load` (бит/с). **Нет** вероятности отказа ребра,
приоритета, отдельной очереди, состояния `up/down`, стоимости или типа
канала. Все отказы — только узловые (`Node::state`).

Базовые значения:

- `Edge::capacity = 1,0·10⁹` бит/с, `base_latency = 0,001` с в структуре;
- `TopologyParams` задаёт `access=1,0·10⁹`, `core=10,0·10⁹`,
  `base_latency=0,002` с и `jitter=0,001` с;
- в генераторе задержка ребра выбирается один раз:
  `lat(u,v) = max(1e-6, base_latency + U(−jitter, +jitter))`;
- для `star`, `ring`, `full_mesh` **все** рёбра получают `core_capacity`
  (10 Гбит/с); `access_capacity` для этих трёх топологий не используется;
- для `erdos_renyi`/`barabasi_albert` после генерации вызывается
  `assignCapacitiesByDegree`:
  `min(deg(u),deg(v)) ≥ 1,10·⟨k⟩` → `core_capacity`, иначе `access_capacity`.
- Пропускная способность не зависит от типа узла (server/switch/host),
  только от степеней концов ребра.

### 2.3 Динамические веса и M/M/1(K)

Для каждой дуги при текущей загрузке `load` (бит/с):

```text
L  = packet_bits = 12 000 бит (1500 байт по умолчанию)
μ  = C / L                      [пакетов/с]
λ  = load / L                   [пакетов/с]
ρ  = λ / μ = load / C
```

Условие устойчивости: `ρ < 1` (в коде — `ρ < 1 − 10⁻¹²`).
При `C ≤ 0` или `ρ ≥ 1 − 10⁻¹²` дуга помечается `saturated`:
`delay_s = kSaturatedDelay = 1,0·10⁶ с`, `loss = 1`, `ipdv_s = 0`.

Иначе (`qos.cpp:132–184`):

```text
IPTD_e = base_latency + 1/(μ − λ)
IPLR_e = (1 − ρ)·ρ^K / (1 − ρ^(K+1)),   ρ ≠ 1
IPLR_e = 1/(K + 1),                     ρ = 1
IPDV_e = −ln(10⁻³)/(μ − λ) = 6,907755…/(μ − λ)
```

где `K = max(1, buffer_packets)`, по умолчанию `K=100`.
`IPDV_e` — верхняя граница квантиля `1−10⁻³`; базовая задержка и её
jitter в IPDV не входят, учитывается только очередь M/M/1.

Пересчёт веса выполняется:

- при каждом рассмотрении ребра в `runDijkstra` — `evaluateEdgeQos(edge,qos)`;
- на каждой итерации MSA после обновления `edge.load` — при следующей
  Дейкстре;
- в `computeMetrics` — не пересчитывается, используются `PairState` из
  `assignTraffic`.

Узловые веса `weight` **статичны**; динамических весов узлов нет.

### 2.4 Маршрутизация, отказы и перераспределение

`assignTraffic` (`traffic.cpp:84–210`):

1. Обнуляет `edge.load`.
2. Формирует список активных узлов (`state==1`).
3. Если активных `<2`, возвращает пустую матрицу.
4. При `preserve_total_demand`:
   `b_A = b · N(N−1) / (A(A−1))`, где `A` — число активных узлов.
   Суммарный спрос сохраняется: `A(A−1)·b_A = N(N−1)·b`.
5. Для `k=0..K−1` (`K = traffic_iterations`, по умолчанию 3):
   - `edge.load = load[id]` (начальные нули);
   - для каждого активного `src` — Дейкстра;
   - для каждого `dst` — весь спрос `b_A` добавляется на все рёбра пути
     `dst→src` по `parent_edge`;
   - MSA: `load_e ← (1−α)·load_e + α·new_load_e`,
     `α = 1/(k+1)`; при `k=0` α=1, затем 0,5, 0,333.
6. На финальных нагрузках снова запускается Дейкстра и строится
   `RoutingMatrix`.

Стоимость Дейкстры:

```text
c_e = IPTD_e(load_e) + hop_penalty
```

`hop_penalty` по умолчанию **0,005 с** (`5 мс`). В коде он добавляется
**на каждое ребро пути**, а не только на промежуточный узел; фактическая
IPTD пути считается без `hop_penalty`. Это расхождение с формулировкой
`docs/01` («за каждый промежуточный узел») и с `TrafficParams`-комментарием.

Агрегация пути:

```text
IPTD_path = Σ_e IPTD_e
IPLR_path = 1 − Π_e (1 − IPLR_e)
IPDV_path = sqrt(Σ_e IPDV_e²)
```

`PairState.saturated = (actual_delay ≥ kSaturatedDelay·0,5)`, то есть путь
считается перегруженным, если его фактическая задержка ≥ 5·10⁵ с.
`reachable_pairs` включает такие пути; `conforming_pairs` — только пути,
прошедшие все определённые пороги класса.

При отказе узла:

- узел получает `state=0`;
- `assignTraffic` исключает его из `active`;
- все входящие/исходящие дуги фактически не используются;
- на следующей точке сетки выполняется полный пересчёт трафика
  (не инкрементальный BFS/локальная перемаршрутизация);
- отдельного отказа рёбер нет, поэтому «ремаршрутизация при отказе ребра»
  в модели не реализована.

Метрики после отказа пересчитываются только при `dirty=true` (после
`registerFailure` или завершения ремонта). Если событий между точками
сетки нет, `current_metrics.time` обновляется, а значения метрик остаются
прежними.

### 2.5 Метрики и нормировка

`metrics.cpp`:

```text
LCC(t)   = |V_LCC(t)| / N
φ_w(t)   = Σ_{i∈V_LCC} w_i / Σ_{i∈V} w_i
E_phys(t)= 1/(N(N−1)) · Σ_{i≠j} 1/IPTD_ij(t)
E_QoS(t) = 1/(N(N−1)) · Σ_{i≠j} (1/IPTD_ij(t))·I_ij(t)
Q(t)     = conforming_pairs / (N(N−1))
R(t)     = reachable_pairs / (N(N−1))
Q|R(t)   = conforming_pairs / reachable_pairs
E_phys_norm = min(1, E_phys/E_phys(0))
E_QoS_norm  = min(1, E_QoS/E_QoS(0))
```

`I_ij=1`, если путь достижим и `IPTD≤IPTD_K`, `IPLR≤IPLR_K`,
`IPDV≤IPDV_K` (для определённых требований класса). `IPER` в `I_ij`
не входит.

`MetricsContext::calibrate` использует **N(N−1)**, а не число активных пар,
поэтому `E_phys(0)`, `E_QoS(0)` нормируются на полный набор пар.

`computeMetrics`:

- `iptd_mean_ms`, `iplr_mean`, `ipdv_mean_ms` — только по достижимым
  путям с `saturated=false`;
- `iptd_p95_ms` — 95-й перцентиль этих задержек;
- `avg_utilization`/`max_utilization` — по активным **направленным** дугам,
  `ρ = load/capacity`;
- `total_load_gbps = Σ load / 10⁹`;
- `active_edges` — число направленных активных дуг.

---

## 3. Стандарты и классы QoS

### 3.1 Какие стандарты реально заложены

- **ITU-T Y.1541 (02/2006)** — основная таблица классов 0–7, пороги
  IPTD/IPDV/IPLR/IPER. Реализована в `qos.cpp:14–32`.
- **ITU-T Y.1540** — только упомянута в литературе `docs/01` как источник
  показателей IPTD/IPLR; отдельного модуля/констант Y.1540 нет.
- **G.1010** — в коде и документации **не реализована**.
- **DiffServ** — **не реализован**; упоминается только как направление
  развития в `docs/02` (ограничение: нет DiffServ-приоритетов).
- **IPER/IPRR** — присутствуют в таблице и в тексте, но не проверяются;
  `PathQos` не содержит полей IPER/IPRR.

### 3.2 Таблица классов из кода

`QosRequirement kRequirements[8]` (`qos.cpp:14–32`), значения в секундах
и вероятностях:

| Класс | IPTD, с (мс) | IPDV, с (мс) | IPLR | IPER | Условный | Приложения (кратко) |
|---:|---:|---:|---:|---:|:---:|---|
| 0 | 0,100 (100) | 0,050 (50) | 1,0·10⁻³ | 1,0·10⁻⁴ | нет | VoIP, видеоконференции |
| 1 | 0,400 (400) | 0,050 (50) | 1,0·10⁻³ | 1,0·10⁻⁴ | нет | интерактивные приложения |
| 2 | 0,100 (100) | не определено (`∞`) | 1,0·10⁻³ | 1,0·10⁻⁴ | нет | сигнализация, транзакции |
| 3 | 0,400 (400) | не определено | 1,0·10⁻³ | 1,0·10⁻⁴ | нет | транзакционные данные |
| 4 | 1,000 (1000) | не определено | 1,0·10⁻³ | 1,0·10⁻⁴ | нет | массовая передача данных |
| 5 | не определено | не определено | не определено | не определено | нет | best effort |
| 6 | 0,100 (100) | 0,050 (50) | 1,0·10⁻⁵ | 1,0·10⁻⁶ | да | цифровое ТВ |
| 7 | 0,400 (400) | 0,050 (50) | 1,0·10⁻⁵ | 1,0·10⁻⁶ | да | цифровое ТВ |

`kUndefined = std::numeric_limits<double>::infinity()`.
Для класса 5 все требования не определены, поэтому **любой достижимый
путь** считается conforming (`evaluatePathQos`, `qos.cpp:194–198`).

### 3.3 Проверка пути

```cpp
out.iptd_ok = !isDefined(req.iptd_s) || (reachable && delay_s <= req.iptd_s);
out.iplr_ok = !isDefined(req.iplr)   || (reachable && loss <= req.iplr);
out.ipdv_ok = !isDefined(req.ipdv_s) || (reachable && ipdv_s <= req.ipdv_s);
out.conforms = reachable && out.iptd_ok && out.iplr_ok && out.ipdv_ok;
```

`IPER` не проверяется; `IPLR` и `IPDV` проверяются. Для класса 5
`conforms = reachable`.

### 3.4 Правила перехода сети из класса в класс

**В реализации отсутствуют.** `qos_class` — входной параметр `SimConfig`,
фиксированный на весь прогон; `NetworkMetrics` не хранит «класс сети»;
нет функции, которая повышает/понижает класс при деградации. Проверяется
только соответствие каждого пути порогам выбранного класса. Следовательно,
фразы «сеть перешла из класса 1 в класс 3» в коде не соответствуют никакой
логике; можно лишь сказать «путь перестал удовлетворять классу 1».

### 3.5 Классы в экспериментах

Во всех семи `results/experiments/*/config.ini`:

```text
qos_class=1
```

Ни один прогон не использует классы 0, 2–7; распределения пар по классам
QoS в CSV нет. `run_meta_<topo>.csv` содержит `qos_requirement` только для
выбранного класса. `topology_comparison.csv`/`sweep_*.csv` не содержат
per-class breakdown. Это ограничение экспериментальной программы, а не
только кода.

---

## 4. Генерация многосвязных топологий

### 4.1 Полный список

`availableTopologies()` (`topology.cpp:119–121`):

```text
star, ring, full_mesh, erdos_renyi, barabasi_albert
```

Других моделей нет: **Watts–Strogatz, конфигурационная модель,
геометрический граф и т. п. не реализованы**. ER и BA — единственные
случайные/частично-ячеистые модели.

### 4.2 Erdős–Rényi `G(N,p)`

Алгоритм (`topology.cpp:168–175`):

```text
prob = clamp(er_p, 0, 1)
для i=0..N−1:
  для j=i+1..N−1:
    если U(0,1) <= prob: addBidirectional(i,j, core_cap, lat)
ensureConnected(g, core_cap, lat)
```

- `er_p` по умолчанию 0,10; `N=50`;
- `addBidirectional` создаёт две направленные дуги;
- `ensureConnected` ищет компоненты BFS и соединяет первую компоненту
  с первой вершиной каждой следующей компоненты двунаправленным ребром
  (`topology.cpp:79–87`). Поэтому фактический граф — **не чистый G(N,p)**,
  а G(N,p) с добавленными рёбрами связности.
- Теоретическое `E[E] = p·N(N−1)/2 = 122,5` неориентированных рёбер,
  `⟨k⟩ = p(N−1) = 4,9`.
- Фактически в `base_topology` (seed 42): **E = 101**, `⟨k⟩ = 4,04`,
  `k_max = 8`, `CV = 0,487311`, `C = 0,097015`, `L = 2,817959`, `D = 5`,
  `r = −0,290382`, `γ_est = 1,992871`.
- Фактическое E ниже теоретического на 21,5 неориентированных ребра
  (≈1,9σ при σ≈10,5); это может быть случайностью seed и/или результатом
  генератора, но `ensureConnected` в данном прогоне не добавлял рёбер
  (компонента одна, `components=1`).

### 4.3 Barabási–Albert

Алгоритм (`topology.cpp:176–212`):

```text
m  = max(1, min(ba_m, N−1))
m0 = max(m+1, 3); если m0 > N: m0 = N
# начальное ядро — полный граф на m0 вершинах
для i=0..m0−1, j=i+1..m0−1: addBidirectional(i,j, core_cap, lat)
# список повторений пропорционально текущим степеням
repeated = [i повторён degree(i) раз для i=0..m0−1]
для v=m0..N−1:
    targets = m различных вершин, выбранных из repeated
    для t in targets:
        addBidirectional(v,t, core_cap, lat)
        repeated.push(v); repeated.push(t)
ensureConnected(...)
```

- `ba_m=2` по умолчанию; `m0=max(3,3)=3`;
- выбор цели пропорционален степени (preferential attachment);
- `guard < 10000` защищает от зацикливания при поиске `m` различных
  целей; теоретически при малом числе уникальных вершин может добавить
  меньше `m` рёбер;
- `ensureConnected` может добавить рёбра и изменить степени;
- `assignCapacitiesByDegree` затем назначает access/core.

Фактически при `N=50`, `m=2`, seed 42: **E=97**, `⟨k⟩=3,88`,
`k_max=37`, `CV=1,944537`, `C=0,086022`, `L=1,920816`, `D=2`,
`r=−0,928203`, `γ_est=3,191512` (теоретическое `⟨k⟩≈2m=4`,
`γ≈3`).

### 4.4 Детерминированные топологии

- **star**: `addLink(0,i,core_cap)` для `i=1..N−1`; центр 0, листья;
  `E=49`, `⟨k⟩=1,96`, `k_max=49`, `L=1,96`, `D=2`, `r=−1`.
- **ring**: `addLink(i,(i+1)%N,core_cap)` при `i<j`; `E=49`,
  `L=17`, `D=49`, `r=−0,020833`.
- **full_mesh**: все пары `i<j`; `E=1225`, `⟨k⟩=49`, `C=1`, `L=1`, `D=1`.

Все три используют **только `core_capacity`**; `access_capacity` не влияет
на их рёбра. Это важно при интерпретации GUI-пресетов: для star/ring/full_mesh
изменение «Доступ, Мбит/с» не меняет ёмкости.

### 4.5 Проверка связности на t=0

- ER/BA: `ensureConnected` после генерации; BFS по направленным дугам
  (все дуги двунаправленные), число компонент; добавление рёбер от
  `comps[0].front()` к `comps[k].front()`.
- star/ring/full_mesh связаны конструктивно.
- `TopologyStructure.components` и `lcc_fraction` считаются BFS по
  направленному графу; для текущих двунаправленных графов это эквивалентно
  неориентированной связности.

### 4.6 Роли и пропускная способность

Роли (`topology.cpp:236–266`):

```text
n_servers  = max(1, int(N·0,15))   # N=50 → 7
n_switches = max(1, int(N·0,20))   # N=50 → 10
if n_servers+n_switches > N−1: ...
hosts = остальные                  # N=50 → 33
```

Узлы сортируются по **убыванию текущей степени**, при равенстве — по `id`.
Первые `n_servers` становятся `server`, следующие `n_switches` — `switch`,
остальные — `host`. Это не случайное назначение и не привязка к типу
устройства; в star-топологии хаб — сервер, но ещё 6 серверов — листья
с degree=1 (tie по id). В BA серверами становятся хабы.

Параметры узла при `heterogeneous=true`:

```text
P_attack   = clamp(profile.P_attack · (base_P_attack/0,60) ± 0,05, 0,01, 0,99)
P_recovery = clamp(profile.P_recovery ± 0,05, 0,01, 0,99)
mu         = clamp(profile.mu · (base_mu/0,20) ± 0,02, 0,01, 1,0)
C_repair   = profile.C_repair
weight     = profile.weight
```

При `heterogeneous=false`:

```text
P_attack   = clamp(base_P_attack ± 0,05)
P_recovery = clamp(base_P_recovery ± 0,05)
mu         = clamp(base_mu ± 0,01)
C_repair   = base_C_repair
weight     = base_weight
```

**Дефект:** в гетерогенном режиме `base_P_recovery`, `base_C_repair`,
`base_weight` **не используются**. GUI по умолчанию `heterogeneous=1`,
поэтому изменение `P_recovery` в GUI не влияет на узлы.

### 4.7 Структурные результаты (N=50, seed 42)

Из `results/experiments/base_topology/topology_structure.csv`:

| Топология | E (undirected) | ⟨k⟩ | k_min | k_max | CV | C | L | D | r | γ_est | E[⟨k⟩] |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| star | 49 | 1,960000 | 1 | 49 | 3,463380 | 0,000000 | 1,960000 | 2 | −1,000000 | — | 1,960000 |
| ring | 49 | 1,960000 | 1 | 2 | 0,100994 | 0,000000 | 17,000000 | 49 | −0,020833 | — | 2,000000 |
| full_mesh | 1225 | 49,000000 | 49 | 49 | 0,000000 | 1,000000 | 1,000000 | 1 | 0,000000 | — | 49,000000 |
| erdos_renyi | 101 | 4,040000 | 1 | 8 | 0,487311 | 0,097015 | 2,817959 | 5 | −0,290382 | 1,992871 | 4,900000 |
| barabasi_albert | 97 | 3,880000 | 2 | 37 | 1,944537 | 0,086022 | 1,920816 | 2 | −0,928203 | 3,191512 | 4,000000 |

`γ_est` для star/ring/full_mesh в CSV — `nan`; для ER/BA — оценка Хилла
по `x_min=2`. `E[⟨k⟩]` — `expected_avg_degree`: ER `p(N−1)=4,9`,
BA `2m=4,0`.

---

## 5. Векторы атак и алгоритмы восстановления

### 5.1 Атаки

События — только `Attack` и `RepairComplete` (`engine.cpp:20–23`).
Атака выбирает **узел**, не ребро. `selectAttackTarget` (`engine.cpp:154–188`):

- `random`: равномерно из активных узлов;
- `cascading`: множество `exposed` — активные соседи уже отказавших узлов
  (по направленным дугам; для двунаправленных — обычные соседи). Если
  `exposed` пуст, выбирается любой активный узел. Это **топологическая
  каскадность**, а не перегрузка/распространение нагрузки;
- `targeted`: узел с максимальной **текущей степенью** среди активных;
  при равенстве — случайный выбор. Вес `Node::weight`, загрузка `Edge::load`,
  центральность и тип узла при выборе **не учитываются**.

Успех атаки: `U(0,1) < Node::P_attack`; при успехе `state=0`,
`waiting.push`, `dispatch`. Неуспешная атака увеличивает `attacks`, но не
`attacks_successful`. Следующая атака планируется через `Exp(attack_lambda)`
от текущего времени; при `attack_lambda=0` атак нет. Первая атака —
`Exp(λ)` от t=0.

**Рёбра не атакуются и не отказывают.** Нет вероятности отказа ребра,
нет атаки на канал, нет cascade по нагрузке.

### 5.2 Восстановление

- Пул `repair_slots`; очередь `waiting`; `in_repair`.
- `dispatch`:
  - выбирает узел `selectWaiting()`;
  - если `resources + 1e-12 < C_repair` — `resource_exhausted=true`,
    **break**;
  - иначе списывает `C_repair`, занимает слот, планирует
    `RepairComplete` через `Exp(mu)`.
- `selectWaiting`:
  - FIFO: минимальное `failure_time`, tie — меньший `node id`;
  - Triage: максимальный `Node::weight`, tie — меньший `failure_time`.
- После ремонта: `U(0,1) < P_recovery` → `state=1`, `recoveries_success++`;
  иначе `recoveries_failed++`, узел возвращается в `waiting`, `failure_time=now`.
- Ресурсы не пополняются; стоимость не возвращается при неудаче.
- Средняя очередь/занятые слоты — точные интегралы по времени:
  `queue_area += |waiting|·Δt`, `slots_area += used_slots·Δt`,
  `mean = area/T`.

**Стратегии:** только `fifo` и `triage`. Новых стратегий (QoS-aware,
centrality-aware, приоритет по загрузке) **нет**. Triage — статический вес
`server=10/switch=5/host=1`, не динамическая центральность.

**Дефект head-of-line:** `dispatch` при нехватке ресурсов на выбранный
узел делает `break`, не пытаясь выбрать более дешёвый узел из очереди.
Если Triage выбрал server `C=3`, а осталось `R=2`, то switch `C=2` или
host `C=1` не будут отремонтированы, хотя ресурсов на них хватило бы.
Это может завышать `resource_exhausted` и искажать Triage.

**Старение/голодание:** нет; FIFO может задерживать дорогие/поздние
узлы, Triage — дешёвые/низкоприоритетные; повторная неудача сбрасывает
`failure_time`, что меняет порядок FIFO.

### 5.3 Изменилась ли логика по сравнению с базовой версией

- Атаки по-прежнему **только узловые**; ребра не атакуются.
- `targeted` по-прежнему по максимальной степени, без веса/загрузки.
- `cascading` по-прежнему по соседям отказавших, без нагрузки.
- Восстановление: только FIFO/Triage, без новых стратегий.
- Добавлены только структурный анализ и демонстрационный сценарий, а не
  новые модели атак/восстановления.

---

## 6. Графический интерфейс

### 6.1 Каркас

- Библиотека: **CustomTkinter** (`ctk.CTk`), тёмная тема, окно `NetSim`,
  размер 1360×860, минимум 1180×760, максимизация через `after(150)`.
- Три страницы: **Параметры**, **Результаты**, **О модели**.
- Слева сайдбар: логотип `NetSim`, навигация, статус ядра (`netsim.exe`),
  версия `NetSim 2.0`.
- Страница результатов: выбор прогона, кнопки «Обновить», «PNG»,
  «Таблицы»; 6 карточек; 8 вкладок.

### 6.2 Вкладки результатов

| Вкладка | Что показывает | Источник |
|---|---|---|
| **Динамика** | 2 графика: E_QoS/E_phys/LCC/Q во времени; IPTD и IPLR×1000 | `timeseries_<topo>.csv` |
| **Топологии** | grouped bars E_QoS/Q/LCC/φ_w с 95 % ДИ | `topology_comparison.csv` |
| **Sweep** | до 4 subplot-кривых E_QoS от λ/R/μ/P_attack | `sweep_*.csv` |
| **Атаки** | bars E_QoS по targeted/random/cascading × топологии | `attack_vector_analysis.csv` |
| **FIFO/Triage** | линии FIFO/Triage по R, переключение E_QoS/φ_w, звёздочки значимости | `strategy_comparison.csv` |
| **Структура** | текстовая таблица метрик + log-log CCDF степеней | `topology_structure.csv`, `topology_degrees_*.csv` |
| **Сводка** | компактные таблицы по типу прогона | `gui/summary.py` |
| **Данные** | любой CSV прогона в Treeview, копирование/экспорт | все `*.csv` |

### 6.3 Настраиваемые параметры

Секции `_build_params_page` (`gui/app.py:335–561`):

1. **Сценарий и топология**: сценарий (8 вариантов), 5 чекбоксов
   топологий, `N`, `seed`.
2. **Масштаб**: `n_sims` (GUI default 20), `T` (150), `sample_points` (200).
3. **Атаки**: `attack_strategy`, 3 чекбокса векторов, `attack_lambda` (1,0),
   `base_P_attack` (0,60).
4. **Восстановление**: `gatekeeper` fifo/triage, `repair_slots` (3),
   `global_resources` (200), `base_P_recovery` (0,80), `base_mu` (0,20),
   `resource_levels` (50,100,200,500).
5. **QoS**: класс 0–7, `demand_mbps` (2,0), `packet_bytes` (1500),
   `buffer_packets` (100).
6. **Каналы**: профиль «Базовая/Перегрузка/Стресс», `access_mbps` (1000),
   `core_gbps` (10), `base_latency_ms` (2,0), `preserve_total_demand`,
   `traffic_iterations` (3), `hop_penalty_ms` (5,0).
7. **Генератор**: `heterogeneous`, `er_p` (0,10), `ba_m` (2),
   `latency_jitter_ms` (1,0).
8. **Sweep**: диапазоны λ, R, μ, P_attack в формате `MIN MAX ТОЧЕК`.

Профили (`_apply_preset`):

| Профиль | demand, Мбит/с | access, Мбит/с | core, Гбит/с | buffer | latency, мс |
|---|---:|---:|---:|---:|---:|
| Базовая | 2,0 | 1000 | 10 | 100 | 2,0 |
| Перегрузка | 20,0 | 100 | 1 | 50 | 2,0 |
| Стресс | 100,0 | 100 | 1 | 20 | 2,0 |

### 6.4 Что визуализируется, а что нет

- **Динамика** — линии метрик, не состояние графа.
- **Топологии** — столбцы средних, не layout графа.
- **Атаки** — столбцы E_QoS.
- **Структура** — таблица + CCDF.
- **Визуализации узлов/рёбер, цветов отказавших узлов, состояния каналов,
  классов QoS на графе нет.** `networkx`/node-link не используются.
  `results/plot_topology.png` и `results/experiments/*/plots/topology.png` —
  это bar chart `plot_topology`, а не схема сети.
- Пользователь не видит, какие именно узлы отказали и как перераспределился
  трафик; только агрегированные метрики.

### 6.5 Таблицы и экспорт

- `DataTable` (`gui/table.py`): `ttk.Treeview`, тёмный стиль, сортировка
  по клику? (нет, только прокрутка), копирование TSV в буфер,
  экспорт CSV с `;` и BOM UTF-8.
- «Сводка» (`gui/summary.py`) формирует:
  - Топологии: `Топология, LCC, φ_w, E_QoS, Q%, IPTD, IPLR`;
  - Sweep: `x, Топология, LCC, φ_w, E_QoS, Q%`;
  - FIFO/Triage: `R, Топология, LCC/φ/E_QoS FIFO и Triage, p(φ), значимость`;
  - Атаки: `Стратегия, Топология, LCC, E_QoS, φ_w, Q%, IPLR`;
  - Временные ряды: строки метрик × топологии;
  - Структура: `Топология, ⟨k⟩, k_max, CV, C, L, D, r, γ`.
- «Данные» — полные CSV без агрегации.
- Кнопки «Таблица» на вкладках открывают **Сводку**, а не CSV;
  фактическая команда `_open_summary` (`gui/app.py:901–972`).
  Это расходится с `docs/03` (там сказано, что открывается CSV/«Данные»).

### 6.6 Интеграция и мониторинг

См. п. 1.4. GUI всегда передаёт `--progress`; поэтому критический дефект
пустого `ProgressFn` в GUI не проявляется, но проявляется в CLI.

### 6.7 Замечания по GUI

- `gui/core.py:default_config()`: `n_sims=20`, ядро `30` — разные дефолты.
- `data.summarize_run` для attack берёт **первую строку CSV** (при
  алфавитном порядке стратегий это `cascading/barabasi_albert`), а не
  агрегат; для strategy — строку с максимальным `E_QoS`, для sweep —
  последнюю точку. Карточки могут не соответствовать выбранному сценарию.
- `run_gui.py:26` перечисляет только «Динамика, Топологии, Sweep, Атаки,
  FIFO/Triage»; нет «Структура/Сводка/Данные».
- `docs/02:272` обещает `runs/<timestamp>`, код создаёт
  `runs/<scenario>_<timestamp>`.
- `docs/03:107–109` обещает «Таблицы» → «Данные» и «Таблица» → CSV;
  код ведёт в «Сводку».
- В `docs/01` остались «НИР» (строки 6, 274, 283) — наследие прежнего
  оформления.

---

## 7. Вычислительные эксперименты и результаты

### 7.1 Сценарии и параметры

Все конфиги: `seed=42`, `heterogeneous=1`, `er_p=0,10`, `ba_m=2`,
`access=1,0·10⁹` (кроме stress), `core=10,0·10⁹` (кроме stress),
`base_latency=0,002`, `jitter=0,001`, `qos_class=1`, `packet_bits=12000`,
`traffic_iterations=3`, `hop_penalty=0,005`, `preserve_total_demand=1`.
Ниже — отличия.

| Прогон | Сценарий | N | n_sims | T | S | λ | R | slots | QoS | demand, Мбит/с | access/core | buffer | Примечание |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|
| `base_topology` | topology | 50 | 30 | 150 | 200 | 1,0 | 200 | 3 | 1 | 2,0 | 1/10 Гбит/с | 100 | 5 топологий, targeted |
| `stress_topology` | topology | 50 | 20 | 150 | 200 | 1,5 | 120 | 2 | 1 | 10,0 | 100/1000 Мбит/с | 20 | 5 топологий, targeted |
| `base_timeseries` | timeseries | 50 | 1 | 150 | 300 | 1,0 | 200 | 3 | 1 | 2,0 | 1/10 Гбит/с | 100 | 5 топологий |
| `sweep_lambda` | sweep | 50 | 20 | 120 | 160 | 0,5–5,0 (6) | 200 | 3 | 1 | 2,0 | 1/10 Гбит/с | 100 | 5 топологий |
| `sweep_resources` | sweep | 50 | 20 | 120 | 160 | 1,5 | 20–300 (6) | 2 | 1 | 2,0 | 1/10 Гбит/с | 100 | 5 топологий |
| `strategy` | strategy | 50 | 30 | 150 | 200 | 1,5 | 20/40/80/150 | 2 | 1 | 2,0 | 1/10 Гбит/с | 100 | FIFO+Triage ×5 топологий |
| `attack` | attack | 50 | 20 | 150 | 200 | 1,2 | 150 | 3 | 1 | 2,0 | 1/10 Гбит/с | 100 | 3 вектора ×5 топологий |

`experiments.log` (7 прогонов, суммарно **462,2 с**):

```text
base_topology    55,4 c
stress_topology   7,6 c
base_timeseries   2,1 c
sweep_lambda     84,6 c
sweep_resources  77,5 c
strategy        147,8 c
attack           87,1 c
```

`demo` (не входит в архив): `N=50`, `n_sims=5`, `T=60`, `S=80`, λ=1,0,
R=120, slots=2, QoS=1, demand=2 Мбит/с, 5 топологий, sweep λ
`{0,5;1,33;2,17;3,0}`, R `{20;66;113;160}`, resource levels `{20;80}`.
`demo` не задаёт `access/core/buffer/base_latency/heterogeneous/gatekeeper`,
поэтому наследует текущие параметры GUI и не является полностью
каноническим.

### 7.2 Базовый сценарий

`base_topology` (30 прогонов/топологию), mean ± 95 % CI:

| Топология | E_QoS | Q | LCC | φ_w | IPTD, мс | IPLR |
|---|---:|---:|---:|---:|---:|---:|
| full_mesh | 0,511249 ± 0,039949 | 0,511286 | 0,687782 ± 0,037090 | 0,711253 ± 0,036310 | 2,00114 ± 0,008205 | 0 |
| erdos_renyi | 0,405890 ± 0,049498 | 0,477116 | 0,624769 ± 0,058115 | 0,425067 ± 0,045139 | 6,14268 ± 0,353740 | 5,0·10⁻⁶ |
| ring | 0,279300 ± 0,036080 | 0,122754 | 0,226094 ± 0,028078 | 0,255163 ± 0,036397 | 7,25692 ± 0,946324 | 0 |
| barabasi_albert | 0,114128 ± 0,026715 | 0,121896 | 0,186591 ± 0,034459 | 0,213477 ± 0,031454 | 1,067787 ± 0,187058 | 0 |
| star | 0,079688 ± 0,020640 | 0,079247 | 0,109524 ± 0,021571 | 0,152058 ± 0,021231 | 0,436679 ± 0,095207 | 0 |

Дополнительно: `exhaustion_rate` = 0 для всех, кроме ER (0,333333);
`baseline_qos_rate=1` для всех. `eff_mean == eff_qos_mean` для BA,
full_mesh, ring и star; у ER `eff_mean=0,406025`, `eff_qos_mean=0,405890`
(разность 1,35·10⁻⁴).

### 7.3 Стресс-сценарий

`stress_topology` (20 прогонов/топологию):

| Топология | E_QoS | Q | LCC | φ_w | IPTD, мс | IPLR |
|---|---:|---:|---:|---:|---:|---:|
| ring | 0,365109 ± 0,047039 | 0,013854 | 0,082613 ± 0,015862 | 0,097923 ± 0,016233 | 1,6973 ± 0,24647 | 7,48·10⁻⁴ |
| full_mesh | 0,173212 ± 0,020611 | 0,173397 | 0,259829 ± 0,024968 | 0,275291 ± 0,028773 | 0,877432 ± 0,074071 | 3,7·10⁻⁵ |
| erdos_renyi | 0,039484 ± 0,007194 | 0,004299 | 0,205437 ± 0,027914 | 0,132323 ± 0,015908 | 1,16808 ± 0,206659 | 2,608·10⁻³ |
| star | 0,016045 ± 0,006011 | 0,016022 | 0,033296 ± 0,006753 | 0,055139 ± 0,007606 | 0,090086 ± 0,034047 | 1,8·10⁻⁵ |
| barabasi_albert | 0,009296 ± 0,003681 | 2,3·10⁻⁵ | 0,059299 ± 0,010930 | 0,075935 ± 0,011350 | 0,020946 ± 0,009673 | 0 |

`exhaustion_rate`: BA 0,10; ER 0,65; full_mesh 0,05; ring 0; star 0.
`baseline_qos_rate=1` для всех.

### 7.4 Статистическая значимость (базовый сценарий)

U-критерий Манна–Уитни, `p_holm` (10 пар на метрику; α=0,05):

| Пара | E_QoS p_holm | Направление E_QoS | LCC p_holm | Направление LCC |
|---|---:|---|---:|---|
| BA — ER | 1,08·10⁻⁹ | ER > BA | 3,82·10⁻¹⁰ | ER > BA |
| BA — full_mesh | 2,87·10⁻¹⁰ | full > BA | 2,87·10⁻¹⁰ | full > BA |
| BA — ring | 5,28·10⁻⁸ | ring > BA | 0,0360 | ring > BA |
| BA — star | 0,0413 | BA > star | 0,000828 | BA > star |
| ER — full_mesh | 0,00217 | full > ER | 0,121 | ~ (незначимо) |
| ER — ring | 0,000197 | ER > ring | 3,82·10⁻¹⁰ | ER > ring |
| ER — star | 2,87·10⁻¹⁰ | ER > star | 2,87·10⁻¹⁰ | ER > star |
| full — ring | 5,28·10⁻⁸ | full > ring | 2,87·10⁻¹⁰ | full > ring |
| full — star | 2,87·10⁻¹⁰ | full > star | 2,87·10⁻¹⁰ | full > star |
| ring — star | 1,08·10⁻⁹ | ring > star | 1,52·10⁻⁷ | ring > star |

В stress все 10 пар значимы и по E_QoS, и по LCC:
`p_holm` от 6,30·10⁻⁷ до 0,0349; LCC — от 6,30·10⁻⁷ до 0,0107.
В частности, stress E_QoS: ring > full_mesh (p=6,30·10⁻⁷),
full_mesh > ER (6,30·10⁻⁷), ER > star (6,99·10⁻⁵),
star > BA (0,0349).

### 7.5 Sweep по λ

`E_QoS`, mean ± CI, 6 точек, 20 прогонов:

| λ | star | ring | full_mesh | ER | BA |
|---:|---:|---:|---:|---:|---:|
| 0,5 | 0,34172 ± 0,049285 | 0,71991 ± 0,028803 | 0,91676 ± 0,013412 | 0,83333 ± 0,030607 | 0,43903 ± 0,068664 |
| 1,4 | 0,037292 ± 0,009598 | 0,17253 ± 0,030250 | 0,30532 ± 0,033344 | 0,27679 ± 0,055972 | 0,062018 ± 0,016948 |
| 2,3 | 0,017545 ± 0,003502 | 0,077773 ± 0,009811 | 0,14006 ± 0,017673 | 0,095447 ± 0,012376 | 0,025605 ± 0,006894 |
| 3,2 | 0,009066 ± 0,003212 | 0,047213 ± 0,005157 | 0,089546 ± 0,009615 | 0,061137 ± 0,008319 | 0,014501 ± 0,004328 |
| 4,1 | 0,006414 ± 0,002048 | 0,036451 ± 0,003953 | 0,065189 ± 0,005434 | 0,044358 ± 0,005374 | 0,009983 ± 0,002651 |
| 5,0 | 0,006197 ± 0,001749 | 0,027325 ± 0,003109 | 0,052324 ± 0,004872 | 0,034929 ± 0,003206 | 0,009163 ± 0,002416 |

Порог: переход 0,5→1,4 снижает E_QoS в 3–9 раз; далее насыщение.

### 7.6 Sweep по R

`E_QoS`, mean ± CI, 6 точек:

| R | star | ring | full_mesh | ER | BA |
|---:|---:|---:|---:|---:|---:|
| 20 | 0,021577 ± 0,006513 | 0,10975 ± 0,02066 | 0,19584 ± 0,01976 | 0,11952 ± 0,010683 | 0,037743 ± 0,009561 |
| 76 | 0,022444 ± 0,00738 | 0,11333 ± 0,02285 | 0,21700 ± 0,025735 | 0,14869 ± 0,020302 | 0,041630 ± 0,011933 |
| 132 | 0,022448 ± 0,00738 | 0,11333 ± 0,02285 | 0,21700 ± 0,025735 | 0,14869 ± 0,019865 | 0,041626 ± 0,011932 |
| 188 | 0,022448 ± 0,00738 | 0,11333 ± 0,02285 | 0,21700 ± 0,025735 | 0,14869 ± 0,019865 | 0,041626 ± 0,011932 |
| 244 | 0,022448 ± 0,00738 | 0,11333 ± 0,02285 | 0,21700 ± 0,025735 | 0,14869 ± 0,019865 | 0,041626 ± 0,011932 |
| 300 | 0,022448 ± 0,00738 | 0,11333 ± 0,02285 | 0,21700 ± 0,025735 | 0,14869 ± 0,019865 | 0,041626 ± 0,011932 |

Насыщение при R≈76: дальнейший рост R не меняет метрики, ограничение —
`repair_slots=2`, а не ресурсы.

### 7.7 FIFO против Triage

20 комбинаций (5 топологий × R=20/40/80/150), 30 прогонов. `φ_w`,
`E_QoS`, p-значения, исчерпание ресурсов:

| R | Топология | φ_w FIFO | φ_w Triage | p(φ_w) | E_QoS FIFO | E_QoS Triage | p(E_QoS) | Исчерпание FIFO/Triage |
|---:|---|---:|---:|---:|---:|---:|---:|---:|
| 20 | star | 0,044 | 0,045 | 0,842 | 0,019 | 0,020 | 0,564 | 100 / 100 % |
| 40 | star | 0,050 | 0,059 | **0,0399** | 0,020 | 0,027 | 0,0668 | 100 / 100 % |
| 80 | star | 0,056 | 0,072 | **0,000978** | 0,020 | 0,028 | **0,0298** | 93 / 100 % |
| 150 | star | 0,057 | 0,084 | **1,81·10⁻⁵** | 0,020 | 0,029 | **0,0287** | 0 / 37 % |
| 20 | ring | 0,088 | 0,099 | 0,160 | 0,090 | 0,090 | 0,918 | 100 / 100 % |
| 40 | ring | 0,092 | 0,131 | **0,00127** | 0,093 | 0,100 | 0,237 | 100 / 100 % |
| 80 | ring | 0,097 | 0,154 | **6,56·10⁻⁵** | 0,094 | 0,106 | 0,101 | 60 / 100 % |
| 150 | ring | 0,097 | 0,167 | **1,29·10⁻⁵** | 0,094 | 0,108 | 0,0863 | 0 / 37 % |
| 20 | full_mesh | 0,229 | 0,236 | 0,554 | 0,155 | 0,152 | 0,842 | 100 / 100 % |
| 40 | full_mesh | 0,260 | 0,281 | 0,151 | 0,169 | 0,174 | 0,802 | 100 / 100 % |
| 80 | full_mesh | 0,268 | 0,324 | **0,00927** | 0,171 | 0,182 | 0,506 | 87 / 100 % |
| 150 | full_mesh | 0,270 | 0,346 | **0,00296** | 0,171 | 0,183 | 0,487 | 0 / 23 % |
| 20 | erdos_renyi | 0,098 | 0,098 | 0,994 | 0,095 | 0,096 | 0,971 | 100 / 100 % |
| 40 | erdos_renyi | 0,117 | 0,114 | 0,751 | 0,112 | 0,109 | 0,734 | 100 / 100 % |
| 80 | erdos_renyi | 0,127 | 0,121 | 0,574 | 0,116 | 0,110 | 0,549 | 100 / 100 % |
| 150 | erdos_renyi | 0,131 | 0,131 | 0,848 | 0,116 | 0,110 | 0,506 | 7 / 23 % |
| 20 | barabasi_albert | 0,059 | 0,062 | 0,610 | 0,029 | 0,032 | 0,383 | 100 / 100 % |
| 40 | barabasi_albert | 0,067 | 0,082 | **0,0298** | 0,032 | 0,042 | 0,0668 | 100 / 100 % |
| 80 | barabasi_albert | 0,074 | 0,101 | **0,000879** | 0,033 | 0,045 | **0,0345** | 90 / 100 % |
| 150 | barabasi_albert | 0,076 | 0,113 | **2,35·10⁻⁵** | 0,033 | 0,045 | **0,0333** | 0 / 33 % |

p-значения в таблице — `p_phi`/`p_raw` из CSV; **поправка Холма для
strategy не применяется** (см. дефект 8). CI для φ_w/E_QoS в CSV; φ_w:

| R | Топология | φ_w FIFO, mean ± 95 % CI | φ_w Triage, mean ± 95 % CI |
|---:|---|---:|---:|
| 20 | star | 0,044 ± 0,005 | 0,045 ± 0,005 |
| 40 | star | 0,050 ± 0,006 | 0,059 ± 0,007 |
| 80 | star | 0,056 ± 0,006 | 0,072 ± 0,007 |
| 150 | star | 0,057 ± 0,006 | 0,084 ± 0,010 |
| 20 | ring | 0,088 ± 0,015 | 0,099 ± 0,014 |
| 40 | ring | 0,092 ± 0,015 | 0,131 ± 0,016 |
| 80 | ring | 0,097 ± 0,014 | 0,154 ± 0,022 |
| 150 | ring | 0,097 ± 0,014 | 0,167 ± 0,025 |
| 20 | full_mesh | 0,229 ± 0,017 | 0,236 ± 0,018 |
| 40 | full_mesh | 0,260 ± 0,021 | 0,281 ± 0,022 |
| 80 | full_mesh | 0,268 ± 0,023 | 0,324 ± 0,033 |
| 150 | full_mesh | 0,270 ± 0,023 | 0,346 ± 0,040 |
| 20 | erdos_renyi | 0,098 ± 0,008 | 0,098 ± 0,008 |
| 40 | erdos_renyi | 0,117 ± 0,011 | 0,114 ± 0,011 |
| 80 | erdos_renyi | 0,127 ± 0,013 | 0,121 ± 0,011 |
| 150 | erdos_renyi | 0,131 ± 0,013 | 0,131 ± 0,012 |
| 20 | barabasi_albert | 0,059 ± 0,006 | 0,062 ± 0,006 |
| 40 | barabasi_albert | 0,067 ± 0,008 | 0,082 ± 0,010 |
| 80 | barabasi_albert | 0,074 ± 0,008 | 0,101 ± 0,011 |
| 150 | barabasi_albert | 0,076 ± 0,009 | 0,113 ± 0,013 |

E_QoS CI приведены в CSV (`eff_qos_ci`); например, star R=40 — 0,020 ± 0,005
и 0,027 ± 0,006; ring R=150 — 0,094 ± 0,013 и 0,108 ± 0,014;
BA R=150 — 0,033 ± 0,007 и 0,045 ± 0,009.

### 7.8 Векторы атак

`attack`, 20 прогонов, λ=1,2, R=150, 3 бригады. E_QoS mean ± CI:

| Стратегия | Топология | LCC | E_QoS | φ_w | Q | IPLR |
|---|---|---:|---:|---:|---:|---:|
| targeted | BA | 0,11911 | 0,066186 ± 0,014655 | 0,14907 | 0,070535 | 0 |
| targeted | ER | 0,40964 | 0,25329 ± 0,042565 | 0,27323 | 0,29242 | 7·10⁻⁶ |
| targeted | full_mesh | 0,57321 | 0,39583 ± 0,055445 | 0,59730 | 0,39571 | 0 |
| targeted | ring | 0,17030 | 0,20183 ± 0,037403 | 0,19568 | 0,086268 | 0 |
| targeted | star | 0,081756 | 0,055188 ± 0,016564 | 0,11961 | 0,054763 | 0 |
| random | BA | 0,43722 | 0,29296 ± 0,032922 | 0,46763 | 0,29672 | 1·10⁻⁶ |
| random | ER | 0,46419 | 0,30830 ± 0,043564 | 0,49837 | 0,32096 | 4·10⁻⁶ |
| random | full_mesh | 0,57321 | 0,39583 ± 0,055445 | 0,59730 | 0,39571 | 0 |
| random | ring | 0,18052 | 0,21575 ± 0,036395 | 0,22984 | 0,090963 | 0 |
| random | star | 0,41763 | 0,31009 ± 0,046787 | 0,44912 | 0,30546 | 0 |
| cascading | BA | 0,41130 | 0,25106 ± 0,031513 | 0,42690 | 0,26586 | 9·10⁻⁶ |
| cascading | ER | 0,50334 | 0,33763 ± 0,054868 | 0,52187 | 0,35314 | 6·10⁻⁶ |
| cascading | full_mesh | 0,57321 | 0,39583 ± 0,055445 | 0,59730 | 0,39571 | 0 |
| cascading | ring | 0,43896 | 0,40604 ± 0,058397 | 0,49349 | 0,27491 | 0 |
| cascading | star | 0,080606 | 0,056217 ± 0,019211 | 0,11704 | 0,055968 | 0 |

Статистических тестов для attack нет: только mean/CI.

### 7.9 Временные ряды

`base_timeseries`, 1 реализация на топологию, 300 точек, T=150:

| Метрика | star | ring | full_mesh | ER | BA |
|---|---:|---:|---:|---:|---:|
| LCC (среднее) | 0,179 | 0,230 | 0,733 | 0,605 | 0,129 |
| E_QoS (среднее) | 0,149 | 0,288 | 0,560 | 0,366 | 0,072 |
| φ_w (среднее) | 0,217 | 0,187 | 0,752 | 0,430 | 0,162 |
| Q (среднее), % | 14,8 | 12,3 | 56,2 | 41,5 | 7,6 |
| IPTD (среднее), мс | 0,69 | 6,99 | 2,00 | 6,65 | 0,77 |
| IPLR (макс.) | 0 | 0 | 0 | 0 | 0 |
| Первый отказ | 1,55 | 1,55 | 1,55 | 1,55 | 1,55 |
| Потреблено ресурсов | 152 | 135 | 131 | 198 | 161 |
| Истощение ресурсов | НЕТ | НЕТ | НЕТ | ДА | НЕТ |
| Успешных восстановлений | 79 | 76 | 74 | 74 | 78 |
| Неудачных попыток | 12 | 14 | 14 | 6 | 14 |
| Макс. очередь | 23 | 29 | 29 | 17 | 31 |

`e0_phys = e0_qos`: star 281,264; ring 77,3042; full 547,573;
ER 223,817; BA 289,241. `attacks/success`: star 159/96, ring 157/108,
full 139/106, ER 138/86, BA 152/106.

Времена первого падения ниже 0,9 (из CSV): star/ring 2,006689/2,006689;
full 7,023411/11,036789; ER 6,020067/9,0301; BA 2,006689/6,020067
(E_QoS/LCC). Для BA/ER/full E_QoS опережает LCC на 3–4 единицы времени.

### 7.10 Распределение по классам QoS

Во всех 7 конфигурациях `qos_class=1`; классов 0,2–7 нет. Поэтому
«распределение по классам QoS» в результатах **отсутствует**. Таблица
фактически одна:

| Класс | Число прогонов |
|---:|---:|
| 0 | 0 |
| 1 | 7 |
| 2–7 | 0 |

`run_meta` хранит `qos_requirement` для класса 1; `topology_comparison`
и `sweep` не содержат per-class полей. Для получения распределения нужен
новый параметр sweep `qos_class` (сейчас `runParameterSweep` его не
поддерживает; допустимы только lambda/resources/mu/P_attack/demand).

### 7.11 Выводы из документации

`docs/04` и `docs/05` дают следующие содержательные выводы:

1. full_mesh — лучшая E_QoS в базовом сценарии (0,511); ER — вторая
   (0,406); ring 0,279; BA 0,114; star 0,080.
2. QoS-эффективность падает раньше LCC для BA/ER/full_mesh на 3–4
   единицы времени; для star/ring — одновременно.
3. Stress: IPLR ER 2,608·10⁻³ > порога класса 1 (1·10⁻³) в 2,6 раза;
   ring 7,48·10⁻⁴ — близко к порогу.
4. E_QoS от λ имеет порог/насыщение; от R насыщается при R≈76 из-за
   `repair_slots=2`.
5. Triage значимо повышает φ_w для star/ring/full_mesh/BA, но не для ER
   (p=0,57–0,99); Triage чаще исчерпывает ресурсы.
6. Targeted опасен для star/BA; ER слабо чувствителен к вектору;
   ring устойчив к cascading; full_mesh инвариантен.
7. BA при m=2 хрупок; ER — нулевая модель без хабов; структурные
   различия: k_max 37 vs 8, CV 1,94 vs 0,49, L 1,92 vs 2,82, γ 3,19 vs 1,99.

---

## 8. Дефекты, риски и рекомендации

### 8.1 Критические

1. **Пустой `ProgressFn` при CLI без `--progress`.**
   - `main.cpp:331–332`: `ProgressFn progress = show_progress ? ... : ProgressFn{}`.
   - `main.cpp:336`, `349`, `353`, `450` вызывают `progress(...)`
     без проверки на пустоту.
   - Воспроизведено:
     ```powershell
     .\x64\Release\netsim.exe run --scenario structure `
       --topologies star --num-nodes 4 --out results\audit_tmp
     # ... Критическая ошибка: bad function call
     # exit=1
     ```
   - Затрагивает все сценарии `runScenario`, включая `topology`, `sweep`,
     `strategy`, `attack`, `timeseries`, `demo`, `all` (GUI всегда
     передаёт `--progress`, поэтому маскируется).
   - Исправление: `if (progress) progress(...)` либо всегда назначать
     no-op lambda.

2. **Игнорирование `base_P_recovery`, `base_C_repair`, `base_weight`
   в гетерогенном режиме.**
   - `topology.cpp:258–266` берёт `P_recovery`, `C_repair`, `weight`
     из `NODE_PROFILES`; `p.base_*` используются только в `else`
     (строки 267–279).
   - При `heterogeneous=1` (по умолчанию) изменение `P_recovery` в GUI
     не влияет на узлы; `base_P_recovery=0,80` в конфиге фактически
     декоративно.
   - Это ставит под сомнение интерпретацию экспериментов с P_recovery
     и C_repair; в текущих 7 конфигах они и так равны дефолтам, но
     параметризация некорректна.

3. **E_QoS почти не отличается от E_phys в базовых прогонах.**
   - В `base_timeseries` для всех 5 топологий и всех 300 точек
     `e_phys_norm − e_qos_norm = 0` (проверено).
   - В `base_topology` `eff_mean == eff_qos_mean` для BA, full_mesh,
     ring и star; у ER `0,406025` против `0,405890` (разность
     1,35·10⁻⁴). В `base_timeseries` `qos_reachable_ratio = 1` для
     ring/full_mesh/ER, а у star/BA равен 0 только при отсутствии
     достижимых пар.
   - Причина: в базовой временной серии все достижимые пути проходят
     класс 1; QoS-фильтр `I_ij` не отсекает ни одной пары. E_QoS
     фактически повторяет E_phys, а не измеряет дополнительный
     QoS-эффект. В 30-прогонном ER-сравнении фильтр срабатывает
     на единичных парах, но вклад в среднее пренебрежимо мал.
   - Рекомендация: добавить сценарий с заведомо низким классом
     (0/6/7) или перегрузкой, где достижимые пути нарушают IPLR/IPDV;
     иначе выводы о «QoS-эффективности» в базовой серии неотличимы
     от физической эффективности.

### 8.2 Высокие

4. **IPER/IPRR не проверяются.** Таблица Y.1541 содержит IPER, но
   `PathQos`/`evaluatePathQos` его не используют; README приписывает
   проверку IPER. Классы 6/7 фактически проверяются только по IPTD/IPDV/IPLR.
5. **Нет переходов класса QoS.** `qos_class` фиксирован; нет функции
   классификации сети/пути в класс, нет динамики 1→3→5. Запрос
   «правила перехода» не покрыт реализацией.
6. **Нет атак/отказов рёбер.** Только `Node::state`; `Edge` не имеет
   состояния/вероятности отказа. Каскадность — только по соседям,
   не по нагрузке.
7. **Нет стратегий восстановления кроме FIFO/Triage.** Нет QoS-aware,
   centrality-aware, load-aware; Triage — статический вес.
8. **Head-of-line блокировка ресурсов.** `dispatch` (`engine.cpp:134–139`)
   при нехватке ресурсов на выбранный узел делает `break`, не пытаясь
   выбрать более дешёвый узел. Это искажает `resource_exhausted` и
   результаты strategy.
9. **`hop_penalty` на ребро, а не на промежуточный узел.** Код
   `candidate = dist[u] + eq.delay_s + hop_penalty` добавляет `h` на
   каждом ребре; документация говорит о промежуточных узлах.
10. **Star/ring/full_mesh используют только core_capacity.**
    `access_capacity` для них не влияет; GUI-параметр «Доступ» меняет
    только ER/BA.
11. **`tCritical` игнорирует `alpha`.** `(void)alpha`; всегда 95 %.
    `confidenceInterval(alpha)` не может построить 90/99 %.
12. **U-критерий без поправки на связи.** `mannWhitneyU` считает ties
    как 0,5 в U, но std без tie correction; при n=20/30 и большом числе
    равных значений p приблизительны.
13. **Strategy p-values без Холма.** `runStrategyComparison` сравнивает
    FIFO/Triage для каждого R отдельно, `p_holm=p_raw`; 20 сравнений
    не корректируются. Attack — без тестов.
14. **Нет `requirements.txt`/фиксации версий.** README перечисляет
    пакеты, но не версии; Python 3.14 + pandas 3.0 + customtkinter 6.0
    могут быть несовместимы с Python 3.11 из руководства.
15. **Toolset `v145` vs README «VS 2022/2026».** VS 2022 не имеет v145;
    проект не соберётся без перенастройки.

### 8.3 Средние

16. `exportTopologyComparison`/`exportParameterSweep` пишут `wsurv_mean`
    и `wsurv_ci`, но значения **дублируют** `phi_w_mean`/`phi_w_ci`;
    отдельной метрики wsurv нет (см. `csv_export.cpp:99–100, 172–173`).
17. `Graph.num_edges` — directed arcs; `TopologyStructure.num_edges` —
    unique undirected; в CSV `edges` не совпадает с `Graph.num_edges`
    (star 49 vs 98). Нужна явная пометка.
18. `ensureConnected` добавляет рёбра в ER/BA; фактический ER — не
    чистый G(N,p). В base ER E=101 vs E[E]=122,5, ⟨k⟩=4,04 vs 4,9;
    интерпретировать как G(N,p) нужно с оговоркой.
19. `n_servers=15 %`, `n_switches=20 %` и ранжирование по степени
    приводит к тому, что в star 6 из 7 «серверов» — листья; это
    не соответствует интуиции «сервер = центр». Влияет на weights/Triage.
20. `demo` не задаёт `access/core/buffer/base_latency/heterogeneous/
    gatekeeper`; результаты зависят от текущих параметров GUI и не
    воспроизводимы как эталон.
21. `data.summarize_run` для attack берёт первую строку CSV
    (`cascading/BA` при алфавитном порядке), для strategy — лучшую
    строку, для sweep — последнюю точку; карточки могут не отражать
    выбранный сценарий.
22. `docs/01` сохраняет «НИР» (3 места), хотя прежний ребрендинг
    требовал убрать подобные упоминания.
23. `docs/02` обещает `runs/<timestamp>`, код — `runs/<scenario>_<timestamp>`.
24. `docs/03` обещает «Таблицы» → «Данные» и «Таблица» → CSV; код —
    «Сводка». `run_gui.py --tab` help не перечисляет «Структура/Сводка/Данные».
25. `results/` содержит устаревшие `topology_comparison.csv` (старая
    схема: `lcc_mean, eff_mean, wsurv_mean`) и `plot_topology.png`;
    они не соответствуют текущим CSV и могут запутать.
26. `results/last_config.ini` и `runs/` — рабочие артефакты; `runs/`
    игнорируется, но `last_config` может содержать сценарий demo с
    topology star и вводить в заблуждение при `--quick`.

### 8.4 Низкие/косметические

27. `gui/app.py:1348` `initialfile=f"netnsim_..."` — опечатка `netnsim`.
28. `docs/03` говорит «Окно состоит из трёх страниц», но у результатов
    8 вкладок; это не ошибка, но терминология «страницы/вкладки»
    смешана.
29. `README` заявляет «Y.1541 ... проверка IPER», хотя IPER не
    проверяется; лучше «таблица IPER, проверка IPTD/IPDV/IPLR».
30. `plot_results.py` не строит сводные/структурные таблицы, только PNG;
    для отчётов это не критично, но GUI «Сводка» — единственный
    табличный агрегатор.

---

## 9. Приоритетный план исправлений

1. **P0:** защитить вызовы `progress` (`if (progress)`) и добавить
   регрессионный тест CLI без `--progress` для всех сценариев.
2. **P0:** исправить гетерогенные параметры: применять
   `base_P_recovery`, `base_C_repair`, `base_weight` как множители/
   значения или явно запретить их в hetero-режиме и убрать из GUI.
3. **P1:** добавить сценарий/прогон с QoS-нарушениями на достижимых
   путях (класс 0/6/7 или перегрузка), чтобы E_QoS отличалась от E_phys;
   иначе базовая серия не проверяет QoS-фильтр.
4. **P1:** реализовать проверку IPER или явно удалить IPER из
   «проверяемых» возможностей; описать отсутствие IPRR.
5. **P1:** исправить `dispatch`: искать доступный по стоимости узел,
   а не делать `break` на первом дорогом.
6. **P2:** добавить атаки/отказы рёбер, если это требуется постановкой;
   иначе явно указать «edge failures not modeled».
7. **P2:** добавить QoS/centrality-aware стратегии или явно оставить
   только FIFO/Triage.
8. **P2:** статистика: tie-corrected U, `alpha` в `tCritical`, Holm для
   strategy, тесты для attack; как минимум документировать ограничения.
9. **P2:** визуализация графа (node-link, цвета состояний, классы QoS)
   или явно указать, что её нет.
10. **P3:** синхронизировать docs/README с кодом (имена runs, кнопки,
    `--tab`, IPER, «НИР»), добавить `requirements.txt`, убрать
    устаревшие `results/topology_comparison.csv`/`plot_topology.png`.

---

## 10. Приложение: точные схемы CSV

### 10.1 `timeseries_<topo>.csv`

21 столбец:

```text
time,lcc,phi_w,phi_active,e_phys,e_phys_norm,e_qos,e_qos_norm,
qos_ratio,reachable_ratio,qos_reachable_ratio,
iptd_mean_ms,iptd_p95_ms,iplr_mean,ipdv_mean_ms,
avg_utilization,max_utilization,total_load_gbps,active_nodes,
reachable_pairs,conforming_pairs
```

### 10.2 `topology_comparison.csv`

```text
topology,lcc_mean,lcc_ci,phi_w_mean,phi_w_ci,
eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,
qos_ratio_mean,qos_ratio_ci,iptd_mean_ms,iptd_ci,
iplr_mean,iplr_ci,ipdv_mean_ms,ipdv_ci,
queue_mean,queue_ci,exhaustion_rate,baseline_qos_rate,
wsurv_mean,wsurv_ci
```

`wsurv_*` — дубликат `phi_w_*` (см. дефект 16).

### 10.3 `topology_significance.csv`

```text
metric,topology_a,topology_b,U,p_raw,p_holm,significant,direction
```

### 10.4 `sweep_<param>.csv`

Первый столбец `x_value`; далее для каждой топологии 16 столбцов:

```text
<topo>_lcc_mean,<topo>_lcc_ci,
<topo>_phi_mean,<topo>_phi_ci,
<topo>_eff_mean,<topo>_eff_ci,
<topo>_eff_qos_mean,<topo>_eff_qos_ci,
<topo>_qos_mean,<topo>_qos_ci,
<topo>_iptd_mean,<topo>_iptd_ci,
<topo>_iplr_mean,<topo>_iplr_ci,
<topo>_wsurv_mean,<topo>_wsurv_ci
```

### 10.5 `strategy_comparison.csv`

```text
topology,gatekeeper,R,lcc_mean,lcc_ci,phi_mean,phi_ci,
eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,qos_mean,qos_ci,
iptd_mean_ms,iptd_ci,iplr_mean,iplr_ci,
sig_direction,p_raw,p_holm,sig_phi_direction,p_phi,exhaustion_rate
```

`sig_direction`/`p_raw`/`p_holm` — E_QoS; `sig_phi_direction`/`p_phi` — φ_w.
`p_holm` здесь равен `p_raw` (нет коррекции).

### 10.6 `attack_vector_analysis.csv`

```text
attack_strategy,topology,lcc_mean,lcc_ci,phi_mean,phi_ci,
eff_mean,eff_ci,eff_qos_mean,eff_qos_ci,qos_mean,qos_ci,
iptd_mean_ms,iptd_ci,iplr_mean,iplr_ci
```

### 10.7 `run_meta_<topo>.csv`

Ключи: `scenario`, `topology`, `num_nodes`, `n_sims`, `T`,
`attack_strategy`, `attack_lambda`, `gatekeeper`, `repair_slots`,
`global_resources`, `qos_class`, `qos_requirement`, `seed`,
`demand_per_pair`, `packet_bits`, `buffer_packets`, `e0_phys`, `e0_qos`,
`baseline_qos_ok`, `first_failure_time`, `had_failure`, `attacks`,
`attacks_successful`, `recoveries_success`, `recoveries_failed`,
`resources_consumed`, `resource_exhausted`, `mean_queue_depth`,
`mean_slots_used`, `max_queue_depth`.

### 10.8 `topology_structure.csv`

```text
topology,nodes,edges,avg_degree,min_degree,max_degree,
degree_cv,clustering,avg_path_length,diameter,assortativity,
components,lcc_fraction,expected_avg_degree,ccdf_slope,gamma_est,purpose
```

`topology_degrees_<topo>.csv`: `degree,count`.

---

## 11. Итоговая оценка

| Направление | Оценка | Комментарий |
|---|---|---|
| Архитектура | 8/10 | чистое разделение C++/Python, понятные модули; нет pinned-зависимостей |
| Математическая модель | 7/10 | M/M/1(K), Y.1541, E_QoS реализованы; IPER/классы/IPDV-приближения ограничены |
| Топологии | 7/10 | ER/BA корректны концептуально; `ensureConnected` меняет ER; нет WS |
| DES/атаки | 6/10 | node-only, нет edge failures; Triage статический; head-of-line баг |
| GUI | 7/10 | полный набор вкладок и таблиц; нет визуализации графа |
| Статистика | 6/10 | CI и U/Холм есть; ties/alpha/strategy/attack ограничены |
| Документация | 7/10 | подробная, но есть расхождения и остаточные «НИР» |
| Воспроизводимость | 7/10 | seed и config есть; demo зависит от текущих параметров, нет lock-файла |

**Общий вывод:** программный комплекс пригоден для исследовательского
моделирования и демонстрации, но перед использованием в отчётах/эксплуатации
нужно устранить P0-дефекты (пустой progress, гетерогенные параметры),
добавить QoS-сценарий с реальными нарушениями на достижимых путях и
синхронизировать документацию с фактическим поведением GUI/CLI.
