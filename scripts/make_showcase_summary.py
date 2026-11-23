"""Сборка сводки и графика по пяти showcase-прогонам."""

from __future__ import annotations

import csv
from pathlib import Path
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "results" / "showcase"
DOCS = ROOT / "docs" / "img"

STAGES = {
    "01_catastrophe": ("1. Катастрофа", "почти полный отказ"),
    "02_severe": ("2. Тяжёлый", "сильная деградация"),
    "03_baseline": ("3. Базовый", "рабочий режим"),
    "04_good": ("4. Хороший", "устойчивый режим"),
    "05_excellent": ("5. Отличный", "атаки почти не влияют"),
}


def read_ini(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key.strip()] = value.strip()
    return result


def main() -> int:
    rows: list[dict] = []
    for directory in sorted(BASE.iterdir()):
        if not directory.is_dir() or directory.name not in STAGES:
            continue
        topology_csv = directory / "topology_comparison.csv"
        config_ini = directory / "config.ini"
        if not topology_csv.is_file():
            continue
        frame = pd.read_csv(topology_csv)
        if frame.empty:
            continue
        row = frame.iloc[0]
        config = read_ini(config_ini)
        label, meaning = STAGES[directory.name]
        rows.append({
            "stage": directory.name,
            "label": label,
            "meaning": meaning,
            "topology": str(row.get("topology", "—")),
            "num_nodes": config.get("num_nodes", ""),
            "n_sims": config.get("n_sims", ""),
            "T": config.get("T", ""),
            "lambda": config.get("attack_lambda", ""),
            "R": config.get("global_resources", ""),
            "slots": config.get("repair_slots", ""),
            "demand": config.get("demand_per_pair", ""),
            "access": config.get("access_capacity", ""),
            "core": config.get("core_capacity", ""),
            "buffer": config.get("buffer_packets", ""),
            "e_qos": float(row.get("eff_qos_mean", 0.0)),
            "e_qos_ci": float(row.get("eff_qos_ci", 0.0)),
            "q": float(row.get("qos_ratio_mean", 0.0)),
            "lcc": float(row.get("lcc_mean", 0.0)),
            "lcc_ci": float(row.get("lcc_ci", 0.0)),
            "phi": float(row.get("phi_w_mean", 0.0)),
            "iptd_ms": float(row.get("iptd_mean_ms", 0.0)),
            "iplr": float(row.get("iplr_mean", 0.0)),
            "exhaustion": float(row.get("exhaustion_rate", 0.0)),
        })

    if len(rows) != 5:
        print(f"Ожидалось 5 стадий, найдено {len(rows)}")
        return 1

    BASE.mkdir(parents=True, exist_ok=True)
    summary_csv = BASE / "showcase_summary.csv"
    with summary_csv.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0].keys()), delimiter=";")
        writer.writeheader()
        writer.writerows(rows)

    stages = list(range(1, 6))
    short_labels = [
        f"{index}\n{row['label'].split('. ', 1)[-1]}"
        for index, row in enumerate(rows, 1)
    ]
    e_qos = [row["e_qos"] for row in rows]
    q = [row["q"] for row in rows]
    lcc = [row["lcc"] for row in rows]
    phi = [row["phi"] for row in rows]
    iplr = [row["iplr"] * 1000.0 for row in rows]
    exhaustion = [row["exhaustion"] * 100.0 for row in rows]

    plt.rcParams.update({
        "font.family": "DejaVu Sans",
        "axes.edgecolor": "#30363d",
        "axes.labelcolor": "#e6edf3",
        "text.color": "#e6edf3",
        "xtick.color": "#8b949e",
        "ytick.color": "#8b949e",
    })
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.0), facecolor="#0f1419")
    for axis in axes:
        axis.set_facecolor("#161b22")
        axis.grid(color="#30363d", linestyle="--", alpha=0.55)
        axis.set_axisbelow(True)

    axes[0].plot(stages, e_qos, "o-", color="#4cc9f0", lw=2.2, label="E_QoS")
    axes[0].plot(stages, q, "s--", color="#f8961e", lw=1.8, label="Q")
    axes[0].plot(stages, lcc, "D-.", color="#f9c74f", lw=1.6, label="LCC")
    axes[0].plot(stages, phi, "^-", color="#80ed99", lw=1.8, label="φ_w")
    axes[0].set_ylim(0, 1.05)
    axes[0].set_xticks(stages, short_labels, fontsize=8)
    axes[0].set_ylabel("Метрика")
    axes[0].set_title("Пять режимов: рост качества", pad=10)
    axes[0].legend(frameon=False, labelcolor="#e6edf3", fontsize=9, ncol=2)

    axes[1].bar(stages, iplr, color="#c77dff", width=0.55)
    axes[1].set_xticks(stages, short_labels, fontsize=8)
    axes[1].set_ylabel("IPLR × 1000")
    axes[1].set_title("Потери пакетов", pad=10)
    for x, value in zip(stages, iplr):
        axes[1].text(x, value + 0.08, f"{value:.3f}", ha="center",
                     color="#e6edf3", fontsize=9)

    axes[2].bar(stages, exhaustion, color="#f94144", width=0.55)
    axes[2].set_ylim(0, 110)
    axes[2].set_xticks(stages, short_labels, fontsize=8)
    axes[2].set_ylabel("Истощение ресурсов, %")
    axes[2].set_title("Ресурсный дефицит", pad=10)
    for x, value in zip(stages, exhaustion):
        axes[2].text(x, value + 2, f"{value:.0f}%", ha="center",
                     color="#e6edf3", fontsize=9)

    fig.suptitle("NetSim: пять сценариев — от катастрофы к устойчивой сети",
                 color="#e6edf3", fontsize=15, y=0.98)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    BASE.mkdir(parents=True, exist_ok=True)
    DOCS.mkdir(parents=True, exist_ok=True)
    fig.savefig(BASE / "showcase_summary.png", dpi=180,
                facecolor=fig.get_facecolor())
    fig.savefig(DOCS / "showcase_summary.png", dpi=180,
                facecolor=fig.get_facecolor())

    with (BASE / "showcase_summary.md").open("w", encoding="utf-8") as stream:
        stream.write("# Пять showcase-режимов\n\n")
        stream.write("| Стадия | Смысл | E_QoS | Q | LCC | φ_w | IPTD, мс | IPLR | Истощение |\n")
        stream.write("|---|---|---:|---:|---:|---:|---:|---:|---:|\n")
        for row in rows:
            stream.write(
                f"| {row['label']} | {row['meaning']} | {row['e_qos']:.3f} | "
                f"{row['q']:.3f} | {row['lcc']:.3f} | {row['phi']:.3f} | "
                f"{row['iptd_ms']:.2f} | {row['iplr']:.3g} | "
                f"{row['exhaustion']*100:.0f}% |\n")

    print(f"Сохранено: {summary_csv}")
    print(f"Сохранено: {BASE / 'showcase_summary.png'}")
    print(f"Сохранено: {DOCS / 'showcase_summary.png'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
