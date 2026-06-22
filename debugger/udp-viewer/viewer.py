"""UDP Log Viewer for MugTextDissector.

Listens on UDP port 5555 for JSON log messages and displays them in a table.
Each message format:
  {"app":..., "timestamp":ms, "tid":..., "level":..., "tag":..., "type":..., "payload":...}

Filter bar: space-separated tokens, each matched against level/tag/type/payload (AND logic).
"""

import json
import socket
import sys
import threading
from datetime import datetime

from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt6.QtGui import QColor, QFont
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QFileDialog,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSplitter,
    QTableWidget,
    QTableWidgetItem,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

PORT = 5555
MAX_ROWS = 5000

USECASE_LIFECYCLE_TYPE = "UseCaseLifecycle"
SELECTION_TYPE = "Selection"

LEVEL_COLORS = {
    "ERROR": QColor(255, 80, 80),
    "WARN":  QColor(255, 200, 80),
    "INFO":  QColor(220, 220, 220),
}

ALL_INDICES = -1
NO_SELECTION = -2


class UdpReceiver(QObject):
    message_received = pyqtSignal(str)

    def __init__(self, port: int):
        super().__init__()
        self._port = port
        self._running = False
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._running = True
        self._thread.start()

    def stop(self):
        self._running = False

    def _run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(1.0)
        sock.bind(("0.0.0.0", self._port))
        while self._running:
            try:
                data, _ = sock.recvfrom(65535)
                for line in data.decode("utf-8", errors="replace").splitlines():
                    line = line.strip()
                    if line:
                        self.message_received.emit(line)
            except socket.timeout:
                continue
        sock.close()


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UDP Log Viewer — OfxBase")
        self.resize(1200, 700)

        self._all_rows: list[dict] = []
        self._paused = False
        self._active_usecases: set[str] = set()
        self._current_selection: list[dict] = []
        self._current_intents: dict[str, str] = {}
        self._building_intents: dict[str, str] = {}
        self._expected_intents_count = 0

        self._build_ui()
        self._receiver = UdpReceiver(PORT)
        self._receiver.message_received.connect(self._on_message)
        self._receiver.start()

        # Batch-flush pending rows every 100 ms to avoid per-packet redraws
        self._pending: list[dict] = []
        self._flush_timer = QTimer()
        self._flush_timer.setInterval(100)
        self._flush_timer.timeout.connect(self._flush_pending)
        self._flush_timer.start()

        self._update_status()

    def _build_ui(self):
        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        # ── Toolbar ──────────────────────────────────────────────
        toolbar = QHBoxLayout()

        toolbar.addWidget(QLabel("Filter:"))
        self._filter_edit = QLineEdit()
        self._filter_edit.setPlaceholderText("space-separated tokens (AND) — matches level / tag / type / payload")
        self._filter_edit.textChanged.connect(self._apply_filter)
        toolbar.addWidget(self._filter_edit, stretch=1)

        self._pause_btn = QPushButton("Pause")
        self._pause_btn.setCheckable(True)
        self._pause_btn.toggled.connect(self._on_pause)
        toolbar.addWidget(self._pause_btn)

        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._clear)
        toolbar.addWidget(clear_btn)

        copy_btn = QPushButton("Copy Selected")
        copy_btn.clicked.connect(self._copy_selected)
        toolbar.addWidget(copy_btn)

        export_btn = QPushButton("Export Selected")
        export_btn.clicked.connect(self._export_selected)
        toolbar.addWidget(export_btn)

        self._autoscroll_cb = QCheckBox("Auto-scroll")
        self._autoscroll_cb.setChecked(True)
        toolbar.addWidget(self._autoscroll_cb)

        layout.addLayout(toolbar)

        # ── Splitter: table (top) + detail (bottom) ───────────────
        splitter = QSplitter(Qt.Orientation.Vertical)

        self._table = QTableWidget()
        self._table.setColumnCount(6)
        self._table.setHorizontalHeaderLabels(["Time", "Level", "Tag", "Type", "Payload", "TID"])
        self._table.horizontalHeader().setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)
        self._table.horizontalHeader().setStretchLastSection(False)
        self._table.setColumnWidth(0, 90)
        self._table.setColumnWidth(1, 55)
        self._table.setColumnWidth(2, 160)
        self._table.setColumnWidth(3, 160)
        self._table.setColumnWidth(5, 80)
        self._table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self._table.setSelectionMode(QTableWidget.SelectionMode.ExtendedSelection)
        self._table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self._table.setAlternatingRowColors(True)
        self._table.verticalHeader().setDefaultSectionSize(20)
        self._table.verticalHeader().setVisible(False)
        mono = QFont("Consolas", 9)
        self._table.setFont(mono)
        self._table.currentItemChanged.connect(lambda cur, _: self._on_row_selected(self._table.row(cur) if cur else -1))
        splitter.addWidget(self._table)

        self._detail = QTextEdit()
        self._detail.setReadOnly(True)
        self._detail.setFont(QFont("Consolas", 9))
        self._detail.setMaximumHeight(160)
        splitter.addWidget(self._detail)

        splitter.setSizes([500, 140])
        layout.addWidget(splitter, stretch=1)

        # ── Active UseCases panel ─────────────────────────────────
        uc_header = QLabel("Active UseCases:")
        uc_header.setStyleSheet("font-weight: bold; color: #aaaaaa;")
        layout.addWidget(uc_header)

        self._usecase_label = QLabel("(none)")
        self._usecase_label.setFont(QFont("Consolas", 9))
        self._usecase_label.setStyleSheet("color: #80ff80;")
        self._usecase_label.setWordWrap(True)
        self._usecase_label.setContentsMargins(12, 0, 0, 6)
        layout.addWidget(self._usecase_label)

        # ── Selection panel ───────────────────────────────────────
        sel_header = QLabel("Selection:")
        sel_header.setStyleSheet("font-weight: bold; color: #aaaaaa;")
        layout.addWidget(sel_header)

        self._selection_label = QLabel("(none)")
        self._selection_label.setFont(QFont("Consolas", 9))
        self._selection_label.setStyleSheet("color: #80d0ff;")
        self._selection_label.setWordWrap(True)
        self._selection_label.setContentsMargins(12, 0, 0, 6)
        layout.addWidget(self._selection_label)

        # ── Intents panel ─────────────────────────────────────────
        intent_header = QLabel("Intents:")
        intent_header.setStyleSheet("font-weight: bold; color: #aaaaaa;")
        layout.addWidget(intent_header)

        self._intents_label = QLabel("(none)")
        self._intents_label.setFont(QFont("Consolas", 9))
        self._intents_label.setStyleSheet("color: #ffb3ff;")
        self._intents_label.setWordWrap(True)
        self._intents_label.setContentsMargins(12, 0, 0, 6)
        layout.addWidget(self._intents_label)

        # ── Status bar ────────────────────────────────────────────
        self._status = QLabel()
        layout.addWidget(self._status)

    # ── Receiving ─────────────────────────────────────────────────

    def _on_message(self, raw: str):
        try:
            entry = json.loads(raw)
        except json.JSONDecodeError:
            entry = {"level": "RAW", "tag": "", "type": "", "payload": raw, "timestamp": 0, "tid": 0}
        entry["_raw"] = raw
        self._process_usecase_lifecycle(entry)
        self._process_selection(entry)
        self._process_intents(entry)
        self._check_critical_errors(entry)
        if not self._paused:
            self._pending.append(entry)

    def _check_critical_errors(self, entry: dict):
        if entry.get("level") != "ERROR":
            return
        payload = entry.get("payload", [])
        if not isinstance(payload, list) or len(payload) < 2:
            return
        event = payload[1]
        if event == "invalid_passive_inheritance":
            name = payload[2] if len(payload) > 2 else "(unknown)"
            QMessageBox.critical(self, "Implementation Error",
                                 f"invalid_passive_inheritance detected:\n\n{name}\n\n"
                                 "canHandle() returned Passive but the UseCase does not inherit PassiveUseCase.")

    def _process_usecase_lifecycle(self, entry: dict):
        if entry.get("type") != USECASE_LIFECYCLE_TYPE:
            return
        # payload is a JSON array: ["start"/"stop", "UseCaseName"]
        payload = entry.get("payload", [])
        if not isinstance(payload, list) or len(payload) < 2:
            return
        event, name = payload[0], payload[1]
        if event == "start":
            self._active_usecases.add(name)
        elif event in ("stop", "finish", "stop_after_remove_if_self"):
            self._active_usecases.discard(name)
        self._refresh_usecase_panel()

    def _refresh_usecase_panel(self):
        if self._active_usecases:
            self._usecase_label.setText("  |  ".join(sorted(self._active_usecases)))
        else:
            self._usecase_label.setText("(none)")

    def _process_selection(self, entry: dict):
        if entry.get("type") != SELECTION_TYPE:
            return
        payload = entry.get("payload", [])
        if not isinstance(payload, list) or not payload:
            return
        event = payload[0]
        if event in ("changed", "set"):
            # Next "item" messages will rebuild the list — reset here on count
            # payload: ["changed"/"set", "count", N]
            try:
                count = int(payload[payload.index("count") + 1])
            except (ValueError, IndexError):
                count = 0
            if count == 0:
                self._current_selection = []
                self._refresh_selection_panel()
            # Items arrive in subsequent "item" messages; panel refreshes there
        elif event == "item":
            # payload: ["item", "index", I, "charIndex", C, "partIndex", P]
            try:
                idx = int(payload[payload.index("index") + 1])
                char_index = int(payload[payload.index("charIndex") + 1])
                part_index = int(payload[payload.index("partIndex") + 1])
            except (ValueError, IndexError):
                return
            # Grow list as needed
            while len(self._current_selection) <= idx:
                self._current_selection.append({"charIndex": ALL_INDICES, "partIndex": ALL_INDICES})
            self._current_selection[idx] = {"charIndex": char_index, "partIndex": part_index}
            self._refresh_selection_panel()

    def _refresh_selection_panel(self):
        if not self._current_selection:
            self._selection_label.setText("(none)")
            return
        parts = []
        for item in self._current_selection:
            c = item["charIndex"]
            p = item["partIndex"]
            if c == ALL_INDICES and p == ALL_INDICES:
                parts.append("All")
            elif c == NO_SELECTION:
                parts.append("NoSelection")
            elif p == ALL_INDICES:
                parts.append(f"char[{c}]")
            else:
                parts.append(f"char[{c}] part[{p}]")
        self._selection_label.setText("  |  ".join(parts))

    def _process_intents(self, entry: dict):
        if entry.get("type") != "Intents":
            return
        payload = entry.get("payload", [])
        if not isinstance(payload, list) or not payload:
            return
        event = payload[0]
        if event == "changed":
            try:
                count = int(payload[payload.index("count") + 1])
            except (ValueError, IndexError):
                count = 0
            self._building_intents = {}
            self._expected_intents_count = count
            if count == 0:
                self._current_intents = {}
                self._refresh_intents_panel()
        elif event == "item":
            try:
                key = str(payload[payload.index("key") + 1])
                typ = str(payload[payload.index("type") + 1])
                if typ == "bool":
                    val = payload[payload.index("val") + 1]
                    val_str = "true" if val is True or val == "true" or val == 1 else "false"
                elif typ == "double":
                    val_str = str(payload[payload.index("val") + 1])
                elif typ == "selection":
                    c = int(payload[payload.index("charIndex") + 1])
                    p = int(payload[payload.index("partIndex") + 1])
                    if c == ALL_INDICES and p == ALL_INDICES:
                        val_str = "All"
                    elif p == ALL_INDICES:
                        val_str = f"char[{c}]"
                    else:
                        val_str = f"char[{c}] part[{p}]"
                else:
                    val_str = typ
                
                self._building_intents[key] = val_str
            except (ValueError, IndexError):
                return
            
            if len(self._building_intents) >= self._expected_intents_count:
                self._current_intents = self._building_intents
                self._refresh_intents_panel()

    def _refresh_intents_panel(self):
        if not self._current_intents:
            self._intents_label.setText("(none)")
            return
        parts = []
        for k, v in sorted(self._current_intents.items()):
            parts.append(f"{k}: {v}")
        self._intents_label.setText("  |  ".join(parts))

    def _flush_pending(self):
        if not self._pending:
            return
        rows = self._pending[:]
        self._pending.clear()
        tokens = self._filter_tokens()
        for entry in rows:
            self._all_rows.append(entry)
            if self._matches(entry, tokens):
                self._append_table_row(entry)
        # Trim if over limit
        overflow = len(self._all_rows) - MAX_ROWS
        if overflow > 0:
            self._all_rows = self._all_rows[overflow:]
        self._update_status()
        if self._autoscroll_cb.isChecked():
            self._table.scrollToBottom()

    # ── Filtering ─────────────────────────────────────────────────

    def _filter_tokens(self) -> list[str]:
        return [t.lower() for t in self._filter_edit.text().split() if t]

    def _matches(self, entry: dict, tokens: list[str]) -> bool:
        if not tokens:
            return True
        haystack = " ".join([
            entry.get("level", ""),
            entry.get("tag", ""),
            entry.get("type", ""),
            self._payload_str(entry.get("payload", "")),
        ]).lower()
        return all(t in haystack for t in tokens)

    def _apply_filter(self):
        tokens = self._filter_tokens()
        self._table.setRowCount(0)
        for entry in self._all_rows:
            if self._matches(entry, tokens):
                self._append_table_row(entry)
        self._update_status()

    # ── UI interactions ───────────────────────────────────────────

    def _on_row_selected(self, row: int):
        if row < 0:
            return
        # Find the corresponding entry via the time+tag cells to locate in _all_rows
        # Simpler: store index in UserRole
        item = self._table.item(row, 0)
        if item is None:
            return
        idx = item.data(Qt.ItemDataRole.UserRole)
        if idx is not None and 0 <= idx < len(self._all_rows):
            raw = self._all_rows[idx].get("_raw", "")
            try:
                pretty = json.dumps(json.loads(raw), indent=2, ensure_ascii=False)
            except Exception:
                pretty = raw
            self._detail.setPlainText(pretty)

    def _selected_entries(self) -> list[dict]:
        rows = sorted(set(idx.row() for idx in self._table.selectedIndexes()))
        entries = []
        for row in rows:
            item = self._table.item(row, 0)
            if item is None:
                continue
            idx = item.data(Qt.ItemDataRole.UserRole)
            if idx is not None and 0 <= idx < len(self._all_rows):
                entries.append(self._all_rows[idx])
        return entries

    def _entries_to_json(self, entries: list[dict]) -> str:
        raws = []
        for e in entries:
            raw = e.get("_raw", "")
            try:
                raws.append(json.loads(raw))
            except Exception:
                raws.append(raw)
        return json.dumps(raws, indent=2, ensure_ascii=False)

    def _copy_selected(self):
        entries = self._selected_entries()
        if not entries:
            return
        QApplication.clipboard().setText(self._entries_to_json(entries))

    def _export_selected(self):
        entries = self._selected_entries()
        if not entries:
            return
        path, _ = QFileDialog.getSaveFileName(self, "Export Selected Logs", "logs.json", "JSON Files (*.json)")
        if not path:
            return
        with open(path, "w", encoding="utf-8") as f:
            f.write(self._entries_to_json(entries))

    def _on_pause(self, checked: bool):
        self._paused = checked
        self._pause_btn.setText("Resume" if checked else "Pause")

    def _clear(self):
        self._all_rows.clear()
        self._pending.clear()
        self._active_usecases.clear()
        self._current_selection.clear()
        self._current_intents.clear()
        self._table.setRowCount(0)
        self._detail.clear()
        self._refresh_usecase_panel()
        self._refresh_selection_panel()
        self._refresh_intents_panel()
        self._update_status()

    def _update_status(self):
        shown = self._table.rowCount()
        total = len(self._all_rows)
        self._status.setText(f"Listening on UDP :{PORT}   |   {shown} shown / {total} total")

    # ── Helpers ───────────────────────────────────────────────────

    @staticmethod
    def _payload_str(payload) -> str:
        if isinstance(payload, (dict, list)):
            return json.dumps(payload, ensure_ascii=False)
        return str(payload)

    # Store original row index in UserRole when appending
    def _append_table_row(self, entry: dict):
        row = self._table.rowCount()
        self._table.insertRow(row)

        ts = entry.get("timestamp", 0)
        time_str = datetime.fromtimestamp(ts / 1000).strftime("%H:%M:%S.") + f"{ts % 1000:03d}"
        level = entry.get("level", "")
        tag = entry.get("tag", "")
        typ = entry.get("type", "")
        payload = self._payload_str(entry.get("payload", ""))
        tid = str(entry.get("tid", ""))[:8]

        cells = [time_str, level, tag, typ, payload, tid]
        color = LEVEL_COLORS.get(level, LEVEL_COLORS["INFO"])
        orig_index = len(self._all_rows) - 1
        for col, text in enumerate(cells):
            item = QTableWidgetItem(str(text))
            item.setForeground(color)
            if col == 0:
                item.setData(Qt.ItemDataRole.UserRole, orig_index)
            self._table.setItem(row, col, item)

    def closeEvent(self, event):
        self._receiver.stop()
        super().closeEvent(event)


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
