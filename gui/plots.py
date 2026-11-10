"""Построение графиков деградации QoS в стиле тёмного интерфейса."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd
from matplotlib.figure import Figure

BG = "#0f1419"
PANEL = "#161b22"
FG = "#e6edf3"
MUTED = "#8b949e"
GRID = "#30363d"

C_EQOS = "#4cc9f0"
C_EPHYS = "#80ed99"
C_LCC = "#f9c74f"
C_Q = "#f8961e"
C_IPTD = "#f94144"
C_IPLR = "#c77dff"

TOPO_COLORS = {
    "star": "#f94144",
    "ring": "#f9c74f",
    "full_mesh": "#4cc9f0",
    "erdos_renyi": "#80ed99",
    "barabasi_albert": "#c77dff",
}
STRATEGY_COLORS = {
    "targeted": "#f94144",
    "random": "#4cc9f0",
    "cascading": "#80ed99",
}
PARAM_LABELS = {
    "lambda": "λ, атак/ед. вр.",
    "resources": "R, ед. ресурса",
    "mu": "μ, 1/ед. вр.",
    "P_attack": "P_attack",
    "demand": "трафик на пару",
}


def style_axes(ax) -> None:
    ax.set_facecolor(PANEL)
    ax.tick_params(colors=MUTED, labelsize=9)
    ax.xaxis.label.set_color(FG)
    ax.yaxis.label.set_color(FG)
    ax.title.set_color(FG)
    for spine in ax.spines.values():
        spine.set_color(GRID)
    ax.grid(color=GRID, alpha=0.55, linestyle="--", linewidth=0.7)
    ax.set_axisbelow(True)


def style_figure(fig: Figure) -> None:
    fig.patch.set_facecolor(BG)


def empty(ax, text: str = "Нет данных для отображения") -> None:
    ax.clear()
    ax.set_facecolor(PANEL)
    ax.axis("off")
    ax.text(0.5, 0.5, text, ha="center", va="center", color=MUTED, fontsize=12)


def empty_figure(fig: Figure, text: str = "Нет данных для отображения") -> None:
    fig.clear()
    style_figure(fig)
    empty(fig.add_subplot(111), text)


def _get(frame: pd.DataFrame, name: str, default: float = 0.0) -> pd.Series:
    if name in frame.columns:
        return pd.to_numeric(frame[name], errors="coerce").fillna(default)
    return pd.Series([default] * len(frame))


def plot_timeseries(fig: Figure, frame: pd.DataFrame, topology: str) -> None:
    fig.clear()
    style_figure(fig)
    if frame.empty:
        empty(fig.add_subplot(111))
        return

    grid = fig.add_gridspec(2, 1, height_ratios=[2.0, 1.1], hspace=0.42,
                            left=0.08, right=0.93, top=0.90, bottom=0.10)
    ax = fig.add_subplot(grid[0])
    style_axes(ax)
    time = _get(frame, "time")

    ax.plot(time, _get(frame, "e_qos_norm"), color=C_EQOS, lw=2.2,
            label="E_QoS / E_QoS(0)")
    ax.plot(time, _get(frame, "e_phys_norm"), color=C_EPHYS, lw=1.4,
            ls="--", label="E_phys / E_phys(0)")
    ax.plot(time, _get(frame, "lcc"), color=C_LCC, lw=1.6,
            label="LCC")
    ax.plot(time, _get(frame, "qos_ratio"), color=C_Q, lw=1.6,
            ls="-.", label="Доля пар с QoS")
    ax.set_ylim(-0.02, 1.08)
    ax.set_ylabel("Нормированная метрика")
    ax.set_title(f"Деградация сети: {topology}", pad=10)
    ax.legend(loc="lower left", frameon=True, facecolor=PANEL,
              edgecolor=GRID, framealpha=0.85, labelcolor=FG, fontsize=9,
              ncol=2)
    ax.set_xlim(float(time.min()), max(float(time.max()), 1e-9))

    ax2 = fig.add_subplot(grid[1])
    style_axes(ax2)
    ax2.plot(time, _get(frame, "iptd_mean_ms"), color=C_IPTD, lw=1.8,
             label="IPTD (мс)")
    ax2.set_ylabel("IPTD, мс", color=C_IPTD)
    ax2.tick_params(axis="y", colors=C_IPTD)
    ax2.set_xlabel("Время t")

    ax3 = ax2.twinx()
    ax3.set_facecolor(PANEL)
    ax3.plot(time, _get(frame, "iplr_mean") * 1000.0, color=C_IPLR, lw=1.5,
             ls="--", label="IPLR ×1000")
    ax3.set_ylabel("IPLR ×1000", color=C_IPLR)
    ax3.tick_params(axis="y", colors=C_IPLR)
    for spine in ax3.spines.values():
        spine.set_color(GRID)

    lines = ax2.get_lines() + ax3.get_lines()
    ax2.legend(lines, [line.get_label() for line in lines], loc="upper right",
               frameon=True, facecolor=PANEL, edgecolor=GRID,
               framealpha=0.85, labelcolor=FG, fontsize=8)


def plot_topology(fig: Figure, frame: pd.DataFrame) -> None:
    fig.clear()
    style_figure(fig)
    ax = fig.add_subplot(111)
    style_axes(ax)
    if frame.empty:
        empty(ax)
        return

    labels = frame["topology"].astype(str).tolist()
    x = np.arange(len(labels))
    width = 0.20

    series = [
        ("eff_qos_mean", "eff_qos_ci", "E_QoS", C_EQOS),
        ("qos_ratio_mean", "qos_ratio_ci", "Доля пар с QoS", C_Q),
        ("lcc_mean", "lcc_ci", "LCC", C_LCC),
        ("phi_mean", "phi_ci", "φ_w", C_EPHYS),
    ]
    for i, (mean_col, ci_col, label, color) in enumerate(series):
        offsets = x + (i - (len(series) - 1) / 2) * width
        ax.bar(offsets, _get(frame, mean_col), width,
               yerr=_get(frame, ci_col), capsize=3,
               label=label, color=color, edgecolor=BG, linewidth=0.6,
               error_kw={"ecolor": MUTED, "lw": 0.9})

    ax.set_xticks(x)
    ax.set_xticklabels([name.replace("_", "\n") for name in labels], fontsize=9)
    ax.set_ylim(0, 1.08)
    ax.set_ylabel("Среднее значение")
    ax.set_title("Сравнение топологий (Монте-Карло, 95% ДИ)", pad=10)
    ax.legend(loc="upper right", frameon=False, labelcolor=FG, ncol=4)


def plot_attack(fig: Figure, frame: pd.DataFrame) -> None:
    fig.clear()
    style_figure(fig)
    ax = fig.add_subplot(111)
    style_axes(ax)
    if frame.empty:
        empty(ax)
        return

    pivot = frame.pivot_table(index="topology", columns="attack_strategy",
                              values="eff_qos_mean", aggfunc="mean")
    topologies = list(pivot.index)
    strategies = list(pivot.columns)
    x = np.arange(len(topologies))
    width = 0.8 / max(1, len(strategies))

    for i, strategy in enumerate(strategies):
        values = pivot[strategy].fillna(0).values
        ax.bar(x + (i - (len(strategies) - 1) / 2) * width, values, width,
               label=strategy, color=STRATEGY_COLORS.get(strategy, MUTED),
               edgecolor=BG, linewidth=0.6)

    ax.set_xticks(x)
    ax.set_xticklabels([name.replace("_", "\n") for name in topologies],
                       fontsize=9)
    ax.set_ylim(0, 1.08)
    ax.set_ylabel("E_QoS (нормированная)")
    ax.set_title("Векторы атак: устойчивость по E_QoS", pad=10)
    ax.legend(frameon=False, labelcolor=FG, ncol=3, loc="upper right")


def plot_sweep(fig: Figure, sweep: dict[str, pd.DataFrame]) -> None:
    fig.clear()
    style_figure(fig)
    if not sweep:
        empty(fig.add_subplot(111))
        return

    params = list(sweep.keys())[:4]
    rows = 2 if len(params) > 1 else 1
    cols = 2 if len(params) > 1 else 1
    for index, param in enumerate(params):
        ax = fig.add_subplot(rows, cols, index + 1)
        style_axes(ax)
        frame = sweep[param]
        x = _get(frame, "x_value")
        plotted = 0
        for column in frame.columns:
            if not column.endswith("_eff_qos_mean"):
                continue
            topology = column[: -len("_eff_qos_mean")]
            color = TOPO_COLORS.get(topology, MUTED)
            y = _get(frame, column)
            ax.plot(x, y, marker="o", ms=4, lw=1.8, color=color,
                    label=topology)
            ci_col = column.replace("_mean", "_ci")
            if ci_col in frame.columns:
                ci = _get(frame, ci_col)
                ax.fill_between(x, y - ci, y + ci, color=color, alpha=0.14,
                                linewidth=0)
            plotted += 1
        if plotted == 0:
            empty(ax)
            continue
        ax.set_title(f"Sweep: {PARAM_LABELS.get(param, param)}", pad=8)
        ax.set_xlabel(PARAM_LABELS.get(param, param))
        ax.set_ylabel("E_QoS")
        ax.set_ylim(0, 1.08)
        ax.legend(frameon=False, labelcolor=FG, fontsize=8)

    fig.subplots_adjust(left=0.07, right=0.97, top=0.90, bottom=0.09,
                        hspace=0.42, wspace=0.22)


def plot_degree_ccdf(fig: Figure, degree_frame: pd.DataFrame,
                     structure: pd.Series | None, topology: str) -> None:
    fig.clear()
    style_figure(fig)
    ax = fig.add_subplot(111)
    style_axes(ax)
    if degree_frame is None or degree_frame.empty:
        empty(ax)
        return

    frame = degree_frame.sort_values("degree")
    degree = pd.to_numeric(frame["degree"], errors="coerce").fillna(0)
    count = pd.to_numeric(frame["count"], errors="coerce").fillna(0)
    total = float(count.sum())
    if total <= 0:
        empty(ax)
        return

    ccdf = count[::-1].cumsum()[::-1] / total
    mask = (degree >= 1) & (ccdf > 0)
    ax.loglog(degree[mask], ccdf[mask], marker="o", ms=5, lw=1.8,
              color=C_EQOS, label="CCDF степеней")

    gamma = None
    if structure is not None and "gamma_est" in structure:
        try:
            gamma = float(structure["gamma_est"])
        except Exception:
            gamma = None
    title = f"{topology}: распределение степеней"
    if gamma is not None and np.isfinite(gamma):
        title += f"  (оценка γ ≈ {gamma:.2f})"
    ax.set_title(title, pad=10)
    ax.set_xlabel("Степень k")
    ax.set_ylabel("P(K ≥ k)")
    ax.grid(True, which="both", color=GRID, alpha=0.45, linestyle="--",
            linewidth=0.7)
    ax.legend(frameon=False, labelcolor=FG, fontsize=9)


def plot_strategy(fig: Figure, frame: pd.DataFrame,
                  metric: str = "eff_qos_mean", ylabel: str = "E_QoS") -> None:
    fig.clear()
    style_figure(fig)
    if frame.empty:
        empty(fig.add_subplot(111))
        return

    topologies = list(frame["topology"].unique())
    axes = fig.subplots(1, len(topologies), sharey=True, squeeze=False)[0]
    ci_column = metric.replace("_mean", "_ci")
    sig_column = "sig_phi_direction" if metric == "phi_mean" else "sig_direction"

    for ax, topology in zip(axes, topologies):
        style_axes(ax)
        subset = frame[frame["topology"] == topology]
        for gatekeeper, color, marker, style in (
            ("fifo", MUTED, "s", "--"),
            ("triage", C_EQOS, "o", "-"),
        ):
            data = subset[subset["gatekeeper"] == gatekeeper].sort_values("R")
            if data.empty:
                continue
            ax.errorbar(data["R"], _get(data, metric),
                        yerr=_get(data, ci_column), fmt=marker, ls=style,
                        color=color, capsize=3, ms=6, lw=1.7,
                        label=gatekeeper.upper())
        try:
            significant = subset[
                (subset["gatekeeper"] == "triage") &
                (subset[sig_column].astype(str).str.startswith("triage"))]
            for _, row in significant.iterrows():
                ax.annotate("*", (row["R"], row[metric] + 0.04),
                            color=C_IPTD, ha="center", fontsize=15)
        except Exception:
            pass
        ax.set_title(topology, pad=8)
        ax.set_xlabel("Ресурсы R")
        ax.set_ylim(0, 1.08)
    axes[0].set_ylabel(ylabel)
    axes[-1].legend(frameon=False, labelcolor=FG, fontsize=8)
    fig.subplots_adjust(left=0.07, right=0.98, top=0.88, bottom=0.12,
                        wspace=0.10)


def save_figure(fig: Figure, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=180, facecolor=fig.get_facecolor())
