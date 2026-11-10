"""NetSim GUI — визуальная оболочка для вычислительного ядра (ВКР)."""

from __future__ import annotations

import queue
import tkinter as tk
from datetime import datetime
from pathlib import Path
from tkinter import filedialog, messagebox
import math
import time

import customtkinter as ctk
import matplotlib

matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

from . import core, data, plots

TOPO_LABELS = {
    "star": "Звезда",
    "ring": "Кольцо",
    "full_mesh": "Полносвязная",
    "erdos_renyi": "Эрдёш–Реньи",
    "barabasi_albert": "Барабаши–Альберт",
}
TOPO_ORDER = ["star", "ring", "full_mesh", "erdos_renyi", "barabasi_albert"]

TOPO_DESCRIPTIONS = {
    "erdos_renyi": (
        "ER (G(N,p)) — однородный случайный граф: каждое ребро независимо "
        "с вероятностью p, степени близки к распределению Пуассона, "
        "выраженных хабов нет. ⟨k⟩ = p(N−1)."
    ),
    "barabasi_albert": (
        "BA — безмасштабная сеть с предпочтительным присоединением: новый "
        "узел подключается к m существующим пропорционально их степени. "
        "P(k) ~ k⁻³, есть хабы; m — число связей нового узла."
    ),
}

SCENARIOS = [
    ("topology", "Сравнение топологий"),
    ("sweep", "Параметрический анализ"),
    ("strategy", "FIFO vs Triage"),
    ("attack", "Векторы атак"),
    ("timeseries", "Временные ряды"),
    ("all", "Все сценарии"),
]
SCENARIO_BY_LABEL = {label: key for key, label in SCENARIOS}
LABEL_BY_SCENARIO = {key: label for key, label in SCENARIOS}

QOS_LABELS = [
    "0 — VoIP: IPTD 100 мс, IPLR 1e-3",
    "1 — интерактив: IPTD 400 мс, IPLR 1e-3",
    "2 — сигнализация: IPTD 100 мс, IPLR 1e-3",
    "3 — транзакции: IPTD 400 мс, IPLR 1e-3",
    "4 — данные: IPTD 1 с, IPLR 1e-3",
    "5 — best effort: без требований",
    "6 — ТВ (условный): IPTD 100 мс, IPLR 1e-5",
    "7 — ТВ (условный): IPTD 400 мс, IPLR 1e-5",
]
QOS_TABLE = {
    0: "IPTD ≤ 100 мс · IPDV ≤ 50 мс · IPLR ≤ 1e-3 · IPER ≤ 1e-4",
    1: "IPTD ≤ 400 мс · IPDV ≤ 50 мс · IPLR ≤ 1e-3 · IPER ≤ 1e-4",
    2: "IPTD ≤ 100 мс · IPDV не определён · IPLR ≤ 1e-3 · IPER ≤ 1e-4",
    3: "IPTD ≤ 400 мс · IPDV не определён · IPLR ≤ 1e-3 · IPER ≤ 1e-4",
    4: "IPTD ≤ 1 с · IPDV не определён · IPLR ≤ 1e-3 · IPER ≤ 1e-4",
    5: "Требования не определены (традиционный best effort)",
    6: "IPTD ≤ 100 мс · IPDV ≤ 50 мс · IPLR ≤ 1e-5 · IPER ≤ 1e-6",
    7: "IPTD ≤ 400 мс · IPDV ≤ 50 мс · IPLR ≤ 1e-5 · IPER ≤ 1e-6",
}

ABOUT_TEXT = """NetSim 2.0 — программный комплекс для оценки функциональной
живучести гетерогенной информационной инфраструктуры с учётом
требований качества обслуживания IP-сетей.

МАТЕМАТИЧЕСКАЯ МОДЕЛЬ
────────────────────────────────────────────────────────────────
1. Каждый канал (дуга направленного графа) описывается пропускной
   способностью C, базовой задержкой d0 и текущей загрузкой λ.
   Система массового обслуживания M/M/1: μ = C / L, ρ = λ / μ.
   Средняя задержка: IPTD_e = d0 + 1/(μ − λ),  ρ < 1.
   Потери (M/M/1/K): IPLR_e = (1−ρ)ρ^K / (1−ρ^(K+1)).
   Изменение задержки: IPDV_e ≈ −ln(10^−3)/(μ − λ).

2. Трафик All-to-All распределяется итеративно: алгоритм Дейкстры
   использует веса, зависящие от загрузки, затем нагрузки
   пересчитываются методом последовательных средних (MSA).

3. Проверка пути по Y.1541: IPTD складывается, IPLR агрегируется как
   1 − Π(1−IPLR_e), IPDV — по среднеквадратичному приближению.

4. QoS-эффективность:
   E_QoS(t) = 1/(N(N−1)) · Σ [1/d_ij · I(QoS_ij выполнено)].
   Нормировка: E_QoS_norm = E_QoS(t) / E_QoS(0).
   Дополнительно: LCC, φ_w (взвешенная живучесть по LCC), доля пар
   с выполненным QoS, IPTD/IPLR.

5. Атаки — пуассоновский поток (targeted / random / cascading).
   Восстановление — дискретно-событийная модель с пулом бригад
   repair_slots, стоимостью C_repair и вероятностью P_recovery.
   Диспетчер FIFO или Triage (по весу критичности).

СТАТИСТИКА
────────────────────────────────────────────────────────────────
Монте-Карло, 95% доверительные интервалы Стьюдента, U-критерий
Манна–Уитни с поправкой Холма.

УПРАВЛЕНИЕ
────────────────────────────────────────────────────────────────
Параметры → Запустить. Результаты загружаются автоматически.
Графики можно сохранить в PNG. CSV лежат в каталоге прогона.
"""


class App(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()
        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")

        self.title("NetSim — живучесть и QoS инфраструктуры (ВКР)")
        self.geometry("1360x860")
        self.minsize(1180, 760)
        self.configure(fg_color=plots.BG)
        self.after(150, self._maximize)

        self.core_path = core.find_core()
        self.events: queue.Queue[tuple] = queue.Queue()
        self.runner = core.CoreRunner(self.events)

        self.current_page = "params"
        self.run_dir: Path | None = None
        self.ts_data: dict[str, object] = {}
        self.topo_df = None
        self.sweep_data: dict = {}
        self.attack_df = None
        self.strategy_df = None
        self.structure_df = None
        self.degree_data: dict = {}
        self._runs: dict[str, Path] = {}
        self._selftest = False
        self._selftest_screenshot: Path | None = None

        self.vars: dict[str, tk.StringVar] = {}
        self.topo_vars: dict[str, tk.BooleanVar] = {}
        self.attack_vars: dict[str, tk.BooleanVar] = {}
        self.card_values: dict[str, ctk.CTkLabel] = {}
        self.figures: dict[str, Figure] = {}
        self.canvases: dict[str, FigureCanvasTkAgg] = {}

        self._build_fonts()
        self._build_layout()
        self._build_params_page()
        self._build_results_page()
        self._build_about_page()
        self._show_page("params")

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self.after(80, self._poll_queue)

    # ------------------------------------------------------------------
    # Оформление
    # ------------------------------------------------------------------
    def _build_fonts(self) -> None:
        self.font_title = ctk.CTkFont(family="Segoe UI", size=25, weight="bold")
        self.font_subtitle = ctk.CTkFont(family="Segoe UI", size=12)
        self.font_section = ctk.CTkFont(family="Segoe UI", size=15, weight="bold")
        self.font_label = ctk.CTkFont(family="Segoe UI", size=12)
        self.font_small = ctk.CTkFont(family="Segoe UI", size=11)
        self.font_value = ctk.CTkFont(family="Segoe UI", size=22, weight="bold")

    def _build_layout(self) -> None:
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(0, weight=1)

        sidebar = ctk.CTkFrame(self, width=232, corner_radius=0,
                               fg_color="#0a0e13")
        sidebar.grid(row=0, column=0, sticky="nsew")
        sidebar.grid_propagate(False)

        ctk.CTkLabel(sidebar, text="NetSim", font=self.font_title,
                     text_color=plots.FG, anchor="w").pack(
            fill="x", padx=20, pady=(22, 0))
        ctk.CTkLabel(sidebar, text="живучесть · QoS · Монте-Карло",
                     font=self.font_small, text_color=plots.MUTED,
                     anchor="w").pack(fill="x", padx=20, pady=(0, 20))

        self.nav_buttons: dict[str, ctk.CTkButton] = {}
        for key, label, icon in (
            ("params", "  Параметры", "⚙"),
            ("results", "  Результаты", "▤"),
            ("about", "  О модели", "ⓘ"),
        ):
            button = ctk.CTkButton(
                sidebar, text=icon + label, anchor="w", height=40,
                corner_radius=8, font=self.font_label,
                fg_color="transparent", text_color=plots.FG,
                hover_color="#1b2430",
                command=lambda k=key: self._show_page(k),
            )
            button.pack(fill="x", padx=12, pady=2)
            self.nav_buttons[key] = button

        spacer = ctk.CTkFrame(sidebar, fg_color="transparent")
        spacer.pack(fill="both", expand=True)

        if self.core_path:
            core_text = f"Ядро: {self.core_path.name}"
            core_color = plots.C_EPHYS
        else:
            core_text = "Ядро не найдено\nСоберите dipl.vcxproj"
            core_color = plots.C_IPTD
        self.core_status = ctk.CTkLabel(
            sidebar, text=core_text, font=self.font_small,
            text_color=core_color, anchor="w", justify="left")
        self.core_status.pack(fill="x", padx=20, pady=(0, 4))
        ctk.CTkLabel(sidebar, text="NetSim 2.0 · ВКР · 2026",
                     font=self.font_small, text_color=plots.MUTED,
                     anchor="w").pack(fill="x", padx=20, pady=(0, 16))

        self.content = ctk.CTkFrame(self, fg_color=plots.BG, corner_radius=0)
        self.content.grid(row=0, column=1, sticky="nsew")
        self.content.grid_rowconfigure(0, weight=1)
        self.content.grid_columnconfigure(0, weight=1)

        self.pages: dict[str, ctk.CTkFrame] = {}
        for key in ("params", "results", "about"):
            page = ctk.CTkFrame(self.content, fg_color=plots.BG, corner_radius=0)
            page.grid(row=0, column=0, sticky="nsew")
            self.pages[key] = page

    def _maximize(self) -> None:
        try:
            self.state("zoomed")
        except Exception:
            pass

    def _show_page(self, key: str) -> None:
        self.current_page = key
        self.pages[key].tkraise()
        for name, button in self.nav_buttons.items():
            if name == key:
                button.configure(fg_color="#1f6feb", text_color="white")
            else:
                button.configure(fg_color="transparent", text_color=plots.FG)
        if key == "results":
            self._refresh_runs()

    # ------------------------------------------------------------------
    # Страница параметров
    # ------------------------------------------------------------------
    def _section(self, parent, title: str, subtitle: str = "") -> ctk.CTkFrame:
        frame = ctk.CTkFrame(parent, fg_color=plots.PANEL, corner_radius=10,
                             border_width=1, border_color=plots.GRID)
        frame.pack(fill="x", padx=18, pady=(0, 12))
        ctk.CTkLabel(frame, text=title, font=self.font_section,
                     text_color=plots.FG, anchor="w").pack(
            fill="x", padx=16, pady=(12, 0))
        if subtitle:
            ctk.CTkLabel(frame, text=subtitle, font=self.font_small,
                         text_color=plots.MUTED, anchor="w",
                         justify="left").pack(fill="x", padx=16, pady=(2, 0))
        body = ctk.CTkFrame(frame, fg_color="transparent")
        body.pack(fill="x", padx=16, pady=(8, 14))
        body.grid_columnconfigure(1, weight=0)
        body.grid_columnconfigure(2, weight=1)
        return body

    def _row(self, body, row: int, label: str, widget, hint: str = "") -> None:
        ctk.CTkLabel(body, text=label, font=self.font_label,
                     text_color=plots.FG, anchor="w").grid(
            row=row, column=0, sticky="w", pady=5, padx=(0, 14))
        widget.grid(row=row, column=1, sticky="w", pady=5)
        if hint:
            ctk.CTkLabel(body, text=hint, font=self.font_small,
                         text_color=plots.MUTED, anchor="w").grid(
                row=row, column=2, sticky="w", padx=(12, 0), pady=5)

    def _entry(self, body, row: int, label: str, key: str,
               default: str, hint: str = "", width: int = 170) -> None:
        var = tk.StringVar(value=default)
        self.vars[key] = var
        entry = ctk.CTkEntry(body, textvariable=var, width=width,
                             fg_color="#0d1117", border_color=plots.GRID,
                             text_color=plots.FG, font=self.font_label)
        self._row(body, row, label, entry, hint)

    def _option(self, body, row: int, label: str, values: list[str],
                default: str, command=None) -> tk.StringVar:
        var = tk.StringVar(value=default)
        menu = ctk.CTkOptionMenu(body, values=values, variable=var,
                                 width=300, fg_color="#0d1117",
                                 button_color="#1f6feb",
                                 button_hover_color="#388bfd",
                                 text_color=plots.FG,
                                 dropdown_fg_color=plots.PANEL,
                                 dropdown_text_color=plots.FG,
                                 font=self.font_label, command=command)
        self._row(body, row, label, menu)
        return var

    def _checkbox_row(self, body, row: int, label: str,
                      options: list[tuple[str, tk.BooleanVar]]) -> None:
        frame = ctk.CTkFrame(body, fg_color="transparent")
        for text, var in options:
            ctk.CTkCheckBox(frame, text=text, variable=var,
                            font=self.font_label, text_color=plots.FG,
                            fg_color="#1f6feb", hover_color="#388bfd",
                            border_color=plots.MUTED,
                            checkbox_width=18, checkbox_height=18).pack(
                side="left", padx=(0, 14), pady=2)
        self._row(body, row, label, frame)

    def _build_params_page(self) -> None:
        page = self.pages["params"]
        page.grid_rowconfigure(0, weight=1)
        page.grid_columnconfigure(0, weight=1)

        controls = ctk.CTkFrame(page, fg_color=plots.PANEL, corner_radius=0,
                                border_width=1, border_color=plots.GRID)
        controls.grid(row=1, column=0, sticky="ew")
        controls.grid_columnconfigure(0, weight=1)

        scroll = ctk.CTkScrollableFrame(page, fg_color=plots.BG,
                                       scrollbar_button_color=plots.GRID,
                                       scrollbar_button_hover_color="#484f58")
        scroll.grid(row=0, column=0, sticky="nsew")

        # 1. Сценарий и топологии --------------------------------------
        body = self._section(
            scroll, "Сценарий и топология",
            "Выберите постановку эксперимента и набор топологий для сравнения.")
        self.scenario_display = self._option(
            body, 0, "Сценарий", [label for _, label in SCENARIOS],
            LABEL_BY_SCENARIO["topology"], command=self._on_scenario_changed)
        self.scenario_hint = ctk.CTkLabel(
            body, text="Сравнение звёздной, кольцевой, полносвязной, "
                       "случайной и безмасштабной топологий.",
            font=self.font_small, text_color=plots.MUTED, anchor="w",
            justify="left", wraplength=760)
        self.scenario_hint.grid(row=1, column=0, columnspan=3, sticky="w",
                                pady=(2, 6))

        for name in TOPO_ORDER:
            self.topo_vars[name] = tk.BooleanVar(value=True)
        topo_frame = ctk.CTkFrame(body, fg_color="transparent")
        for index, name in enumerate(TOPO_ORDER):
            ctk.CTkCheckBox(
                topo_frame, text=TOPO_LABELS[name],
                variable=self.topo_vars[name], font=self.font_label,
                text_color=plots.FG, fg_color="#1f6feb",
                hover_color="#388bfd", border_color=plots.MUTED,
                checkbox_width=18, checkbox_height=18).grid(
                row=0, column=index, padx=(0, 14), pady=2, sticky="w")
        self._row(body, 2, "Топологии", topo_frame)
        ctk.CTkLabel(
            body,
            text=TOPO_DESCRIPTIONS["erdos_renyi"] + "\n" +
                 TOPO_DESCRIPTIONS["barabasi_albert"],
            font=self.font_small, text_color=plots.MUTED, anchor="w",
            justify="left", wraplength=820).grid(
            row=3, column=0, columnspan=3, sticky="w", pady=(2, 6))

        self._entry(body, 4, "Узлов N", "num_nodes", "50",
                    "2…1000", 120)
        self._entry(body, 5, "Seed", "seed", "42", "0 = случайный", 120)

        # 2. Масштаб ---------------------------------------------------
        body = self._section(
            scroll, "Масштаб моделирования",
            "Горизонт, дискретизация и число независимых прогонов Монте-Карло.")
        self._entry(body, 0, "Прогонов MC", "n_sims", "20", "1…1000", 120)
        self._entry(body, 1, "Горизонт T", "T", "150.0", "единиц времени", 120)
        self._entry(body, 2, "Точек ряда", "sample_points", "200",
                    "2…100000", 120)

        # 3. Атаки -----------------------------------------------------
        body = self._section(
            scroll, "Деструктивные воздействия",
            "Пуассоновский поток атак и вектор поражения узлов.")
        self.attack_strategy_var = self._option(
            body, 0, "Стратегия атаки",
            ["targeted", "random", "cascading"], "targeted")
        for name, label in (("targeted", "целевая"), ("random", "случайная"),
                            ("cascading", "каскадная")):
            self.attack_vars[name] = tk.BooleanVar(value=True)
        attack_frame = ctk.CTkFrame(body, fg_color="transparent")
        for index, (name, label) in enumerate(
                (("targeted", "целевая"), ("random", "случайная"),
                 ("cascading", "каскадная"))):
            ctk.CTkCheckBox(
                attack_frame, text=label, variable=self.attack_vars[name],
                font=self.font_label, text_color=plots.FG,
                fg_color="#1f6feb", hover_color="#388bfd",
                border_color=plots.MUTED, checkbox_width=18,
                checkbox_height=18).grid(row=0, column=index,
                                         padx=(0, 14), pady=2, sticky="w")
        self._row(body, 1, "Векторы (attack)", attack_frame)
        self._entry(body, 2, "Интенсивность λ", "attack_lambda", "1.0",
                    "атак / ед. времени", 120)
        self._entry(body, 3, "P_attack", "base_P_attack", "0.60",
                    "вероятность успеха", 120)

        # 4. Восстановление -------------------------------------------
        body = self._section(
            scroll, "Восстановление",
            "Пул бригад, стоимость ремонта и политика диспетчеризации.")
        self.gatekeeper_var = self._option(
            body, 0, "Диспетчер", ["fifo", "triage"], "fifo")
        self._entry(body, 1, "Бригад (слотов)", "repair_slots", "3",
                    "параллельных ремонтов", 120)
        self._entry(body, 2, "Ресурсы R", "global_resources", "200.0",
                    "единиц ресурса", 120)
        self._entry(body, 3, "P_recovery", "base_P_recovery", "0.80",
                    "вероятность успеха", 120)
        self._entry(body, 4, "μ (скорость ремонта)", "base_mu", "0.20",
                    "1 / ед. времени", 120)
        self._entry(body, 5, "Уровни R (strategy)", "resource_levels",
                    "50,100,200,500", "через запятую", 240)

        # 5. QoS -------------------------------------------------------
        body = self._section(
            scroll, "Качество обслуживания (МСЭ-Т Y.1541)",
            "Класс QoS задаёт пороги IPTD, IPDV и IPLR для проверки путей.")
        self.qos_display = self._option(
            body, 0, "Класс QoS", QOS_LABELS, QOS_LABELS[1],
            command=self._on_qos_changed)
        self.qos_hint = ctk.CTkLabel(
            body, text=QOS_TABLE[1], font=self.font_small,
            text_color=plots.C_EQOS,
            anchor="w", justify="left", wraplength=760)
        self.qos_hint.grid(row=1, column=0, columnspan=3, sticky="w",
                           pady=(0, 8))
        self._entry(body, 2, "Трафик на пару", "demand_mbps", "2.0",
                    "Мбит/с", 120)
        self._entry(body, 3, "Размер пакета", "packet_bytes", "1500",
                    "байт (Y.1541: 1500)", 120)
        self._entry(body, 4, "Буфер очереди", "buffer_packets", "100",
                    "пакетов M/M/1/K", 120)

        # 6. Каналы и нагрузка ----------------------------------------
        body = self._section(
            scroll, "Каналы и нагрузка",
            "Физические параметры дуг и режим перераспределения трафика.")
        preset = ctk.CTkSegmentedButton(
            body, values=["Базовая", "Перегрузка", "Стресс"],
            command=self._apply_preset, font=self.font_label,
            selected_color="#1f6feb", selected_hover_color="#388bfd",
            unselected_color="#0d1117", unselected_hover_color="#1b2430",
            text_color=plots.FG)
        preset.set("Базовая")
        self._row(body, 0, "Профиль нагрузки", preset)
        self._entry(body, 1, "Доступ, Мбит/с", "access_mbps", "1000",
                    "пропускная способность", 120)
        self._entry(body, 2, "Ядро, Гбит/с", "core_gbps", "10",
                    "пропускная способность", 120)
        self._entry(body, 3, "Базовая задержка", "base_latency_ms", "2.0",
                    "мс", 120)
        self.preserve_var = tk.BooleanVar(value=True)
        switch = ctk.CTkSwitch(body, text="Сохранять суммарный трафик",
                               variable=self.preserve_var,
                               font=self.font_label, text_color=plots.FG,
                               progress_color="#1f6feb")
        self._row(body, 4, "Режим нагрузки", switch,
                  "трафик выбывших узлов перераспределяется")
        self._entry(body, 5, "Итераций MSA", "traffic_iterations", "3",
                    "1…100", 120)
        self._entry(body, 6, "Плата за транзит", "hop_penalty_ms", "5.0",
                    "мс на узел", 120)

        # 7. Генератор -------------------------------------------------
        body = self._section(
            scroll, "Генератор топологии",
            "Параметры сложных сетей и профилирование узлов.")
        self.hetero_var = tk.BooleanVar(value=True)
        switch = ctk.CTkSwitch(body, text="Гетерогенные узлы",
                               variable=self.hetero_var,
                               font=self.font_label, text_color=plots.FG,
                               progress_color="#1f6feb")
        self._row(body, 0, "Профили", switch, "server / switch / host")
        self._entry(body, 1, "ER: вероятность p", "er_p", "0.10",
                    "⟨k⟩ = p(N−1)", 120)
        self._entry(body, 2, "BA: связей m", "ba_m", "2",
                    "γ ≈ 3; m=2 — хрупкая", 120)
        self._entry(body, 3, "Джиттер задержки", "latency_jitter_ms", "1.0",
                    "мс", 120)

        # 8. Sweep -----------------------------------------------------
        body = self._section(
            scroll, "Параметрический анализ",
            "Диапазоны задаются тройкой: MIN MAX ТОЧЕК. Используются в сценарии sweep.")
        self._entry(body, 0, "Диапазон λ", "sweep_lambda", "0.5 5.0 8",
                    "MIN MAX ТОЧЕК", 200)
        self._entry(body, 1, "Диапазон R", "sweep_resources", "50 1000 8",
                    "MIN MAX ТОЧЕК", 200)
        self._entry(body, 2, "Диапазон μ", "sweep_mu", "0.05 0.80 8",
                    "MIN MAX ТОЧЕК", 200)
        self._entry(body, 3, "Диапазон P_attack", "sweep_p_attack",
                    "0.30 0.95 8", "MIN MAX ТОЧЕК", 200)

        # Нижняя панель управления ------------------------------------
        top = ctk.CTkFrame(controls, fg_color="transparent")
        top.pack(fill="x", padx=16, pady=(10, 4))
        self.run_button = ctk.CTkButton(
            top, text="▶  Запустить", width=160, height=38,
            fg_color="#1f6feb", hover_color="#388bfd",
            font=self.font_label, command=self._start_run)
        self.run_button.pack(side="left")
        self.stop_button = ctk.CTkButton(
            top, text="■  Остановить", width=140, height=38,
            fg_color="#30363d", hover_color="#484f58",
            font=self.font_label, state="disabled", command=self._stop_run)
        self.stop_button.pack(side="left", padx=(8, 0))
        ctk.CTkButton(
            top, text="Открыть каталог", width=140, height=38,
            fg_color="transparent", border_width=1, border_color=plots.GRID,
            text_color=plots.FG, font=self.font_label,
            command=self._open_run_dir).pack(side="left", padx=(8, 0))

        self.percent_label = ctk.CTkLabel(
            top, text="0%", font=self.font_label, text_color=plots.MUTED)
        self.percent_label.pack(side="right")

        self.progress = ctk.CTkProgressBar(
            controls, height=10, corner_radius=5,
            fg_color="#0d1117", progress_color="#1f6feb")
        self.progress.pack(fill="x", padx=16, pady=(2, 4))
        self.progress.set(0)

        self.status_var = tk.StringVar(value="Готов к запуску")
        ctk.CTkLabel(controls, textvariable=self.status_var,
                     font=self.font_small, text_color=plots.MUTED,
                     anchor="w").pack(fill="x", padx=16, pady=(0, 4))

        self.log_box = ctk.CTkTextbox(
            controls, height=96, fg_color="#0d1117", border_width=1,
            border_color=plots.GRID, text_color=plots.MUTED,
            font=("Consolas", 10), wrap="none")
        self.log_box.pack(fill="x", padx=16, pady=(0, 10))
        self.log_box.configure(state="disabled")

    # ------------------------------------------------------------------
    # Обработчики параметров
    # ------------------------------------------------------------------
    def _on_scenario_changed(self, label: str) -> None:
        hints = {
            "Сравнение топологий": "Сравнение топологий по E_QoS, LCC, φ_w, "
                                   "IPTD и IPLR; попарные тесты Манна–Уитни.",
            "Параметрический анализ": "Однофакторный sweep по λ, R, μ и "
                                      "P_attack для выбранных топологий.",
            "FIFO vs Triage": "Сравнение политик восстановления при разных "
                              "уровнях ресурсов R.",
            "Векторы атак": "Сравнение целевых, случайных и каскадных атак.",
            "Временные ряды": "Одна реализация с детальным рядом метрик.",
            "Все сценарии": "Последовательный запуск всех сценариев.",
        }
        self.scenario_hint.configure(text=hints.get(label, ""))

    def _on_qos_changed(self, label: str) -> None:
        try:
            cls = int(label.split()[0])
        except Exception:
            cls = 1
        self.qos_hint.configure(text=QOS_TABLE.get(cls, ""))

    def _apply_preset(self, name: str) -> None:
        presets = {
            "Базовая": ("2.0", "1000", "10", "100", "2.0"),
            "Перегрузка": ("20.0", "100", "1", "50", "2.0"),
            "Стресс": ("100.0", "100", "1", "20", "2.0"),
        }
        demand, access, core, buffer, latency = presets[name]
        self.vars["demand_mbps"].set(demand)
        self.vars["access_mbps"].set(access)
        self.vars["core_gbps"].set(core)
        self.vars["buffer_packets"].set(buffer)
        self.vars["base_latency_ms"].set(latency)

    # ------------------------------------------------------------------
    # Сбор конфигурации
    # ------------------------------------------------------------------
    def _collect_config(self) -> tuple[dict | None, str | None]:
        def as_int(key: str, lo: int, hi: int) -> int:
            raw = self.vars[key].get().strip().replace(",", ".")
            value = int(float(raw))
            if not lo <= value <= hi:
                raise ValueError(f"{key}: требуется {lo}…{hi}")
            return value

        def as_float(key: str, lo: float, hi: float) -> float:
            raw = self.vars[key].get().strip().replace(",", ".")
            value = float(raw)
            if not lo <= value <= hi:
                raise ValueError(f"{key}: требуется {lo}…{hi}")
            return value

        try:
            topologies = [name for name, var in self.topo_vars.items()
                          if var.get()]
            if not topologies:
                raise ValueError("Выберите хотя бы одну топологию")

            strategies = [name for name, var in self.attack_vars.items()
                          if var.get()]
            if not strategies:
                strategies = [self.attack_strategy_var.get()]

            scenario = SCENARIO_BY_LABEL[self.scenario_display.get()]
            qos_class = int(self.qos_display.get().split()[0])

            config = core.default_config()
            config.update({
                "scenario": scenario,
                "topology": topologies[0],
                "topologies": ",".join(topologies),
                "num_nodes": as_int("num_nodes", 2, 1000),
                "n_sims": as_int("n_sims", 1, 1000),
                "T": as_float("T", 0.1, 100000.0),
                "sample_points": as_int("sample_points", 2, 100000),
                "seed": as_int("seed", 0, 2_000_000_000),
                "attack_lambda": as_float("attack_lambda", 0.0, 100.0),
                "attack_strategy": self.attack_strategy_var.get(),
                "attack_strategies": ",".join(strategies),
                "base_P_attack": as_float("base_P_attack", 0.0, 1.0),
                "gatekeeper": self.gatekeeper_var.get(),
                "repair_slots": as_int("repair_slots", 0, 100),
                "global_resources": as_float("global_resources", 0.0, 1e7),
                "base_P_recovery": as_float("base_P_recovery", 0.0, 1.0),
                "base_mu": as_float("base_mu", 0.001, 100.0),
                "qos_class": qos_class,
                "demand_per_pair": as_float("demand_mbps", 0.0, 1e6) * 1e6,
                "packet_bits": as_float("packet_bytes", 64.0, 65535.0) * 8.0,
                "buffer_packets": as_int("buffer_packets", 1, 100000),
                "heterogeneous": 1 if self.hetero_var.get() else 0,
                "er_p": as_float("er_p", 0.0, 1.0),
                "ba_m": as_int("ba_m", 1, 1000),
                "access_capacity": as_float("access_mbps", 0.1, 1e9) * 1e6,
                "core_capacity": as_float("core_gbps", 0.001, 1e6) * 1e9,
                "base_latency": as_float("base_latency_ms", 0.001, 1000.0) / 1000.0,
                "latency_jitter": as_float("latency_jitter_ms", 0.0, 1000.0) / 1000.0,
                "traffic_iterations": as_int("traffic_iterations", 1, 100),
                "hop_penalty": as_float("hop_penalty_ms", 0.0, 1000.0) / 1000.0,
                "preserve_total_demand": 1 if self.preserve_var.get() else 0,
                "resource_levels": self._parse_levels(
                    self.vars["resource_levels"].get()),
                "sweep_params": "lambda,resources,mu,P_attack",
            })

            for key in ("sweep_lambda", "sweep_resources", "sweep_mu",
                        "sweep_p_attack"):
                parts = self.vars[key].get().replace(",", " ").split()
                if len(parts) != 3:
                    raise ValueError(f"{key}: нужен формат MIN MAX ТОЧЕК")
                float(parts[0]); float(parts[1]); int(float(parts[2]))
                config[key] = f"{parts[0]},{parts[1]},{parts[2]}"

            if config["ba_m"] >= config["num_nodes"]:
                raise ValueError("BA: m должно быть меньше N")
            return config, None
        except Exception as exc:
            return None, str(exc)

    @staticmethod
    def _parse_levels(text: str) -> str:
        normalized = text.replace(";", ",").replace(" ", ",").replace("\t", ",")
        values = [float(token) for token in normalized.split(",") if token.strip()]
        if not values:
            raise ValueError("Уровни R: пустой список")
        return ",".join(str(v) for v in values)

    # ------------------------------------------------------------------
    # Запуск ядра
    # ------------------------------------------------------------------
    def _start_run(self, selftest: bool = False) -> None:
        if self.runner.running:
            return
        if self.core_path is None:
            messagebox.showerror(
                "Ядро не найдено",
                "Соберите C++-ядро:\n\n"
                "  msbuild dipl.vcxproj /p:Configuration=Release /p:Platform=x64\n\n"
                "Ожидаемый путь: x64\\Release\\dipl.exe")
            return

        config, error = self._collect_config()
        if error or config is None:
            messagebox.showerror("Ошибка параметров", error or "Неизвестная ошибка")
            return

        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        run_dir = core.ROOT / "runs" / stamp
        suffix = 1
        while run_dir.exists():
            run_dir = core.ROOT / "runs" / f"{stamp}_{suffix}"
            suffix += 1
        run_dir.mkdir(parents=True, exist_ok=True)

        config["out_dir"] = str(run_dir)
        config_path = run_dir / "config.ini"
        core.write_config(config, config_path)

        self._selftest = selftest
        self.log_box.configure(state="normal")
        self.log_box.delete("1.0", "end")
        self.log_box.configure(state="disabled")
        self.progress.set(0)
        self.percent_label.configure(text="0%")
        self.status_var.set("Запуск ядра…")
        self.run_button.configure(state="disabled")
        self.stop_button.configure(state="normal")
        self._show_page("params")

        args = [self.core_path, "run", "--config", config_path,
                "--out", run_dir, "--progress"]
        try:
            self.runner.start(args, core.ROOT)
        except Exception as exc:
            self._on_done(-1, str(exc))

    def _stop_run(self) -> None:
        if self.runner.running:
            self.runner.cancel()
            self.status_var.set("Остановка процесса…")

    def _poll_queue(self) -> None:
        try:
            while True:
                kind, payload = self.events.get_nowait()
                if kind == "line":
                    self._append_log(str(payload))
                    parsed = core.parse_progress(str(payload))
                    if parsed:
                        fraction, stage = parsed
                        self.progress.set(fraction)
                        self.percent_label.configure(text=f"{fraction * 100:.0f}%")
                        self.status_var.set(stage)
                else:
                    self._on_done(int(payload))
        except queue.Empty:
            pass
        self.after(80, self._poll_queue)

    def _append_log(self, line: str) -> None:
        self.log_box.configure(state="normal")
        self.log_box.insert("end", line + "\n")
        lines = int(self.log_box.index("end-1c").split(".")[0])
        if lines > 500:
            self.log_box.delete("1.0", "100.0")
        self.log_box.see("end")
        self.log_box.configure(state="disabled")

    def _on_done(self, code: int, error: str = "") -> None:
        self.run_button.configure(state="normal")
        self.stop_button.configure(state="disabled")
        if code == 0:
            self.progress.set(1.0)
            self.percent_label.configure(text="100%")
            self.status_var.set("Готово — результаты загружены")
            self._refresh_runs()
            if self.run_dir is not None:
                self._load_run(self.run_dir)
            self._show_page("results")
            if self._selftest:
                self.after(700, self._finish_selftest)
        else:
            self.status_var.set("Ошибка выполнения")
            if error:
                self._append_log("ERROR: " + error)
            if not self._selftest:
                messagebox.showerror(
                    "Ошибка ядра",
                    "Процесс завершился с кодом " + str(code) +
                    ".\nПодробности в журнале.")

    # ------------------------------------------------------------------
    # Страница результатов
    # ------------------------------------------------------------------
    def _build_results_page(self) -> None:
        page = self.pages["results"]
        page.grid_rowconfigure(3, weight=1)
        page.grid_columnconfigure(0, weight=1)

        top = ctk.CTkFrame(page, fg_color="transparent")
        top.grid(row=0, column=0, sticky="ew", padx=18, pady=(14, 6))
        top.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(top, text="Прогон", font=self.font_label,
                     text_color=plots.FG).grid(row=0, column=0, sticky="w",
                                               padx=(0, 10))
        self.run_menu = ctk.CTkOptionMenu(
            top, values=["нет прогонов"], width=520,
            fg_color="#0d1117", button_color="#1f6feb",
            button_hover_color="#388bfd", text_color=plots.FG,
            dropdown_fg_color=plots.PANEL, dropdown_text_color=plots.FG,
            font=self.font_label, command=self._on_run_selected)
        self.run_menu.grid(row=0, column=1, sticky="w")
        ctk.CTkButton(top, text="Обновить", width=110, height=30,
                      fg_color="transparent", border_width=1,
                      border_color=plots.GRID, text_color=plots.FG,
                      command=self._refresh_runs).grid(row=0, column=2,
                                                       padx=(8, 0))
        ctk.CTkButton(top, text="PNG", width=80, height=30,
                      fg_color="transparent", border_width=1,
                      border_color=plots.GRID, text_color=plots.FG,
                      command=self._save_current_plot).grid(row=0, column=3,
                                                            padx=(8, 0))

        cards = ctk.CTkFrame(page, fg_color="transparent")
        cards.grid(row=1, column=0, sticky="ew", padx=14, pady=(0, 8))
        for index in range(6):
            cards.grid_columnconfigure(index, weight=1)
        card_specs = [
            ("e_qos_norm", "E_QoS", plots.C_EQOS),
            ("qos_ratio", "Доля пар с QoS", plots.C_Q),
            ("lcc", "LCC", plots.C_LCC),
            ("phi_w", "φ_w", plots.C_EPHYS),
            ("iptd_mean_ms", "IPTD, мс", plots.C_IPTD),
            ("iplr_mean", "IPLR", plots.C_IPLR),
        ]
        for index, (key, title, color) in enumerate(card_specs):
            card = ctk.CTkFrame(cards, fg_color=plots.PANEL, corner_radius=10,
                                border_width=1, border_color=plots.GRID)
            card.grid(row=0, column=index, sticky="nsew", padx=5)
            ctk.CTkLabel(card, text=title, font=self.font_small,
                         text_color=plots.MUTED).pack(pady=(9, 0))
            value = ctk.CTkLabel(card, text="—", font=self.font_value,
                                 text_color=color)
            value.pack(pady=(0, 9))
            self.card_values[key] = value

        self.summary_label = ctk.CTkLabel(
            page, text="Выберите прогон", font=self.font_small,
            text_color=plots.MUTED, anchor="w")
        self.summary_label.grid(row=2, column=0, sticky="w", padx=24,
                                pady=(0, 2))

        self.tabview = ctk.CTkTabview(
            page, fg_color=plots.PANEL, border_width=1,
            border_color=plots.GRID,
            segmented_button_fg_color="#0d1117",
            segmented_button_selected_color="#1f6feb",
            segmented_button_selected_hover_color="#388bfd",
            segmented_button_unselected_color="#0d1117",
            segmented_button_unselected_hover_color="#1b2430",
            text_color=plots.FG, command=self._on_tab_changed)
        self.tabview.grid(row=3, column=0, sticky="nsew", padx=14,
                          pady=(2, 12))

        for name in ("Динамика", "Топологии", "Sweep", "Атаки",
                     "FIFO/Triage", "Структура"):
            tab = self.tabview.add(name)
            tab.grid_columnconfigure(0, weight=1)
            canvas_col = 0
            if name == "Динамика":
                bar = ctk.CTkFrame(tab, fg_color="transparent")
                bar.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 0))
                ctk.CTkLabel(bar, text="Топология:", font=self.font_small,
                             text_color=plots.MUTED).pack(side="left")
                self.ts_topo_var = tk.StringVar(value="")
                self.ts_topo_menu = ctk.CTkOptionMenu(
                    bar, values=["—"], variable=self.ts_topo_var, width=200,
                    fg_color="#0d1117", button_color="#1f6feb",
                    button_hover_color="#388bfd", text_color=plots.FG,
                    dropdown_fg_color=plots.PANEL,
                    dropdown_text_color=plots.FG, font=self.font_small,
                    command=self._on_ts_topo_changed)
                self.ts_topo_menu.pack(side="left", padx=(8, 0))
                canvas_row = 1
            elif name == "FIFO/Triage":
                bar = ctk.CTkFrame(tab, fg_color="transparent")
                bar.grid(row=0, column=0, sticky="ew", padx=8, pady=(8, 0))
                ctk.CTkLabel(bar, text="Метрика:", font=self.font_small,
                             text_color=plots.MUTED).pack(side="left")
                self.strategy_metric_var = tk.StringVar(value="E_QoS")
                ctk.CTkSegmentedButton(
                    bar, values=["E_QoS", "φ_w"],
                    variable=self.strategy_metric_var, width=180,
                    selected_color="#1f6feb", selected_hover_color="#388bfd",
                    unselected_color="#0d1117",
                    unselected_hover_color="#1b2430",
                    text_color=plots.FG, font=self.font_small,
                    command=lambda _: self._draw_active_tab()).pack(
                    side="left", padx=(8, 0))
                canvas_row = 1
            elif name == "Структура":
                tab.grid_columnconfigure(0, weight=0)
                tab.grid_columnconfigure(1, weight=1)
                bar = ctk.CTkFrame(tab, fg_color="transparent")
                bar.grid(row=0, column=0, columnspan=2, sticky="ew",
                         padx=8, pady=(8, 0))
                ctk.CTkLabel(bar, text="Топология:", font=self.font_small,
                             text_color=plots.MUTED).pack(side="left")
                self.structure_topo_var = tk.StringVar(value="")
                self.structure_topo_menu = ctk.CTkOptionMenu(
                    bar, values=["—"], variable=self.structure_topo_var,
                    width=220, fg_color="#0d1117", button_color="#1f6feb",
                    button_hover_color="#388bfd", text_color=plots.FG,
                    dropdown_fg_color=plots.PANEL,
                    dropdown_text_color=plots.FG, font=self.font_small,
                    command=self._on_structure_topo_changed)
                self.structure_topo_menu.pack(side="left", padx=(8, 0))

                self.structure_text = ctk.CTkTextbox(
                    tab, width=370, fg_color="#0d1117",
                    border_width=1, border_color=plots.GRID,
                    text_color=plots.FG, font=("Consolas", 11), wrap="word")
                self.structure_text.grid(row=1, column=0, sticky="nsew",
                                         padx=(8, 4), pady=6)
                self.structure_text.configure(state="disabled")
                canvas_row = 1
                canvas_col = 1
            else:
                canvas_row = 0
            tab.grid_rowconfigure(canvas_row, weight=1)
            figure = Figure(figsize=(9, 5.2), dpi=100)
            figure.patch.set_facecolor(plots.BG)
            canvas = FigureCanvasTkAgg(figure, master=tab)
            canvas.get_tk_widget().grid(row=canvas_row, column=canvas_col,
                                        sticky="nsew", padx=6, pady=6)
            self.figures[name] = figure
            self.canvases[name] = canvas

    def _refresh_runs(self) -> None:
        runs = data.list_runs()
        labels = [data.run_label(path) for path in runs]
        self._runs = dict(zip(labels, runs))
        current = self.run_menu.get()
        values = labels or ["нет прогонов"]
        self.run_menu.configure(values=values)
        if current in labels:
            self.run_menu.set(current)
        elif labels:
            self.run_menu.set(labels[0])
            self._load_run(runs[0])
        else:
            self.summary_label.configure(text="Прогонов пока нет")

    def _on_run_selected(self, label: str) -> None:
        path = self._runs.get(label)
        if path is not None:
            self._load_run(path)

    def _load_run(self, path: Path) -> None:
        self.run_dir = path
        self.ts_data = data.load_time_series(path)
        self.topo_df = data.load_topology(path)
        self.sweep_data = data.load_sweep(path)
        self.attack_df = data.load_attack(path)
        self.strategy_df = data.load_strategy(path)
        self.structure_df = data.load_structure(path)
        self.degree_data = data.load_degrees(path)

        topologies = list(self.ts_data.keys())
        self.ts_topo_menu.configure(values=topologies or ["—"])
        if topologies:
            self.ts_topo_var.set(topologies[0])
        else:
            self.ts_topo_var.set("—")

        structures: list[str] = []
        if self.structure_df is not None and not self.structure_df.empty:
            structures = [str(x) for x in self.structure_df["topology"].tolist()]
        self.structure_topo_menu.configure(values=structures or ["—"])
        preferred = ("barabasi_albert" if "barabasi_albert" in structures
                     else "erdos_renyi" if "erdos_renyi" in structures
                     else structures[0] if structures else "—")
        self.structure_topo_var.set(preferred)

        selected = self.ts_topo_var.get() if topologies else None
        summary = data.summarize_run(path, selected)
        self._update_cards(summary)
        self.summary_label.configure(
            text=f"Источник: {summary['source']}  ·  топология: "
                 f"{summary['topology']}  ·  {path.name}")

        self._draw_active_tab()

    def _on_ts_topo_changed(self, topology: str) -> None:
        if self.run_dir is None:
            return
        summary = data.summarize_run(self.run_dir, topology)
        self._update_cards(summary)
        self.summary_label.configure(
            text=f"Источник: {summary['source']}  ·  топология: "
                 f"{summary['topology']}  ·  {self.run_dir.name}")
        self._draw_active_tab()

    def _update_cards(self, summary: dict) -> None:
        metrics = summary.get("metrics", {})
        e_qos = float(metrics.get("e_qos_norm", 0.0) or 0.0)
        qos = float(metrics.get("qos_ratio", 0.0) or 0.0)
        lcc = float(metrics.get("lcc", 0.0) or 0.0)
        phi = float(metrics.get("phi_w", 0.0) or 0.0)
        iptd = float(metrics.get("iptd_mean_ms", 0.0) or 0.0)
        iplr = float(metrics.get("iplr_mean", 0.0) or 0.0)
        values = {
            "e_qos_norm": f"{e_qos:.3f}",
            "qos_ratio": f"{qos * 100:.1f}%",
            "lcc": f"{lcc:.3f}",
            "phi_w": f"{phi:.3f}",
            "iptd_mean_ms": f"{iptd:.2f}",
            "iplr_mean": f"{iplr:.2e}",
        }
        for key, text in values.items():
            self.card_values[key].configure(text=text)

    def _on_tab_changed(self) -> None:
        self._draw_active_tab()

    def _on_structure_topo_changed(self, topology: str) -> None:
        self._draw_structure()
        canvas = self.canvases.get("Структура")
        if canvas is not None:
            canvas.draw_idle()

    def _draw_structure(self) -> None:
        figure = self.figures.get("Структура")
        if figure is None:
            return

        if self.structure_df is None or self.structure_df.empty:
            self._set_structure_text(
                "Структурный анализ для этого прогона не найден.\n\n"
                "Он создаётся автоматически при запуске сценария "
                "(файл topology_structure.csv) или отдельной командой:\n"
                "dipl.exe run --scenario structure --config config.ini --out .")
            plots.empty_figure(figure)
            return

        topology = self.structure_topo_var.get()
        subset = self.structure_df[self.structure_df["topology"] == topology]
        if subset.empty:
            self._set_structure_text("Нет данных по выбранной топологии.")
            plots.empty_figure(figure)
            return

        row = subset.iloc[0]

        def value(key: str, digits: int = 3) -> str:
            try:
                number = float(row.get(key, 0.0))
            except Exception:
                return "—"
            if not math.isfinite(number):
                return "—"
            if abs(number) < 1.0 and number != 0.0:
                return f"{number:.3f}"
            return f"{number:.{digits}f}"

        lines = [
            f"Топология: {topology}",
            f"Назначение: {row.get('purpose', '—')}",
            "",
            f"Узлов N: {value('nodes', 0)}",
            f"Рёбер E: {value('edges', 0)}",
            f"Средняя степень ⟨k⟩: {value('avg_degree')}",
            f"Мин./макс. степень: {value('min_degree', 0)} / "
            f"{value('max_degree', 0)}",
            f"Коэффициент вариации k: {value('degree_cv')}",
            f"Кластеризация C: {value('clustering')}",
            f"Средняя длина пути L: {value('avg_path_length')}",
            f"Диаметр D: {value('diameter', 0)}",
            f"Ассортативность r: {value('assortativity')}",
            f"Компонент связности: {value('components', 0)}",
            f"Доля LCC: {value('lcc_fraction')}",
            f"Ожидаемая ⟨k⟩ модели: {value('expected_avg_degree')}",
            f"Наклон CCDF: {value('ccdf_slope')}",
            f"Оценка γ (Хилл): {value('gamma_est')}",
        ]
        self._set_structure_text("\n".join(lines))
        plots.plot_degree_ccdf(
            figure, self.degree_data.get(topology), row, topology)

    def _set_structure_text(self, text: str) -> None:
        self.structure_text.configure(state="normal")
        self.structure_text.delete("1.0", "end")
        self.structure_text.insert("1.0", text)
        self.structure_text.configure(state="disabled")

    def _draw_active_tab(self) -> None:
        if not self.figures:
            return
        try:
            name = self.tabview.get()
        except Exception:
            return
        figure = self.figures.get(name)
        canvas = self.canvases.get(name)
        if figure is None or canvas is None:
            return

        try:
            if name == "Динамика":
                if not self.ts_data:
                    plots.empty_figure(figure)
                else:
                    topo = self.ts_topo_var.get()
                    if topo not in self.ts_data:
                        topo = next(iter(self.ts_data))
                        self.ts_topo_var.set(topo)
                    plots.plot_timeseries(figure, self.ts_data[topo], topo)
            elif name == "Топологии":
                if self.topo_df is None:
                    plots.empty_figure(figure)
                else:
                    plots.plot_topology(figure, self.topo_df)
            elif name == "Sweep":
                if not self.sweep_data:
                    plots.empty_figure(figure)
                else:
                    plots.plot_sweep(figure, self.sweep_data)
            elif name == "Атаки":
                if self.attack_df is None:
                    plots.empty_figure(figure)
                else:
                    plots.plot_attack(figure, self.attack_df)
            elif name == "FIFO/Triage":
                if self.strategy_df is None:
                    plots.empty_figure(figure)
                else:
                    metric = ("phi_mean"
                              if self.strategy_metric_var.get() == "φ_w"
                              else "eff_qos_mean")
                    ylabel = "φ_w" if metric == "phi_mean" else "E_QoS"
                    plots.plot_strategy(figure, self.strategy_df,
                                        metric=metric, ylabel=ylabel)
            elif name == "Структура":
                self._draw_structure()
        except Exception as exc:
            figure.clear()
            axes = figure.add_subplot(111)
            plots.empty(axes, f"Ошибка построения: {exc}")
        canvas.draw_idle()

    def _save_current_plot(self) -> None:
        try:
            name = self.tabview.get()
        except Exception:
            return
        figure = self.figures.get(name)
        if figure is None:
            return
        path = filedialog.asksaveasfilename(
            title="Сохранить график",
            defaultextension=".png",
            filetypes=[("PNG", "*.png"), ("PDF", "*.pdf")],
            initialfile=f"netnsim_{name.lower().replace('/', '_')}.png")
        if path:
            figure.savefig(path, dpi=180, facecolor=figure.get_facecolor())
            self.status_var.set("График сохранён: " + path)

    # ------------------------------------------------------------------
    # Страница «О модели»
    # ------------------------------------------------------------------
    def _build_about_page(self) -> None:
        page = self.pages["about"]
        page.grid_rowconfigure(1, weight=1)
        page.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(page, text="О математической модели",
                     font=self.font_title, text_color=plots.FG,
                     anchor="w").grid(row=0, column=0, sticky="w",
                                      padx=24, pady=(20, 8))
        box = ctk.CTkTextbox(page, fg_color=plots.PANEL,
                             border_width=1, border_color=plots.GRID,
                             text_color=plots.FG, font=("Consolas", 11),
                             wrap="word")
        box.grid(row=1, column=0, sticky="nsew", padx=20, pady=(0, 20))
        box.insert("1.0", ABOUT_TEXT)
        box.configure(state="disabled")

    # ------------------------------------------------------------------
    # Self-test и скриншот
    # ------------------------------------------------------------------
    def run_selftest(self, screenshot: Path | None = None) -> None:
        self._selftest_screenshot = screenshot
        if self.core_path is None:
            print("GUI_SELFTEST_FAIL core not found")
            self.destroy()
            return
        self.scenario_display.set(LABEL_BY_SCENARIO["timeseries"])
        self._on_scenario_changed(LABEL_BY_SCENARIO["timeseries"])
        for name, var in self.topo_vars.items():
            var.set(name == "star")
        for name, var in self.attack_vars.items():
            var.set(name == "targeted")
        self.attack_strategy_var.set("targeted")
        self.gatekeeper_var.set("fifo")
        self.qos_display.set(QOS_LABELS[1])
        self._on_qos_changed(QOS_LABELS[1])
        self.vars["num_nodes"].set("20")
        self.vars["n_sims"].set("1")
        self.vars["T"].set("20")
        self.vars["sample_points"].set("40")
        self.vars["attack_lambda"].set("0.8")
        self.vars["global_resources"].set("60")
        self.vars["repair_slots"].set("2")
        self.vars["demand_mbps"].set("5")
        self.vars["access_mbps"].set("1000")
        self.vars["core_gbps"].set("10")
        self.vars["buffer_packets"].set("100")
        self._start_run(selftest=True)

    def _finish_selftest(self) -> None:
        try:
            self.update_idletasks()
            self.update()
            if self.run_dir is not None:
                for name, figure in self.figures.items():
                    try:
                        plots.save_figure(
                            figure, self.run_dir /
                            f"gui_{name.lower().replace('/', '_')}.png")
                    except Exception:
                        pass
            if self._selftest_screenshot:
                self.capture_window(self._selftest_screenshot)
            print("GUI_SELFTEST_OK", self.run_dir)
        finally:
            self.after(300, self.destroy)

    # ------------------------------------------------------------------
    def _open_run_dir(self) -> None:
        if self.run_dir is not None:
            core.open_in_explorer(self.run_dir)

    def open_run(self, path: Path, tab: str | None = None) -> None:
        """Открывает существующий прогон (для демонстрации и тестов)."""
        self._show_page("results")
        self._refresh_runs()
        label = data.run_label(path)
        if label in self._runs:
            self.run_menu.set(label)
        self._load_run(path)
        if tab:
            try:
                self.tabview.set(tab)
            except Exception:
                pass
            self._draw_active_tab()

    def capture_window(self, path: Path) -> None:
        self.deiconify()
        self.lift()
        try:
            self.attributes("-topmost", True)
        except Exception:
            pass
        self.focus_force()
        self.update_idletasks()
        self.update()
        time.sleep(0.5)
        self.update()
        try:
            from PIL import ImageGrab
            scale = self.winfo_fpixels("1i") / 96.0
            x = int(self.winfo_rootx() * scale)
            y = int(self.winfo_rooty() * scale)
            width = int(self.winfo_width() * scale)
            height = int(self.winfo_height() * scale)
            image = ImageGrab.grab(bbox=(x, y, x + width, y + height))
            path.parent.mkdir(parents=True, exist_ok=True)
            image.save(path)
            print("SCREENSHOT", path)
        except Exception as exc:
            print("SCREENSHOT_ERROR", exc)
        finally:
            try:
                self.attributes("-topmost", False)
            except Exception:
                pass

    def _on_close(self) -> None:
        if self.runner.running:
            self.runner.cancel()
        self.destroy()
