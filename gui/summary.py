"""Компактные сводные таблицы по сценариям для анализа и отчётов."""

from __future__ import annotations

import math
from pathlib import Path

import pandas as pd

from . import data

TOPO_ORDER = {
    "star": 0,
    "ring": 1,
    "full_mesh": 2,
    "erdos_renyi": 3,
    "barabasi_albert": 4,
}
ATTACK_ORDER = {"targeted": 0, "random": 1, "cascading": 2}
PARAM_LABELS = {
    "lambda": "λ",
    "resources": "R",
    "mu": "μ",
    "P_attack": "P_attack",
    "demand": "Трафик",
}


def _num(value, digits: int = 3) -> str:
    try:
        number = float(value)
    except Exception:
        return "—"
    if not math.isfinite(number):
        return "—"
    return f"{number:.{digits}f}".replace(".", ",")


def _sci(value) -> str:
    try:
        number = float(value)
    except Exception:
        return "—"
    if not math.isfinite(number):
        return "—"
    if number == 0.0:
        return "0"
    if abs(number) < 1e-2:
        return f"{number:.2e}".replace(".", ",")
    return f"{number:.4f}".replace(".", ",")


def _p(value) -> str:
    try:
        number = float(value)
    except Exception:
        return "—"
    if not math.isfinite(number):
        return "—"
    if number < 0.001:
        return "<0,001"
    return f"{number:.3f}".replace(".", ",")


def _direction(value) -> str:
    text = str(value)
    if text.startswith("triage"):
        return "Triage > FIFO"
    if text.startswith("fifo"):
        return "FIFO > Triage"
    return "~"


def _mean(series) -> float:
    return float(series.mean()) if len(series) else 0.0


def _sort_topologies(frame: pd.DataFrame) -> pd.DataFrame:
    if "topology" not in frame.columns:
        return frame
    order = frame["topology"].map(TOPO_ORDER).fillna(99)
    return frame.assign(_order=order).sort_values(
        ["_order", "topology"]).drop(columns="_order")


def _topology_table(run_dir: Path):
    frame = data.load_topology(run_dir)
    if frame is None:
        return [], [], ""
    frame = _sort_topologies(frame)
    headers = ["Топология", "LCC", "φ_w", "E_QoS", "Q, %", "IPTD, мс", "IPLR"]
    rows = []
    for _, row in frame.iterrows():
        rows.append([
            str(row.get("topology", "—")),
            _num(row.get("lcc_mean", 0.0)),
            _num(row.get("phi_w_mean", 0.0)),
            _num(row.get("eff_qos_mean", 0.0)),
            _num(float(row.get("qos_ratio_mean", 0.0)) * 100.0, 1),
            _num(row.get("iptd_mean_ms", 0.0), 2),
            _sci(row.get("iplr_mean", 0.0)),
        ])
    return headers, rows, (
        "E_QoS — нормированная QoS-эффективность; Q — доля пар, "
        "выполнивших SLA; IPTD/IPLR — по достижимым путям без перегрузки.")


def _sweep_table(run_dir: Path, param: str):
    path = run_dir / f"sweep_{param}.csv"
    frame = data.read_csv(path) if path.is_file() else None
    if frame is None or frame.empty:
        return [], [], ""
    headers = ["x", "Топология", "LCC", "φ_w", "E_QoS", "Q, %"]
    rows = []
    for _, row in frame.iterrows():
        x_value = row.get("x_value", 0.0)
        for topology in sorted(TOPO_ORDER, key=TOPO_ORDER.get):
            prefix = f"{topology}_"
            if f"{prefix}lcc_mean" not in frame.columns:
                continue
            rows.append([
                _num(x_value, 2),
                topology,
                _num(row.get(f"{prefix}lcc_mean", 0.0)),
                _num(row.get(f"{prefix}phi_mean", 0.0)),
                _num(row.get(f"{prefix}eff_qos_mean", 0.0)),
                _num(float(row.get(f"{prefix}qos_mean", 0.0)) * 100.0, 1),
            ])
    label = PARAM_LABELS.get(param, param)
    return headers, rows, (
        f"x — значение параметра {label}. Показаны ключевые метрики; "
        "доверительные интервалы и p-значения — в полном CSV.")


def _strategy_table(run_dir: Path):
    frame = data.load_strategy(run_dir)
    if frame is None or frame.empty:
        return [], [], ""
    headers = ["R", "Топология", "LCC FIFO", "LCC Triage",
               "E_QoS FIFO", "E_QoS Triage",
               "φ_w FIFO", "φ_w Triage", "p(φ_w)", "Значимость"]
    rows = []
    for topology in sorted(frame["topology"].unique(),
                           key=lambda x: TOPO_ORDER.get(x, 99)):
        subset = frame[frame["topology"] == topology]
        for resource in sorted(subset["R"].unique()):
            fifo = subset[(subset["gatekeeper"] == "fifo") &
                          (subset["R"] == resource)]
            triage = subset[(subset["gatekeeper"] == "triage") &
                            (subset["R"] == resource)]
            if fifo.empty or triage.empty:
                continue
            f = fifo.iloc[0]
            t = triage.iloc[0]
            rows.append([
                _num(resource, 0), topology,
                _num(f.get("lcc_mean", 0.0)), _num(t.get("lcc_mean", 0.0)),
                _num(f.get("eff_qos_mean", 0.0)),
                _num(t.get("eff_qos_mean", 0.0)),
                _num(f.get("phi_mean", 0.0)), _num(t.get("phi_mean", 0.0)),
                _p(f.get("p_phi", 1.0)),
                _direction(f.get("sig_phi_direction", "~")),
            ])
    return headers, rows, (
        "p(φ_w) — U-критерий Манна–Уитни; «Triage > FIFO» означает значимое "
        "преимущество при p < 0,05. Исчерпание ресурсов — в полном CSV.")


def _attack_table(run_dir: Path):
    frame = data.load_attack(run_dir)
    if frame is None or frame.empty:
        return [], [], ""
    headers = ["Стратегия", "Топология", "LCC", "E_QoS", "φ_w", "Q, %", "IPLR"]
    rows = []
    ordered = frame.assign(
        _s=frame["attack_strategy"].map(ATTACK_ORDER).fillna(99),
        _t=frame["topology"].map(TOPO_ORDER).fillna(99),
    ).sort_values(["_s", "_t"])
    for _, row in ordered.iterrows():
        rows.append([
            str(row.get("attack_strategy", "—")),
            str(row.get("topology", "—")),
            _num(row.get("lcc_mean", 0.0)),
            _num(row.get("eff_qos_mean", 0.0)),
            _num(row.get("phi_mean", 0.0)),
            _num(float(row.get("qos_mean", 0.0)) * 100.0, 1),
            _sci(row.get("iplr_mean", 0.0)),
        ])
    return headers, rows, (
        "Сравнение векторов атак по топологиям; Q — доля пар с QoS, "
        "IPLR — средние потери.")


def _timeseries_table(run_dir: Path):
    series = data.load_time_series(run_dir)
    metadata = data.load_meta(run_dir)
    if not series:
        return [], [], ""
    topologies = sorted(series, key=lambda x: TOPO_ORDER.get(x, 99))
    headers = ["Метрика"] + topologies

    def value(frame, meta, name):
        if name == "lcc":
            return _num(_mean(frame["lcc"]))
        if name == "eqos":
            return _num(_mean(frame["e_qos_norm"]))
        if name == "phi":
            return _num(_mean(frame["phi_w"]))
        if name == "q":
            return _num(_mean(frame["qos_ratio"]) * 100.0, 1)
        if name == "iptd":
            return _num(_mean(frame["iptd_mean_ms"]), 2)
        if name == "iplr":
            return _sci(frame["iplr_mean"].max())
        if name == "first":
            return _num(meta.get("first_failure_time", 0.0), 2)
        if name == "res":
            return _num(meta.get("resources_consumed", 0.0), 0)
        if name == "exhaust":
            return "ДА" if str(meta.get("resource_exhausted", "0")) == "1" else "НЕТ"
        if name == "success":
            return _num(meta.get("recoveries_success", 0.0), 0)
        if name == "failed":
            return _num(meta.get("recoveries_failed", 0.0), 0)
        if name == "queue":
            return _num(meta.get("max_queue_depth", 0.0), 0)
        return "—"

    rows = []
    for title, key in (
        ("LCC (среднее)", "lcc"),
        ("E_QoS (среднее)", "eqos"),
        ("φ_w (среднее)", "phi"),
        ("Q (среднее), %", "q"),
        ("IPTD (среднее), мс", "iptd"),
        ("IPLR (макс.)", "iplr"),
        ("Первый отказ", "first"),
        ("Потреблено ресурсов", "res"),
        ("Истощение ресурсов", "exhaust"),
        ("Успешных восстановлений", "success"),
        ("Неудачных попыток", "failed"),
        ("Макс. очередь", "queue"),
    ):
        rows.append([title] + [
            value(series[topology], metadata.get(topology, {}), key)
            for topology in topologies
        ])
    return headers, rows, (
        "Интегральные показатели одной реализации: средние по времени, "
        "первый отказ, расход ресурсов и работа восстановления.")


def _structure_table(run_dir: Path):
    frame = data.load_structure(run_dir)
    if frame is None or frame.empty:
        return [], [], ""
    frame = _sort_topologies(frame)
    headers = ["Топология", "⟨k⟩", "k_max", "CV", "C", "L", "D", "r", "γ"]
    rows = []
    for _, row in frame.iterrows():
        rows.append([
            str(row.get("topology", "—")),
            _num(row.get("avg_degree", 0.0), 2),
            _num(row.get("max_degree", 0.0), 0),
            _num(row.get("degree_cv", 0.0), 2),
            _num(row.get("clustering", 0.0), 3),
            _num(row.get("avg_path_length", 0.0), 2),
            _num(row.get("diameter", 0.0), 0),
            _num(row.get("assortativity", 0.0), 2),
            _num(row.get("gamma_est", float("nan")), 2),
        ])
    return headers, rows, (
        "Структурные метрики: средняя и максимальная степень, CV, "
        "кластеризация C, длина пути L, диаметр D, ассортативность r, γ.")


def available_sets(run_dir: Path) -> list[tuple[str, str]]:
    result: list[tuple[str, str]] = []
    if (run_dir / "topology_comparison.csv").is_file():
        result.append(("Топологии", "topology"))
    for path in sorted(run_dir.glob("sweep_*.csv")):
        param = path.stem.replace("sweep_", "")
        result.append((f"Sweep: {PARAM_LABELS.get(param, param)}",
                       f"sweep:{param}"))
    if (run_dir / "strategy_comparison.csv").is_file():
        result.append(("FIFO/Triage", "strategy"))
    if (run_dir / "attack_vector_analysis.csv").is_file():
        result.append(("Атаки", "attack"))
    if any(run_dir.glob("timeseries_*.csv")):
        result.append(("Временные ряды", "timeseries"))
    if (run_dir / "topology_structure.csv").is_file():
        result.append(("Структура", "structure"))
    return result


def build(run_dir: Path, key: str):
    if key == "topology":
        return _topology_table(run_dir)
    if key.startswith("sweep:"):
        return _sweep_table(run_dir, key.split(":", 1)[1])
    if key == "strategy":
        return _strategy_table(run_dir)
    if key == "attack":
        return _attack_table(run_dir)
    if key == "timeseries":
        return _timeseries_table(run_dir)
    if key == "structure":
        return _structure_table(run_dir)
    return [], [], ""
