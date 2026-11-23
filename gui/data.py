"""Загрузка и первичная обработка CSV-отчётов ядра."""

from __future__ import annotations

from pathlib import Path

import pandas as pd

from .core import ROOT, read_config


def read_csv(path: Path) -> pd.DataFrame | None:
    try:
        return pd.read_csv(path)
    except Exception:
        return None


def list_runs() -> list[Path]:
    """Каталоги прогонов, содержащие отчёты ядра (включая вложенные)."""
    candidates: list[Path] = []
    for base in (ROOT / "runs", ROOT / "results"):
        if not base.is_dir():
            continue
        for config in base.rglob("config.ini"):
            candidates.append(config.parent)
    unique: dict[str, Path] = {}
    for path in candidates:
        if not path.is_dir():
            continue
        unique[str(path.resolve())] = path
    return sorted(unique.values(), key=lambda p: p.stat().st_mtime, reverse=True)


def time_series_files(run_dir: Path) -> dict[str, Path]:
    result: dict[str, Path] = {}
    for path in sorted(run_dir.glob("timeseries_*.csv")):
        name = path.stem.replace("timeseries_", "")
        result[name] = path
    return result


def load_time_series(run_dir: Path) -> dict[str, pd.DataFrame]:
    result: dict[str, pd.DataFrame] = {}
    for name, path in time_series_files(run_dir).items():
        frame = read_csv(path)
        if frame is not None and not frame.empty:
            result[name] = frame
    return result


def load_topology(run_dir: Path) -> pd.DataFrame | None:
    path = run_dir / "topology_comparison.csv"
    return read_csv(path) if path.is_file() else None


def load_sweep(run_dir: Path) -> dict[str, pd.DataFrame]:
    result: dict[str, pd.DataFrame] = {}
    for path in sorted(run_dir.glob("sweep_*.csv")):
        name = path.stem.replace("sweep_", "")
        frame = read_csv(path)
        if frame is not None and not frame.empty:
            result[name] = frame
    return result


def load_structure(run_dir: Path) -> pd.DataFrame | None:
    path = run_dir / "topology_structure.csv"
    return read_csv(path) if path.is_file() else None


def load_degrees(run_dir: Path) -> dict[str, pd.DataFrame]:
    result: dict[str, pd.DataFrame] = {}
    for path in sorted(run_dir.glob("topology_degrees_*.csv")):
        name = path.stem.replace("topology_degrees_", "")
        frame = read_csv(path)
        if frame is not None and not frame.empty:
            result[name] = frame
    return result


def load_meta(run_dir: Path) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for path in sorted(run_dir.glob("run_meta_*.csv")):
        name = path.stem.replace("run_meta_", "")
        frame = read_csv(path)
        if frame is None or "key" not in frame.columns:
            continue
        values: dict[str, str] = {}
        for _, row in frame.iterrows():
            values[str(row["key"])] = str(row.get("value", ""))
        result[name] = values
    return result


def load_attack(run_dir: Path) -> pd.DataFrame | None:
    path = run_dir / "attack_vector_analysis.csv"
    return read_csv(path) if path.is_file() else None


def load_strategy(run_dir: Path) -> pd.DataFrame | None:
    path = run_dir / "strategy_comparison.csv"
    return read_csv(path) if path.is_file() else None


def run_label(path: Path) -> str:
    config = read_config(path / "config.ini")
    scenario = config.get("scenario", "run")
    topology = config.get("topology", "")
    topologies = [x for x in config.get("topologies", "").split(",") if x.strip()]
    stamp = f"{path.parent.name}/{path.name}"
    if len(topologies) > 1:
        suffix = f"  ·  {len(topologies)} топологий"
    elif topology:
        suffix = f"  ·  {topology}"
    else:
        suffix = ""
    return f"{stamp}  ·  {scenario}{suffix}"


def _average(frame: pd.DataFrame, column: str) -> float:
    if frame is None or column not in frame.columns:
        return 0.0
    return float(frame[column].mean())


def summarize_run(run_dir: Path, topology: str | None = None) -> dict:
    """Возвращает карточки метрик для выбранного прогона."""
    summary: dict = {"source": "—", "topology": "—", "metrics": {}}
    config = read_config(run_dir / "config.ini")
    scenario = config.get("scenario", "")

    series = load_time_series(run_dir)
    # Для сценария topology карточки должны совпадать с топологическим
    # сравнением (средние по Монте-Карло), даже если рядом лежит timeseries.
    if series and scenario != "topology":
        name = topology if topology in series else next(iter(series))
        frame = series[name]
        metrics = {
            "e_qos_norm": float(frame["e_qos_norm"].mean()) if "e_qos_norm" in frame else 0.0,
            "qos_ratio": float(frame["qos_ratio"].mean()) if "qos_ratio" in frame else 0.0,
            "lcc": float(frame["lcc"].mean()) if "lcc" in frame else 0.0,
            "phi_w": float(frame["phi_w"].mean()) if "phi_w" in frame else 0.0,
            "iptd_mean_ms": float(frame["iptd_mean_ms"].mean()) if "iptd_mean_ms" in frame else 0.0,
            "iplr_mean": float(frame["iplr_mean"].max()) if "iplr_mean" in frame else 0.0,
            "e_phys_norm": float(frame["e_phys_norm"].mean()) if "e_phys_norm" in frame else 0.0,
        }
        summary.update(source="timeseries", topology=name, metrics=metrics)
        return summary

    topology = load_topology(run_dir)
    if topology is not None and not topology.empty:
        best = topology.sort_values("eff_qos_mean", ascending=False).iloc[0]
        metrics = {
            "e_qos_norm": float(best.get("eff_qos_mean", 0.0)),
            "qos_ratio": float(best.get("qos_ratio_mean", 0.0)),
            "lcc": float(best.get("lcc_mean", 0.0)),
            "phi_w": float(best.get("phi_w_mean", 0.0)),
            "iptd_mean_ms": float(best.get("iptd_mean_ms", 0.0)),
            "iplr_mean": float(best.get("iplr_mean", 0.0)),
            "e_phys_norm": float(best.get("eff_mean", 0.0)),
        }
        summary.update(source="topology", topology=str(best.get("topology", "—")),
                       metrics=metrics)
        return summary

    attack = load_attack(run_dir)
    if attack is not None and not attack.empty:
        summary.update(
            source="attack (среднее по векторам и топологиям)",
            topology="—",
            metrics={
                "e_qos_norm": _average(attack, "eff_qos_mean"),
                "qos_ratio": _average(attack, "qos_mean"),
                "lcc": _average(attack, "lcc_mean"),
                "phi_w": _average(attack, "phi_mean"),
                "iptd_mean_ms": _average(attack, "iptd_mean_ms"),
                "iplr_mean": _average(attack, "iplr_mean"),
                "e_phys_norm": _average(attack, "eff_mean"),
            },
        )
        return summary

    strategy = load_strategy(run_dir)
    if strategy is not None and not strategy.empty:
        summary.update(
            source="strategy (среднее FIFO/Triage)",
            topology="—",
            metrics={
                "e_qos_norm": _average(strategy, "eff_qos_mean"),
                "qos_ratio": _average(strategy, "qos_mean"),
                "lcc": _average(strategy, "lcc_mean"),
                "phi_w": _average(strategy, "phi_mean"),
                "iptd_mean_ms": _average(strategy, "iptd_mean_ms"),
                "iplr_mean": _average(strategy, "iplr_mean"),
                "e_phys_norm": _average(strategy, "eff_mean"),
            },
        )
        return summary

    sweep = load_sweep(run_dir)
    if sweep:
        frames = list(sweep.values())
        values: dict[str, list[float]] = {}
        for frame in frames:
            for column in frame.columns:
                if column == "x_value":
                    continue
                if column.endswith("_eff_qos_mean") or \
                        column.endswith("_qos_mean") or \
                        column.endswith("_lcc_mean") or \
                        column.endswith("_phi_mean") or \
                        column.endswith("_iptd_mean") or \
                        column.endswith("_iplr_mean") or \
                        column.endswith("_eff_mean"):
                    values.setdefault(column, []).extend(
                        pd.to_numeric(frame[column], errors="coerce")
                        .dropna().tolist())
        def sweep_mean(suffix: str) -> float:
            selected = [number for column, numbers in values.items()
                        if column.endswith(suffix) for number in numbers]
            return float(sum(selected) / len(selected)) if selected else 0.0
        summary.update(
            source="sweep (среднее по точкам и топологиям)",
            topology="—",
            metrics={
                "e_qos_norm": sweep_mean("_eff_qos_mean"),
                "qos_ratio": sweep_mean("_qos_mean"),
                "lcc": sweep_mean("_lcc_mean"),
                "phi_w": sweep_mean("_phi_mean"),
                "iptd_mean_ms": sweep_mean("_iptd_mean"),
                "iplr_mean": sweep_mean("_iplr_mean"),
                "e_phys_norm": sweep_mean("_eff_mean"),
            },
        )
        return summary

    return summary
