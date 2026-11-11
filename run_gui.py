"""Запуск графического интерфейса NetSim.

Примеры:
    python run_gui.py
    python run_gui.py --selftest --screenshot runs/gui.png
"""

from __future__ import annotations

import argparse
import traceback
from pathlib import Path

from gui.app import App


def main() -> int:
    parser = argparse.ArgumentParser(description="NetSim GUI")
    parser.add_argument("--selftest", action="store_true",
                        help="автоматический прогон и проверка интерфейса")
    parser.add_argument("--screenshot", type=Path, default=None,
                        help="сохранить снимок окна после self-test")
    parser.add_argument("--open-run", type=Path, default=None,
                        help="открыть готовый каталог прогона")
    parser.add_argument("--tab", default=None,
                        help="вкладка результатов: Динамика, Топологии, Sweep, Атаки, FIFO/Triage")
    args = parser.parse_args()

    app = App()

    if args.selftest:
        def run() -> None:
            try:
                app.run_selftest(args.screenshot)
            except Exception:
                traceback.print_exc()
                print("GUI_SELFTEST_FAIL")
                app.destroy()

        app.after(700, run)
    elif args.open_run:
        def open_existing() -> None:
            app.open_run(args.open_run, args.tab)
            if args.screenshot:
                app.after(900, lambda: (app.capture_window(args.screenshot),
                                        app.after(400, app.destroy)))

        app.after(900, open_existing)

    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
