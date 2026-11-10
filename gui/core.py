"""Мост между GUI и вычислительным ядром NetSim (C++)."""

from __future__ import annotations

import os
import queue
import re
import subprocess
import sys
import threading
from pathlib import Path
from typing import Callable, Iterable

ROOT = Path(__file__).resolve().parent.parent

PROGRESS_RE = re.compile(r"^PROGRESS\s+([0-9]*\.?[0-9]+)\s+(.*)$")


def find_core() -> Path | None:
    """Ищет исполняемый файл ядра в типовых местах сборки."""
    env = os.environ.get("NETSIM_EXE")
    candidates: list[Path] = []
    if env:
        candidates.append(Path(env))
    candidates += [
        ROOT / "x64" / "Release" / "dipl.exe",
        ROOT / "x64" / "Debug" / "dipl.exe",
        ROOT / "build" / "Release" / "netsim.exe",
        ROOT / "build" / "netsim.exe",
        ROOT / "bin" / "netsim.exe",
    ]
    for path in candidates:
        if path and path.is_file():
            return path
    return None


def default_config() -> dict:
    """Значения по умолчанию, совпадающие с SimConfig ядра."""
    return {
        "scenario": "topology",
        "topology": "barabasi_albert",
        "topologies": "star,ring,full_mesh,erdos_renyi,barabasi_albert",
        "num_nodes": 50,
        "n_sims": 20,
        "T": 150.0,
        "sample_points": 200,
        "seed": 42,
        "attack_lambda": 1.0,
        "attack_strategy": "targeted",
        "attack_strategies": "targeted,random,cascading",
        "base_P_attack": 0.60,
        "gatekeeper": "fifo",
        "repair_slots": 3,
        "global_resources": 200.0,
        "base_P_recovery": 0.80,
        "base_mu": 0.20,
        "base_C_repair": 1,
        "base_weight": 1.0,
        "heterogeneous": 1,
        "er_p": 0.10,
        "ba_m": 2,
        "access_capacity": 1.0e9,
        "core_capacity": 10.0e9,
        "base_latency": 0.002,
        "latency_jitter": 0.001,
        "qos_class": 1,
        "packet_bits": 12000.0,
        "buffer_packets": 100,
        "demand_per_pair": 2.0e6,
        "traffic_iterations": 3,
        "hop_penalty": 0.005,
        "preserve_total_demand": 1,
        "out_dir": "runs/latest",
        "resource_levels": "50,100,200,500",
        "sweep_params": "lambda,resources,mu,P_attack",
        "sweep_lambda": "0.5,5.0,8",
        "sweep_resources": "50,1000,8",
        "sweep_mu": "0.05,0.80,8",
        "sweep_P_attack": "0.30,0.95,8",
    }


def write_config(config: dict, path: Path) -> None:
    """Пишет INI-файл, понятный C++-парсеру."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Конфигурация NetSim, сформированная GUI", ""]
    for key, value in config.items():
        if isinstance(value, bool):
            value = 1 if value else 0
        if isinstance(value, (list, tuple)):
            value = ",".join(str(x) for x in value)
        lines.append(f"{key}={value}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_config(path: Path) -> dict:
    """Читает INI в словарь строк."""
    result: dict[str, str] = {}
    if not path.is_file():
        return result
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        result[key.strip()] = value.strip()
    return result


class CoreRunner:
    """Запускает ядро в отдельном процессе и отдаёт строки в очередь."""

    def __init__(self, events: "queue.Queue[tuple]") -> None:
        self._events = events
        self._process: subprocess.Popen | None = None
        self._thread: threading.Thread | None = None

    @property
    def running(self) -> bool:
        return self._process is not None and self._process.poll() is None

    def start(self, args: Iterable[str], cwd: Path) -> None:
        if self.running:
            raise RuntimeError("Процесс уже запущен")
        env = os.environ.copy()
        env["PYTHONUTF8"] = "1"
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self._process = subprocess.Popen(
            [str(a) for a in args],
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            env=env,
            creationflags=creationflags,
        )
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()

    def _reader(self) -> None:
        assert self._process is not None
        stdout = self._process.stdout
        if stdout is not None:
            for line in stdout:
                self._events.put(("line", line.rstrip("\r\n")))
        code = self._process.wait()
        self._events.put(("done", code))

    def cancel(self) -> None:
        if self._process is not None and self._process.poll() is None:
            self._process.terminate()


def parse_progress(line: str) -> tuple[float, str] | None:
    match = PROGRESS_RE.match(line.strip())
    if not match:
        return None
    fraction = max(0.0, min(1.0, float(match.group(1))))
    return fraction, match.group(2).strip()


def format_bytes_per_second(value: float) -> str:
    if value >= 1e9:
        return f"{value / 1e9:.2f} Гбит/с"
    if value >= 1e6:
        return f"{value / 1e6:.1f} Мбит/с"
    if value >= 1e3:
        return f"{value / 1e3:.0f} кбит/с"
    return f"{value:.0f} бит/с"


def open_in_explorer(path: Path) -> None:
    if not path.exists():
        return
    if sys.platform.startswith("win"):
        os.startfile(str(path))  # type: ignore[attr-defined]
    elif sys.platform == "darwin":
        subprocess.Popen(["open", str(path)])
    else:
        subprocess.Popen(["xdg-open", str(path)])


def run_core_sync(args: Iterable[str], cwd: Path,
                  on_line: Callable[[str], None] | None = None) -> int:
    """Синхронный запуск (для self-test)."""
    env = os.environ.copy()
    env["PYTHONUTF8"] = "1"
    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    process = subprocess.Popen(
        [str(a) for a in args],
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        stdin=subprocess.DEVNULL,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
        env=env,
        creationflags=creationflags,
    )
    assert process.stdout is not None
    for line in process.stdout:
        if on_line:
            on_line(line.rstrip("\r\n"))
    return process.wait()
