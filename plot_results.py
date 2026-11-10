"""Построение всех графиков по CSV-отчётам одного прогона.

Использование:
    python plot_results.py runs/20260101_120000
    python plot_results.py results/vkr

Скрипт не требует GUI: используется тот же модуль оформления, что и в
приложении CustomTkinter. Готовые PNG сохраняются в <run>/plots/.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from matplotlib.figure import Figure

from gui import data, plots

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def save(figure: Figure, path: Path) -> None:
    plots.save_figure(figure, path)
    print("  сохранено:", path)


def main() -> int:
    parser = argparse.ArgumentParser(description="NetSim: графики по CSV")
    parser.add_argument("run_dir", nargs="?", default="results",
                        help="каталог прогона с CSV")
    args = parser.parse_args()

    run_dir = Path(args.run_dir)
    if not run_dir.is_dir():
        print("Каталог не найден:", run_dir)
        return 1

    out_dir = run_dir / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)

    series = data.load_time_series(run_dir)
    for name, frame in series.items():
        figure = Figure(figsize=(10, 6), dpi=140)
        plots.plot_timeseries(figure, frame, name)
        save(figure, out_dir / f"timeseries_{name}.png")

    topology = data.load_topology(run_dir)
    if topology is not None:
        figure = Figure(figsize=(10, 6), dpi=140)
        plots.plot_topology(figure, topology)
        save(figure, out_dir / "topology.png")

    attack = data.load_attack(run_dir)
    if attack is not None:
        figure = Figure(figsize=(10, 6), dpi=140)
        plots.plot_attack(figure, attack)
        save(figure, out_dir / "attack.png")

    strategy = data.load_strategy(run_dir)
    if strategy is not None:
        for metric, label, slug in (
            ("eff_qos_mean", "E_QoS", "E_QoS"),
            ("phi_mean", "φ_w", "phi_w"),
        ):
            figure = Figure(figsize=(11, 6), dpi=140)
            plots.plot_strategy(figure, strategy, metric=metric, ylabel=label)
            save(figure, out_dir / f"strategy_{slug}.png")

    sweep = data.load_sweep(run_dir)
    if sweep:
        figure = Figure(figsize=(10, 7), dpi=140)
        plots.plot_sweep(figure, sweep)
        save(figure, out_dir / "sweep.png")

    print("Готово:", out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
