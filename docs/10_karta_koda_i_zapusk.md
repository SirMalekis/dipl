# Карта кода, назначение функций и упрощённый запуск

Документ отвечает на два вопроса:

1. какая функция/модуль за что отвечает и зачем нужна;
2. как запустить NetSim минимальным числом действий.

Актуальные математические детали — в [docs/01](01_teoriya.md) и
[docs/09](09_tekhnicheskiy_audit_v2.md); здесь — инженерная карта кода.

---

## 1. Запуск: текущий способ и упрощение

### 1.1 Было: три шага

```powershell
.\build.ps1
python -m pip install -r requirements.txt
python run_gui.py
```

Для первого запуска нужны Visual Studio с C++ и Python. Повторный запуск —
только `python run_gui.py`, но пользователь должен помнить про сборку и
зависимости.

### 1.2 Стало: один файл `start_netsim.cmd`

В корне лежит `start_netsim.cmd`. Достаточно **открыть/дважды щёлкнуть
этот файл**:

```text
start_netsim.cmd
```

Что делает launcher:

1. переходит в каталог проекта (`cd /d "%~dp0"`);
2. ищет `python`, иначе `py -3`;
3. если `x64\Release\netsim.exe` отсутствует — вызывает `build.ps1`;
4. запускает `python run_gui.py`;
5. пробрасывает все аргументы в GUI;
6. при ошибке показывает код выхода и (кроме `--selftest`) ставит паузу.

Полезные варианты:

```text
start_netsim.cmd --selftest
start_netsim.cmd --open-run results\experiments\strategy --tab "FIFO/Triage"
start_netsim.cmd --rebuild
```

`--rebuild` обрабатывается самим launcher-ом и не передаётся в GUI; после
сборки открывается обычный GUI.

### 1.3 Что проверено

- `start_netsim.cmd --help` — код 0;
- `start_netsim.cmd --selftest` — `GUI_SELFTEST_OK`, код 0;
- `start_netsim.cmd --rebuild --help` — сборка + `--help`, код 0;
- `build.ps1` сохранён в UTF-8 с BOM, чтобы Windows PowerShell 5.1
  корректно читал русские строки; `tests/*.ps1` — также с BOM;
- `run_showcase.cmd` запускает пять режимов, проверяет пороги и делает
  скриншоты; результат `SHOWCASE_OK`.

### 1.4 Пять showcase-режимов

```text
run_showcase.cmd
run_showcase.cmd -NoScreenshots
```

Создаются `results/showcase/01_catastrophe` … `05_excellent`,
`showcase_summary.csv`, `showcase_summary.png` и
`docs/img/showcase_*.png`. Параметры и пороги — в
[docs/11_eksperimenty_showcase.md](11_eksperimenty_showcase.md).

### 1.5 Можно ли сделать «один .exe»?

Да, но это отдельный способ упаковки, а не текущий launcher.

**Вариант A. `start_netsim.cmd` (реализован).**
Плюсы: один файл для пользователя, не нужно помнить команды; минусы:
нужны Python и (для первой сборки) Visual Studio; это `.cmd`, а не
самостоятельный `.exe`.

**Вариант B. PyInstaller one-file.**
Можно собрать `NetSim.exe`, включив `run_gui.py`, Python, Tk/Tcl,
matplotlib/pandas/numpy и `netsim.exe` как data-файл. Плюс: один `.exe`
для пользователя. Минусы: размер 150–400 МБ, антивирусные ложные
срабатывания, необходимость при сборке явно включить `netsim.exe` и
ресурсы Matplotlib; C++-ядро всё равно остаётся отдельным процессом.
В репозитории PyInstaller-сборка пока не добавлена, чтобы не вводить
тяжёлую зависимость.

**Вариант C. Установщик/zip.**
Inno Setup/NSIS или zip с `netsim.exe`, Python embeddable, `start_netsim.cmd`
и `requirements`-пакетами. Плюс: воспроизводимо; минус: нужен сборочный
пайплайн и место.

**Вариант D. Полностью один нативный `.exe`.**
Потребовал бы встроить Python/Tk или переписать GUI на C++ (Qt/WinAPI);
это уже другая архитектура, не текущая.

Рекомендация: для работы в этом репозитории — `start_netsim.cmd`; для
передачи заказчику — PyInstaller/установщик с уже собранным `netsim.exe`.

---

## 2. C++-ядро: файлы и функции

### 2.1 `main.cpp` — точка входа и сценарии

| Функция | Назначение |
|---|---|
| `main` | разбирает команду (`run`, `selftest`, `version`, `interactive`) и вызывает нужный режим |
| `runCli` | читает `--config`/CLI-опции, применяет `SimConfig`, вызывает `runScenario`, сохраняет `results/last_config.ini` |
| `runScenario` | главный диспетчер: `topology`, `sweep`, `strategy`, `attack`, `timeseries`, `structure`, `demo`, `all`; делает структуру, вызывает эксперименты, экспортирует CSV |
| `applyDemoSettings` | фиксирует компактный `demo`: 5 прогонов, T=60, S=80, seed 42, R=120, 2 слота, 5 топологий |
| `runSelfTest` | встроенная быстрая проверка Y.1541, M/M/1, топологий, DES |
| `interactive` | интерактивное меню с вопросами |
| `configureAdvanced` | расширенные параметры (каналы, ER/BA, seed) |
| `analyzeTopologies` | генерирует графы всех выбранных топологий и считает `TopologyStructure` |
| `printTopologyTable` / `printStrategyTable` / `printTopologyStructure` | консольные таблицы |
| `printUsage` | справка CLI |
| `progressPrinter` | печатает `PROGRESS <fraction> <stage>` для GUI |
| `header`, `hr`, `lower`, `askInt/Double/Bool/String`, `askChoice`, `askMultiChoice` | консольное оформление и ввод |

Ключевая логика `runScenario`:

```text
validate config
save config.ini (кроме structure)
analyze topology structure → topology_structure.csv / topology_degrees_*.csv
dispatch scenario:
  topology  → runTopologyComparison
  sweep     → runParameterSweep по params × topologies
  strategy  → runStrategyComparison
  attack    → runAttackVectorAnalysis + attack_significance.csv
  timeseries→ runSimulation для каждой топологии
  structure → только структура
  demo      → topology, sweep, strategy, attack, timeseries
  all       → те же пять с текущими параметрами
```

### 2.2 `config.*` — единая конфигурация

| Функция | Назначение |
|---|---|
| `SimConfig::applyOption` | применяет одну пару `key=value`, валидирует диапазон, поддерживает CLI/INI |
| `SimConfig::loadFromFile` | построчно читает INI, пропускает комментарии, вызывает `applyOption`, `validate` |
| `SimConfig::saveToFile` | пишет полный `config.ini` |
| `SimConfig::validate` | проверяет N, n_sims, T, S, вероятности, QoS, ёмкости, топологии, sweep |
| `SimConfig::topologyParams` | преобразует `SimConfig` → `TopologyParams` |
| `SimConfig::qosParams` | `qos_class`, `packet_bits`, `buffer_packets` → `QosParams` |
| `SimConfig::trafficParams` | demand, MSA, hop, preserve → `TrafficParams` |
| `SimConfig::ensureDefaultSweepRanges` | задаёт λ/R/μ/P_attack диапазоны |
| `SimConfig::summary` | человекочитаемая сводка для консоли/GUI |
| `trim`, `parseDoubleList`, `parseStringList`, `parseRange`, `parseBool/Int/Double/UInt64`, `linspace`, `join*` | парсинг и форматирование |

### 2.3 `topology.*` — граф и генераторы

| Функция | Назначение |
|---|---|
| `Graph::addEdge` | одна направленная дуга `u→v` с capacity/latency |
| `Graph::addBidirectional` | две независимые дуги `u→v`, `v→u` |
| `Graph::rebuildEdgeIndex` | сквозные `Edge::id` 0..E−1 для массивов нагрузок |
| `Graph::activeNodes/totalCapacity/totalLoad` | агрегаты графа |
| `createNetworkTopology` | строит star/ring/full_mesh/ER/BA, назначает роли и ёмкости |
| `availableTopologies` | список 5 топологий |
| `isKnownTopology` | проверка имени |
| `topologyDescription` | человекочитаемое описание |
| `components` | BFS-компоненты для `ensureConnected` |
| `ensureConnected` | соединяет компоненты ER/BA рёбрами от первой компоненты |
| `assignCapacitiesByDegree` | ER/BA: core при `min(deg)≥1,10·⟨k⟩`, иначе access |
| `jitter` | ограниченный шум вероятностей; при 0/1 разброс 0 |

`createNetworkTopology` также задаёт `P_attack/P_recovery/mu/C_repair/weight`
по `NodeProfile`, с множителями `base_*` (см. docs/09).

### 2.4 `topology_metrics.*` — структурный анализ

| Функция | Назначение |
|---|---|
| `analyzeTopology` | заполняет `TopologyStructure`: N, E, степени, CV, C, L, D, r, компоненты, LCC, E[⟨k⟩], CCDF slope, γ |
| `topologyPurpose` | текст «зачем эта топология» |
| `computeComponents` | компоненты и доля LCC |
| `computePaths` | BFS-длины путей, средняя L, диаметр |
| `computeClustering` | глобальный коэффициент кластеризации |
| `computeAssortativity` | ассортативность по степеням концов рёбер |
| `computePowerLaw` | линейный наклон CCDF и оценка Хилла `γ=1+n_tail/Σln(k/1,5)` |

### 2.5 `qos.*` — Y.1541 и M/M/1(K)

| Функция | Назначение |
|---|---|
| `qosRequirement` | доступ к строке таблицы Y.1541 по классу 0–7 |
| `qosClassFromInt`, `parseQosClass` | разбор класса |
| `qosClassName`, `qosRequirementText`, `qosTableText` | текстовое представление |
| `evaluateEdgeQos` | `μ=C/L`, `ρ=load/C`, IPTD/IPLR/IPDV, `saturated` |
| `evaluatePathQos` | проверка пути по IPTD/IPDV/IPLR; IPER не проверяется |

### 2.6 `traffic.*` — All-to-All, Дейкстра, MSA

| Функция | Назначение |
|---|---|
| `assignTraffic` | активные узлы, сохранение суммарного спроса, MSA, финальная матрица путей |
| `runDijkstra` | кратчайшие пути по `IPTD_e + hop_penalty`; хранит actual delay, `ln Π(1−loss)`, `ΣIPDV²` |
| `ShortestPathTree::reset` | инициализация массивов дерева |

Алгоритм:

```text
для k итераций:
  load ← текущие нагрузки
  для каждого src: Дейкстра
  для каждого dst: весь спрос b_A на путь
  load ← (1−α)load + α·new_load, α=1/(k+1)
финальная Дейкстра на сошедшихся нагрузках
```

### 2.7 `metrics.*` — метрики

| Функция | Назначение |
|---|---|
| `MetricsContext::calibrate` | `E0_phys`, `E0_qos`, baseline reachable/conforming |
| `computeMetrics` | LCC, φ_w, E_phys/E_QoS, Q/R, IPTD/IPLR/IPDV, утилизация |
| `computeLccRatio` | LCC/N |
| `computeWeightedSurvivability` | `φ_w` по весам LCC |
| `computeActiveWeightRatio` | доля веса работоспособных узлов |
| `largestComponent` | BFS по активным узлам |

### 2.8 `engine.*` — дискретно-событийный Монте-Карло

| Функция/лямбда | Назначение |
|---|---|
| `runSimulation` | калибровка, очередь событий, сетка T/S, интегралы, результат |
| `schedule` | добавляет `Attack`/`RepairComplete` в priority_queue |
| `selectWaiting(available)` | FIFO/Triage среди **доступных по стоимости** |
| `dispatch(now)` | занимает слоты, списывает `C_repair`, планирует `Exp(μ)` |
| `selectAttackTarget` | targeted по степени, random, cascading по соседям |
| `registerFailure` | state=0, очередь, first_failure, dirty |
| `processEvent` | обработка атаки/ремонта, успех по `P_attack/P_recovery` |
| `sampleExp(rate)` | `Exp(rate)` через `std::exponential_distribution` |

### 2.9 `experiments.*` — статистика и сценарии

| Функция | Назначение |
|---|---|
| `tCritical(df)` | 95 % t-критическое (таблица + Корниш–Фишер) |
| `makeStat` | mean, std, se, CI |
| `mannWhitneyU` | U-критерий с рангами, tie correction, continuity correction |
| `timeAverage` | интеграл метрики по временной сетке (трапеции) |
| `confidenceInterval` | 95 % CI по выборке |
| `pairwiseSignificance` | все пары, U, p_raw, Holm, direction |
| `runMonteCarlo` | n_sims реализаций, seed offset `i·1000003`, MCResult |
| `runTopologyComparison` | MC по топологиям + significance E_QoS/LCC |
| `runParameterSweep` | значения λ/R/μ/P_attack/demand |
| `runStrategyComparison` | FIFO/Triage, p и Holm по 20 сравнениям |
| `runAttackVectorAnalysis` | MC по векторам × топологиям |

### 2.10 `csv_export.*` — отчёты

| Функция | Файл |
|---|---|
| `exportTimeSeries` | `timeseries_<topo>.csv` |
| `exportTopologyComparison` | `topology_comparison.csv` |
| `exportTopologySignificance` | `topology_significance.csv` |
| `exportParameterSweep` | `sweep_<param>.csv` |
| `exportStrategyComparison` | `strategy_comparison.csv` |
| `exportAttackVectorAnalysis` | `attack_vector_analysis.csv` |
| `exportAttackSignificance` | `attack_significance.csv` |
| `exportRunMetadata` | `run_meta_<topo>.csv` |
| `exportTopologyStructure` | `topology_structure.csv` |
| `exportDegreeDistribution` | `topology_degrees_<topo>.csv` |

`escape`, `openCsv`, `ensureParentDir`, `writeP` — общая CSV-инфраструктура
(UTF-8, 6 знаков, p-values в scientific).

---

## 3. Python GUI: файлы и функции

### 3.1 `run_gui.py`

| Функция | Назначение |
|---|---|
| `main` | argparse (`--selftest`, `--screenshot`, `--open-run`, `--tab`), создаёт `App`, запускает mainloop |
| `run` | обёртка selftest с traceback |
| `open_existing` | открывает готовый прогон и делает скриншот |

### 3.2 `gui/core.py`

| Функция/класс | Назначение |
|---|---|
| `find_core` | ищет `NETSIM_EXE`, `x64/Release/netsim.exe`, Debug/build/bin |
| `default_config` | GUI-дефолты, совместимые с `SimConfig` |
| `write_config` / `read_config` | INI для C++/чтение |
| `CoreRunner` | `Popen`, поток чтения stdout, очередь, `terminate` |
| `parse_progress` | разбор `PROGRESS` |
| `format_bytes_per_second` | человекочитаемые единицы |
| `open_in_explorer` | открыть каталог |
| `run_core_sync` | синхронный запуск для self-test |

### 3.3 `gui/data.py`

| Функция | Назначение |
|---|---|
| `read_csv` | безопасное чтение DataFrame |
| `list_runs` | каталоги с `config.ini` в `runs/` и `results/` |
| `time_series_files` / `load_time_series` | временные ряды |
| `load_topology/sweep/structure/degrees/meta/attack/strategy` | загрузка соответствующих CSV |
| `run_label` | подпись `родитель/имя · сценарий · N топологий` |
| `summarize_run` | карточки; timeseries → mean, topology → лучший E_QoS, attack/strategy/sweep → средние |
| `_average` | среднее по колонке |

### 3.4 `gui/plots.py`

| Функция | Назначение |
|---|---|
| `style_axes`, `style_figure`, `empty`, `empty_figure` | тёмное оформление/заглушки |
| `plot_timeseries` | E_QoS/E_phys/LCC/Q + IPTD/IPLR×1000 |
| `plot_topology` | grouped bars с CI |
| `plot_attack` | bars E_QoS по векторам |
| `plot_sweep` | до 4 subplot-кривых |
| `plot_degree_ccdf` | log-log CCDF + γ |
| `plot_strategy` | FIFO/Triage по R, звёздочки значимости |
| `save_figure` | PNG 180 dpi |

### 3.5 `gui/summary.py`

| Функция | Таблица |
|---|---|
| `_topology_table` | Топология, LCC, φ, E_QoS, Q, IPTD, IPLR |
| `_sweep_table` | x, топология, LCC/φ/E_QoS/Q |
| `_strategy_table` | R, топология, LCC/φ/E_QoS FIFO/Triage, p(φ_holm), значимость |
| `_attack_table` | стратегия, топология, LCC, E_QoS, φ, Q, IPLR |
| `_attack_significance_table` | пары стратегий, U, p_raw, p_holm, направление |
| `_timeseries_table` | метрики × топологии, включая ресурсы/ремонты |
| `_structure_table` | ⟨k⟩, k_max, CV, C, L, D, r, γ |
| `available_sets` / `build` | какие таблицы доступны и их сборка |

Форматтеры `_num`, `_sci`, `_p`, `_direction`, `_mean`,
`_sort_topologies` обеспечивают русскую десятичную запятую и `—` для NaN.

### 3.6 `gui/table.py`

`DataTable` (наследник `CTkFrame`):
`set_dataframe` / `set_data` — заполнение Treeview;
`copy_to_clipboard` — TSV для Word/Excel;
`export_csv` — CSV `;` + BOM;
`_format` — NaN, проценты, scientific.

### 3.7 `gui/app.py`

| Группа | Методы | Назначение |
|---|---|---|
| инициализация | `__init__`, `_build_fonts`, `_build_layout`, `_maximize` | окно, сайдбар, страницы |
| параметры | `_build_params_page`, `_section`, `_row`, `_entry`, `_option`, `_checkbox_row`, `_on_scenario_changed`, `_on_qos_changed`, `_apply_preset`, `_collect_config`, `_parse_levels` | формы и сборка config |
| запуск | `_start_run`, `_stop_run`, `_poll_queue`, `_append_log`, `_on_done` | Popen, прогресс, журнал, переход к результатам |
| результаты | `_build_results_page`, `_refresh_runs`, `_on_run_selected`, `_load_run` | вкладки, выбор прогона, загрузка CSV |
| сводка/данные | `_refresh_summary`, `_on_summary_changed`, `_open_summary_tab`, `_open_summary`, `_refresh_data_table`, `_on_data_file_changed`, `_open_data_tab`, `_open_data`, `_open_sweep_data` | таблицы и переключение |
| графики | `_draw_active_tab`, `_draw_structure`, `_on_structure_topo_changed`, `_save_current_plot` | отрисовка/PNG |
| структура | `_set_structure_text` | текстовая карточка |
| self-test | `run_selftest`, `_finish_selftest`, `capture_window` | сквозной тест и скриншот |
| прочее | `_show_page`, `_build_about_page`, `_open_run_dir`, `open_run`, `_on_close` | навигация/каталог/закрытие |

### 3.8 `plot_results.py`

`main` проходит по прогону, строит `timeseries_*.png`, `topology.png`,
`attack.png`, `strategy_E_QoS.png`, `strategy_phi_w.png`, `sweep.png`,
`degree_<topo>.png`; `save` сохраняет Figure.

### 3.9 Служебные скрипты и тесты

| Файл | Функция | Назначение |
|---|---|---|
| `scripts/run_showcase.ps1` | — | 5 режимов (timeseries + topology), plots, скриншоты, summary + пороги |
| `scripts/make_showcase_summary.py` | `main`, `read_ini` | `showcase_summary.csv`, `showcase_summary.png`, Markdown-таблица |
| `tests/showcase_check.py` | `main`, `require` | монотонность и критические пороги 5 режимов |
| `tests/test_core.cpp` | `main`, `check`, `checkNear`, `makeLine` | 82 проверки ядра |
| `tests/cli_smoke.ps1` | — | CLI без `--progress` |
| `start_netsim.cmd` | — | сборка при необходимости + GUI |
| `run_showcase.cmd` | — | запуск showcase в один клик |

---

## 4. Поток данных

```text
GUI form
  → config.ini
  → netsim.exe run --config ... --out ... --progress
  → stdout PROGRESS + журнал
  → CSV в каталоге прогона:
      config.ini
      timeseries_<topo>.csv
      topology_comparison.csv / topology_significance.csv
      sweep_<param>.csv
      strategy_comparison.csv
      attack_vector_analysis.csv / attack_significance.csv
      run_meta_<topo>.csv
      topology_structure.csv / topology_degrees_<topo>.csv
  → pandas DataFrame
  → вкладки/карточки/«Сводка»/«Данные»
```

CSV — единственный контракт между C++ и Python; GUI не вызывает C++
функции напрямую.

---

## 5. Что упрощать дальше

1. **`start_netsim.cmd`** — уже основной сценарий для Windows.
2. **Ярлык на рабочем столе** на `start_netsim.cmd` — один клик.
3. **PyInstaller/установщик** — если нужен один `.exe` без Python/VS;
   потребуется включить `netsim.exe`, Tcl/Tk, Matplotlib, pandas/numpy.
4. **Docker/portable zip** — если нужна воспроизводимость на другой машине;
   ядро всё равно требует Windows-бинарник или пересборку под Linux.
5. **`--demo` в launcher-е** — можно добавить запуск demo и сразу
   `--open-run`; сейчас demo запускается из GUI одним сценарием.

План переноса этих изменений в текст ПЗ НИР — в
[docs/12_plan_redaktirovaniya_PZ.md](12_plan_redaktirovaniya_PZ.md).

---

## 6. Диагностика запуска

| Симптом | Причина | Что сделать |
|---|---|---|
| `Python 3 not found` | Python не в PATH | установить Python 3.11+, `python -m pip install -r requirements.txt` |
| `Build failed` | нет VS/C++ toolset | установить VS 2026 C++ или заменить v145 на v143 |
| `Ядро не найдено` | нет `netsim.exe` | `start_netsim.cmd --rebuild` или `.\build.ps1` |
| GUI не видит прогон | список кэширован | «Обновить» |
| ошибка кодировки `.ps1` | нет BOM в Windows PowerShell 5.1 | скрипты сохранены с UTF-8 BOM |
| `bad function call` | старый бинарник без no-op progress | пересобрать `netsim.exe` |
