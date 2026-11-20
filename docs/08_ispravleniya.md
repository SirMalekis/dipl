# Исправление дефектов аудита

Документ фиксирует изменения после аудита `docs/07_tehnicheskiy_audit.md`.
Аудит описывает состояние `58f8db0`; здесь — что исправлено и чем это
подтверждено.

## 1. Критические исправления

| № | Дефект | Исправление | Проверка |
|---|---|---|---|
| 1 | CLI без `--progress` вызывал `bad function call` | `runScenario` всегда получает no-op `ProgressFn`; sweep использует `progress`, а не прямой `progressPrinter` | `tests/cli_smoke.ps1`: `structure` и `topology` без `--progress`, код 0 |
| 2 | В hetero-режиме игнорировались `base_P_recovery`, `base_C_repair`, `base_weight` | Параметры стали множителями профилей `server/switch/host`; базовые значения по умолчанию дают прежние профили | 3 новых теста в `test_core.cpp`; 82/82 |
| 3 | Head-of-line blocking в `dispatch` | Выбирается лучший **доступный по стоимости** узел; дорогой узел не блокирует дешёвые | Поведение в strategy/attack; регрессия 82/82 |
| 4 | `tCritical` игнорировал `alpha` | API сведён к явному двустороннему 95 % ДИ; ложный параметр удалён | Все CI в отчётах 95 % |
| 5 | U-критерий без поправки на связи | Средние ранги, tie-corrected variance, continuity correction | p-значения пересчитаны во всех CSV |
| 6 | Strategy p-values без Холма | Холм по всем 20 сравнениям отдельно для E_QoS и φ_w; в CSV `p_holm`, `p_phi_holm` | Таблица strategy в `docs/04` |
| 7 | Для attack не было тестов | `attack_significance.csv`: попарные U-тесты по E_QoS с Холмом внутри топологии | 15 строк, 5 топологий × 3 пары |
| 8 | `demo` зависел от текущих параметров GUI | `applyDemoSettings` фиксирует seed, ёмкости, буфер, QoS, hetero, gatekeeper и модель | `demo` воспроизводим |

## 2. Исправления документации и GUI

- `README`: IPER больше не заявлен как проверяемый; указаны IPTD/IPDV/IPLR
  и что IPER/IPRR не моделируются; toolset `v145`/VS 2026.
- `docs/01`: остаточные «НИР» заменены; добавлено явное утверждение, что
  класс QoS — входное требование, а автоматических переходов класса нет.
- `docs/02`: имя прогона `runs/<scenario>_<timestamp>`; добавлен
  `attack_significance.csv`; 82 теста и CLI smoke.
- `docs/03`: кнопка «Таблицы» ведёт в «Сводку», полные CSV — в «Данные»;
  требования к Visual Studio и `requirements.txt`.
- `run_gui.py --tab`: перечислены все 8 вкладок.
- `gui/data.py`: карточки attack/strategy/sweep показывают средние по
  прогону, а не первую/лучшую/последнюю строку.
- `gui/summary.py`: добавлена таблица «Атаки: значимость», p(φ_w)
  берётся из `p_phi_holm`.
- Удалены устаревшие `results/topology_comparison.csv`,
  `results/plot_topology.png`; `wsurv_*` удалены из CSV.
- Добавлен `requirements.txt`; исправлена опечатка `netnsim` → `netsim`.

## 3. Математические уточнения

- `hop_penalty` задокументирован как плата **за каждое пройденное ребро**
  (как в коде), а не за промежуточный узел; фактическая IPTD по-прежнему
  считается без него.
- `E_QoS` не «подменяет» `E_phys`: в базовой серии при классе 1 все
  достижимые пути проходят SLA, поэтому `Q|R = 1` и `E_QoS = E_phys`.
  Это зафиксировано в `docs/04`; стресс-серия (ER IPLR = 2,608·10⁻³,
  ring = 7,48·10⁻⁴) показывает работу фильтра. Добавлен тест
  `E_phys > 0` при `E_QoS = 0` для полного нарушения SLA.
- IPER/IPRR остаются **не моделируемыми**; таблица Y.1541 хранит пороги,
  но проверка пути использует IPTD/IPDV/IPLR. Это явно указано в README
  и docs.
- Автоматических переходов класса QoS нет; это не скрывается, а описано
  как свойство постановки (класс фиксирован на прогон).

## 4. Актуальные результаты после пересчёта

- Базовая серия: full_mesh E_QoS 0,511; ER 0,406 (Q 0,477, LCC 0,625);
  ring 0,279; BA 0,114; star 0,080. ER-full p_holm = 0,00235;
  BA-star p_holm = 0,0421; LCC ER-full p_holm = 0,112.
- Strategy после Холма: φ_w значимо растёт для ring с R = 40,
  star/BA с R = 80, full_mesh при R = 150; по E_QoS значимых различий
  нет; ER — без значимых эффектов.
- Attack: BA targeted значимо хуже random/cascading (p_holm = 2,04·10⁻⁷);
  star random лучше targeted/cascading (2,75·10⁻⁷); ring cascading лучше
  targeted/random (3,3·10⁻⁵/4,1·10⁻⁵); ER и full_mesh — без значимых
  различий.
- Stress: ER IPLR 2,608·10⁻³ (в 2,6 раза выше класса 1), ring 7,48·10⁻⁴.
- Timeseries: ER LCC 0,606, Q 41,7 %, ресурсов 200, успешных ремонтов 75;
  остальные значения близки к прежним.

## 5. Проверки

```powershell
.\build.ps1
.\tests\build_tests.ps1          # 82 проверки, 0 провалов
.\tests\cli_smoke.ps1            # structure/topology без --progress + selftest
.\x64\Release\netsim.exe selftest
python run_gui.py --selftest
```

Все перечисленные проверки проходят. GUI-скриншоты пересняты по новым CSV.
`demo` без `--progress` завершается с кодом 0 за 58,6 с и создаёт 23 CSV
(включая `attack_significance.csv`), то есть все вкладки заполняются.

## 6. Что осталось ограничением модели (не ошибкой)

- IPER/IPRR не моделируются; нет битовых ошибок и переупорядочивания.
- Класс QoS фиксирован на прогон; автоматических переходов класса нет.
- Атакуются только узлы; отказов/атак рёбер нет.
- Нет node-link визуализации графа: GUI показывает агрегированные метрики,
  bar chart и CCDF.
- `access_capacity` влияет только на ER/BA; star/ring/full_mesh используют
  `core_capacity`.
- `ensureConnected` может добавлять рёбра в ER/BA; это не чистый G(N,p).
- `Graph::num_edges` считает дуги, `TopologyStructure::num_edges` — уникальные
  неориентированные рёбра.
- Triage использует статический вес `server/switch/host`, без центральности.
