"""Пороговые проверки пяти showcase-режимов.

Запуск: python tests/showcase_check.py
"""

from __future__ import annotations

from pathlib import Path
import sys

import pandas as pd

ROOT = Path(__file__).resolve().parent.parent
SUMMARY = ROOT / "results" / "showcase" / "showcase_summary.csv"


def main() -> int:
    if not SUMMARY.is_file():
        print(f"Нет файла {SUMMARY}; сначала запустите scripts/run_showcase.ps1")
        return 1

    frame = pd.read_csv(SUMMARY, sep=";")
    if len(frame) != 5:
        print(f"Ожидалось 5 стадий, получено {len(frame)}")
        return 1

    e = frame["e_qos"].astype(float).tolist()
    q = frame["q"].astype(float).tolist()
    lcc = frame["lcc"].astype(float).tolist()
    phi = frame["phi"].astype(float).tolist()
    iplr = frame["iplr"].astype(float).tolist()
    exhaustion = frame["exhaustion"].astype(float).tolist()

    failures: list[str] = []

    def require(condition: bool, message: str) -> None:
        if not condition:
            failures.append(message)

    # Монотонность с допуском на Монте-Карло.
    for name, values in (("E_QoS", e), ("Q", q), ("LCC", lcc), ("phi", phi)):
        for index in range(1, len(values)):
            require(values[index] + 0.02 >= values[index - 1],
                    f"{name}: стадия {index + 1} ниже стадии {index}")

    # Явные критические пороги.
    require(e[0] <= 0.01, "стадия 1: E_QoS должна быть ≤ 0,01")
    require(e[1] <= 0.15, "стадия 2: E_QoS должна быть ≤ 0,15")
    require(0.30 <= e[2] <= 0.80, "стадия 3: E_QoS должна быть 0,30…0,80")
    require(e[3] >= 0.85, "стадия 4: E_QoS должна быть ≥ 0,85")
    require(e[4] >= 0.97, "стадия 5: E_QoS должна быть ≥ 0,97")

    require(q[0] <= 0.01, "стадия 1: Q должна быть ≤ 0,01")
    require(q[4] >= 0.95, "стадия 5: Q должна быть ≥ 0,95")
    require(lcc[0] <= 0.30, "стадия 1: LCC должна быть ≤ 0,30")
    require(lcc[4] >= 0.95, "стадия 5: LCC должна быть ≥ 0,95")
    require(phi[0] <= 0.20, "стадия 1: φ_w должна быть ≤ 0,20")
    require(phi[4] >= 0.95, "стадия 5: φ_w должна быть ≥ 0,95")

    require(iplr[0] >= 1.0e-3, "стадия 1: IPLR должна превышать 10⁻³")
    require(iplr[1] >= 1.0e-3, "стадия 2: IPLR должна превышать 10⁻³")
    require(iplr[4] <= 1.0e-6, "стадия 5: IPLR должна быть практически нулевой")
    require(exhaustion[0] >= 0.90, "стадия 1: ожидается истощение ресурсов ≥ 90 %")
    require(exhaustion[4] == 0.0, "стадия 5: истощения ресурсов быть не должно")

    require(e[4] - e[3] <= 0.10,
            "стадия 5: атаки должны почти не менять E_QoS относительно стадии 4")
    require(q[4] >= 0.95,
            "стадия 5: доля пар с QoS должна оставаться ≥ 0,95")

    print(frame[["label", "e_qos", "q", "lcc", "phi", "iplr", "exhaustion"]]
          .to_string(index=False))
    if failures:
        print("\nSHOWCASE_FAIL")
        for failure in failures:
            print(" -", failure)
        return 1
    print("\nSHOWCASE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
