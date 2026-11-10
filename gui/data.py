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


def summarize_run(run_dir: Path, topology: str | None = None) -> dict:
    """Возвращает карточки метрик для выбранного прогона."""
    summary: dict = {"source": "—", "topology": "—", "metrics": {}}

    series = load_time_series(run_dir)
    if series:
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
        row = attack.iloc[0]
        summary.update(
            source="attack",
            topology=str(row.get("topology", "—")),
            metrics={
                "e_qos_norm": float(row.get("eff_qos_mean", 0.0)),
                "qos_ratio": float(row.get("qos_mean", 0.0)),
                "lcc": float(row.get("lcc_mean", 0.0)),
                "phi_w": float(row.get("phi_mean", 0.0)),
                "iptd_mean_ms": float(row.get("iptd_mean_ms", 0.0)),
                "iplr_mean": float(row.get("iplr_mean", 0.0)),
                "e_phys_norm": float(row.get("eff_mean", 0.0)),
            },
        )
        return summary

    strategy = load_strategy(run_dir)
    if strategy is not None and not strategy.empty:
        row = strategy.sort_values("eff_qos_mean", ascending=False).iloc[0]
        summary.update(
            source="strategy",
            topology=str(row.get("topology", "—")),
            metrics={
                "e_qos_norm": float(row.get("eff_qos_mean", 0.0)),
                "qos_ratio": float(row.get("qos_mean", 0.0)),
                "lcc": float(row.get("lcc_mean", 0.0)),
                "phi_w": float(row.get("phi_mean", 0.0)),
                "iptd_mean_ms": float(row.get("iptd_mean_ms", 0.0)),
                "iplr_mean": float(row.get("iplr_mean", 0.0)),
                "e_phys_norm": float(row.get("eff_mean", 0.0)),
            },
        )
        return summary

    sweep = load_sweep(run_dir)
    if sweep:
        frame = next(iter(sweep.values()))
        row = frame.iloc[-1]
        columns = {column: row[column] for column in frame.columns}
        topology = "—"
        for column in frame.columns:
            if column.endswith("_eff_qos_mean"):
                topology = column[: -len("_eff_qos_mean")]
                break
        summary.update(
            source="sweep",
            topology=topology,
            metrics={
                "e_qos_norm": float(columns.get(f"{topology}_eff_qos_mean", 0.0)),
                "qos_ratio": float(columns.get(f"{topology}_qos_mean", 0.0)),
                "lcc": float(columns.get(f"{topology}_lcc_mean", 0.0)),
                "phi_w": float(columns.get(f"{topology}_phi_mean", 0.0)),
                "iptd_mean_ms": float(columns.get(f"{topology}_iptd_mean", 0.0)),
                "iplr_mean": float(columns.get(f"{topology}_iplr_mean", 0.0)),
                "e_phys_norm": float(columns.get(f"{topology}_eff_mean", 0.0)),
            },
        )
        return summary

    return summary
