"""Табличное представление результатов с копированием и экспортом."""

from __future__ import annotations

import csv
import math
import tkinter as tk
from tkinter import filedialog, ttk

import customtkinter as ctk


class DataTable(ctk.CTkFrame):
    """Тёмная таблица для числовых отчётов."""

    def __init__(self, master) -> None:
        super().__init__(master, fg_color="transparent")
        self._columns: list[str] = []
        self._rows: list[list] = []

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure(
            "NetSim.Treeview",
            background="#0d1117",
            fieldbackground="#0d1117",
            foreground="#e6edf3",
            rowheight=24,
            borderwidth=0,
            font=("Segoe UI", 9),
        )
        style.configure(
            "NetSim.Treeview.Heading",
            background="#161b22",
            foreground="#e6edf3",
            relief="flat",
            font=("Segoe UI", 9, "bold"),
        )
        style.map(
            "NetSim.Treeview",
            background=[("selected", "#1f6feb")],
            foreground=[("selected", "#ffffff")],
        )

        toolbar = ctk.CTkFrame(self, fg_color="transparent")
        toolbar.pack(fill="x", pady=(0, 4))
        self.info = ctk.CTkLabel(
            toolbar, text="", font=("Segoe UI", 10), text_color="#8b949e",
            anchor="w")
        self.info.pack(side="left")
        ctk.CTkButton(
            toolbar, text="Копировать", width=110, height=28,
            fg_color="transparent", border_width=1, border_color="#30363d",
            text_color="#e6edf3", font=("Segoe UI", 10),
            command=self.copy_to_clipboard).pack(side="right", padx=(6, 0))
        ctk.CTkButton(
            toolbar, text="Экспорт CSV", width=120, height=28,
            fg_color="transparent", border_width=1, border_color="#30363d",
            text_color="#e6edf3", font=("Segoe UI", 10),
            command=self.export_csv).pack(side="right")

        body = ttk.Frame(self)
        body.pack(fill="both", expand=True)
        body.grid_rowconfigure(0, weight=1)
        body.grid_columnconfigure(0, weight=1)

        self.tree = ttk.Treeview(body, show="headings",
                                 style="NetSim.Treeview")
        vertical = ttk.Scrollbar(body, orient="vertical",
                                 command=self.tree.yview)
        horizontal = ttk.Scrollbar(body, orient="horizontal",
                                   command=self.tree.xview)
        self.tree.configure(yscrollcommand=vertical.set,
                            xscrollcommand=horizontal.set)
        self.tree.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")

    @staticmethod
    def _format(value) -> str:
        if value is None:
            return ""
        if isinstance(value, bool):
            return "да" if value else "нет"
        if isinstance(value, float):
            if math.isnan(value):
                return "—"
            if value != 0.0 and abs(value) < 1e-3:
                return f"{value:.3e}"
            if abs(value) >= 1000.0:
                return f"{value:.1f}"
            return f"{value:.4f}"
        return str(value)

    def set_dataframe(self, frame) -> None:
        if frame is None or frame.empty:
            self.set_data([], [])
            return
        columns = [str(c) for c in frame.columns]
        rows = frame.itertuples(index=False, name=None)
        self.set_data(columns, rows)

    def set_data(self, columns, rows) -> None:
        self._columns = [str(c) for c in columns]
        self._rows = [list(r) for r in rows]
        self.tree.delete(*self.tree.get_children())
        self.tree["columns"] = self._columns
        for column in self._columns:
            self.tree.heading(column, text=column)
            width = max(85, min(230, 9 * len(column) + 24))
            self.tree.column(column, width=width, anchor="w", stretch=True)
        for row in self._rows:
            self.tree.insert(
                "", "end", values=[self._format(value) for value in row])
        self.info.configure(text=f"{len(self._rows)} строк × "
                                f"{len(self._columns)} столбцов")

    def copy_to_clipboard(self) -> None:
        if not self._columns:
            return
        lines = ["\t".join(self._columns)]
        for row in self._rows:
            lines.append("\t".join(self._format(value) for value in row))
        self.clipboard_clear()
        self.clipboard_append("\n".join(lines))
        self.info.configure(text="Скопировано в буфер обмена")

    def export_csv(self) -> None:
        if not self._columns:
            return
        path = filedialog.asksaveasfilename(
            title="Экспорт таблицы",
            defaultextension=".csv",
            filetypes=[("CSV", "*.csv")],
            initialfile="netsim_table.csv")
        if not path:
            return
        with open(path, "w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.writer(stream, delimiter=";")
            writer.writerow(self._columns)
            for row in self._rows:
                writer.writerow([self._format(value) for value in row])
        self.info.configure(text=f"Сохранено: {path}")
