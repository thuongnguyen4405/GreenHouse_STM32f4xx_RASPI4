#!/usr/bin/env python3
"""
Smart Greenhouse & Fire Alarm — Raspberry Pi SPI Dashboard
═══════════════════════════════════════════════════════════
Reads a 16-byte binary frame from STM32F411 (SPI slave) and
renders a real-time Tkinter dashboard with:

  • Temperature gauge (LM35) with colour-coded alarm state
  • Gas level gauge with colour-coded alarm state
  • 4× raw ADC channel readouts
  • Actuator indicators (buzzer, motor)
  • Rolling history chart (last 120 seconds)
  • Connection status & frame error statistics

SPI Frame (16 bytes, little-endian):
  [0]  0xAA  magic
  [1]  0x55  magic
  [2]  SEQ   sequence counter
  [3]  STATUS  bit-field (buzzer|motor|gas_alarm|temp_alarm)
  [4-5]   ADC0 (LM35 raw)
  [6-7]   ADC1 (Gas raw)
  [8-9]   ADC2 (Sensor 3)
  [10-11]  ADC3 (Sensor 4)
  [12-13]  TEMP_X10  (temperature × 10, 0.1 °C)
  [14]  XOR checksum of [0..13]
  [15]  0x0D  end marker

Author : Thuong
Date   : 2025
"""

from __future__ import annotations

import sys
import time
import struct
import threading
import logging
from collections import deque
from dataclasses import dataclass, field
from typing import Optional

import tkinter as tk
from tkinter import ttk, messagebox

# ── Optional: matplotlib for history chart ──────────────────
try:
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

# ── Optional: spidev (graceful fallback for dev on non-Pi) ──
try:
    import spidev
    HAS_SPIDEV = True
except ImportError:
    HAS_SPIDEV = False

# ════════════════════════════════════════════════════════════
#  CONFIGURATION — mirrors board.h on STM32 side
#
#  These constants MUST match board.h Section 7 exactly.
#  If you change the STM32 protocol, update both files.
# ════════════════════════════════════════════════════════════

# Frame geometry (board.h §7 — PACKET_LEN)
PACKET_LEN       = 16

# Magic bytes (board.h §7 — FRAME_MAGIC_0/1, FRAME_END_MARKER)
MAGIC_0          = 0xAA
MAGIC_1          = 0x55
END_MARKER       = 0x0D

# Byte offsets (board.h §7 — FRAME_OFF_*)
OFF_MAGIC0       = 0
OFF_MAGIC1       = 1
OFF_SEQ          = 2
OFF_STATUS       = 3
OFF_ADC0_L       = 4
OFF_ADC0_H       = 5
OFF_ADC1_L       = 6
OFF_ADC1_H       = 7
OFF_ADC2_L       = 8
OFF_ADC2_H       = 9
OFF_ADC3_L       = 10
OFF_ADC3_H       = 11
OFF_TEMP_L       = 12
OFF_TEMP_H       = 13
OFF_XOR          = 14
OFF_END          = 15

# STATUS bit positions (board.h §7 — STATUS_BIT_*)
STATUS_BIT_BUZZER     = 0
STATUS_BIT_MOTOR      = 1
STATUS_BIT_GAS_ALARM  = 2
STATUS_BIT_TEMP_ALARM = 3

# SPI bus parameters (board.h §7 — SPI_CLOCK_HZ, SPI_CPOL, SPI_CPHA)
SPI_BUS          = 0
SPI_DEV          = 0
SPI_SPEED_HZ     = 1_000_000   # must match SPI_CLOCK_HZ in board.h
SPI_MODE         = 0b00        # Mode 0 (CPOL=0, CPHA=0)

# Alarm thresholds for GUI colour (board.h §5)
TEMP_WARN_THRESH  = 35.0       # board.h TEMP_WARN_ON_X10 / 10
TEMP_ALARM_THRESH = 50.0       # board.h TEMP_ALARM_ON_X10 / 10
GAS_WARN_THRESH   = 2000       # board.h GAS_WARN_ON_ADC
GAS_ALARM_THRESH  = 2500       # board.h GAS_ALARM_ON_ADC

POLL_INTERVAL_S  = 0.02      # 50 Hz SPI poll
UI_REFRESH_MS    = 100       # 10 Hz GUI update
CHART_HISTORY_S  = 120       # seconds of chart history
CHART_POINTS     = int(CHART_HISTORY_S / (UI_REFRESH_MS / 1000))

# Alarm thresholds — aliases for display colour logic
# (primary defines are TEMP_WARN_THRESH etc. above)
TEMP_WARN_ON     = TEMP_WARN_THRESH
TEMP_ALARM_ON    = TEMP_ALARM_THRESH
GAS_WARN_ON      = GAS_WARN_THRESH
GAS_ALARM_ON     = GAS_ALARM_THRESH

# Colour palette
CLR_BG           = "#1e1e2e"
CLR_CARD         = "#2a2a3c"
CLR_TEXT         = "#cdd6f4"
CLR_DIM          = "#6c7086"
CLR_NORMAL       = "#a6e3a1"
CLR_WARN         = "#f9e2af"
CLR_ALARM        = "#f38ba8"
CLR_ACCENT       = "#89b4fa"
CLR_BUZZER_ON    = "#fab387"
CLR_MOTOR_ON     = "#74c7ec"
CLR_OFF          = "#45475a"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("greenhouse")

# ════════════════════════════════════════════════════════════
#  DATA MODEL
# ════════════════════════════════════════════════════════════

@dataclass
class SensorFrame:
    """Parsed representation of one 16-byte SPI frame."""
    seq:        int = 0
    status:     int = 0
    adc:        tuple = (0, 0, 0, 0)
    temp_x10:   int = 0          # 0.1 °C units
    temp_c:     float = 0.0
    buzzer:     bool = False
    motor:      bool = False
    gas_alarm:  bool = False
    temp_alarm: bool = False
    timestamp:  float = field(default_factory=time.monotonic)

    @property
    def gas_raw(self) -> int:
        return self.adc[1]

    def alarm_level_temp(self) -> str:
        """Return 'NORMAL', 'WARN', or 'ALARM' based on thresholds."""
        if self.temp_c >= TEMP_ALARM_ON:
            return "ALARM"
        if self.temp_c >= TEMP_WARN_ON:
            return "WARN"
        return "NORMAL"

    def alarm_level_gas(self) -> str:
        if self.gas_raw >= GAS_ALARM_ON:
            return "ALARM"
        if self.gas_raw >= GAS_WARN_ON:
            return "WARN"
        return "NORMAL"


@dataclass
class FrameStats:
    """Tracks SPI communication health."""
    total_reads:       int = 0
    valid_frames:      int = 0
    magic_errors:      int = 0
    checksum_errors:   int = 0
    length_errors:     int = 0
    last_seq:          int = -1
    seq_gaps:          int = 0

    @property
    def error_total(self) -> int:
        return self.magic_errors + self.checksum_errors + self.length_errors

    @property
    def error_rate_pct(self) -> float:
        if self.total_reads == 0:
            return 0.0
        return (self.error_total / self.total_reads) * 100.0

# ════════════════════════════════════════════════════════════
#  SPI PROTOCOL LAYER
# ════════════════════════════════════════════════════════════

def xor_checksum(buf, length=14):
    """XOR checksum over bytes [0..length-1]."""
    cs = 0
    for i in range(min(length, len(buf))):
        cs ^= buf[i]
    return cs & 0xFF


def parse_frame(raw):
    """
    Parse a 16-byte raw SPI frame into a SensorFrame.
    Returns None if validation fails.

    Byte layout matches board.h Section 7 (FRAME_OFF_* defines).
    """
    if len(raw) != PACKET_LEN:
        return None
    if raw[OFF_MAGIC0] != MAGIC_0 or raw[OFF_MAGIC1] != MAGIC_1:
        return None
    if raw[OFF_END] != END_MARKER:
        return None
    if raw[OFF_XOR] != xor_checksum(raw):
        return None

    status = raw[OFF_STATUS]
    # Unpack 4× uint16 LE for ADC + 1× uint16 LE for temp
    adc0 = raw[OFF_ADC0_L] | (raw[OFF_ADC0_H] << 8)
    adc1 = raw[OFF_ADC1_L] | (raw[OFF_ADC1_H] << 8)
    adc2 = raw[OFF_ADC2_L] | (raw[OFF_ADC2_H] << 8)
    adc3 = raw[OFF_ADC3_L] | (raw[OFF_ADC3_H] << 8)
    temp_x10 = raw[OFF_TEMP_L] | (raw[OFF_TEMP_H] << 8)

    return SensorFrame(
        seq       = raw[OFF_SEQ],
        status    = status,
        adc       = (adc0, adc1, adc2, adc3),
        temp_x10  = temp_x10,
        temp_c    = temp_x10 / 10.0,
        buzzer    = bool(status & (1 << STATUS_BIT_BUZZER)),
        motor     = bool(status & (1 << STATUS_BIT_MOTOR)),
        gas_alarm = bool(status & (1 << STATUS_BIT_GAS_ALARM)),
        temp_alarm= bool(status & (1 << STATUS_BIT_TEMP_ALARM)),
    )

# ════════════════════════════════════════════════════════════
#  SPI READER (background thread)
# ════════════════════════════════════════════════════════════

class SpiReader:
    """
    Background thread that continuously polls the STM32 SPI slave.

    Thread safety: `latest` and `stats` are protected by a lock.
    The GUI thread calls get_snapshot() to read both atomically.
    """

    def __init__(
        self,
        bus=SPI_BUS,
        dev=SPI_DEV,
        hz=SPI_SPEED_HZ,
        mode=SPI_MODE,
        simulate=False,
    ):
        self.bus = bus
        self.dev = dev
        self.hz = hz
        self.mode = mode
        self.simulate = simulate

        self._spi = None
        self._lock = threading.Lock()
        self._running = False
        self._thread = None

        self.latest = None
        self.stats = FrameStats()

        # History ring buffers for charting
        self.temp_history = deque(maxlen=CHART_POINTS)
        self.gas_history  = deque(maxlen=CHART_POINTS)
        self.time_history = deque(maxlen=CHART_POINTS)

    # ── lifecycle ──────────────────────────────────────────

    def start(self):
        """Open SPI and start polling thread. Returns True on success."""
        if self._running:
            return True

        if not self.simulate:
            if not HAS_SPIDEV:
                log.error("spidev module not installed. "
                          "Install with: pip3 install spidev")
                return False
            try:
                self._spi = spidev.SpiDev()
                self._spi.open(self.bus, self.dev)
                self._spi.max_speed_hz = self.hz
                self._spi.mode = self.mode
                log.info("SPI%d.%d opened @ %d Hz, mode %d",
                         self.bus, self.dev, self.hz, self.mode)
            except (FileNotFoundError, PermissionError, OSError) as exc:
                log.error("Cannot open SPI: %s", exc)
                return False
        else:
            log.info("Running in SIMULATION mode (no real SPI)")

        self._running = True
        self._thread = threading.Thread(target=self._poll_loop,
                                        daemon=True, name="spi-poll")
        self._thread.start()
        return True

    def stop(self):
        """Signal thread to stop and close SPI."""
        self._running = False
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=0.5)
        if self._spi:
            try:
                self._spi.close()
            except Exception:
                pass
            self._spi = None
        log.info("SPI reader stopped.")

    # ── public getters (thread-safe) ──────────────────────

    def get_snapshot(self):
        """Return a consistent (frame, stats) pair."""
        with self._lock:
            return self.latest, FrameStats(**self.stats.__dict__)

    def get_history(self):
        """Return copies of chart history deques."""
        with self._lock:
            return (list(self.time_history),
                    list(self.temp_history),
                    list(self.gas_history))

    # ── internal ─────────────────────────────────────────

    def _poll_loop(self):
        while self._running:
            try:
                raw = self._read_raw()
                self._process(raw)
            except Exception as exc:
                log.warning("SPI read error: %s", exc)
            time.sleep(POLL_INTERVAL_S)

    def _read_raw(self):
        if self.simulate:
            return self._simulate_frame()
        return self._spi.xfer2([0x00] * PACKET_LEN)

    def _process(self, raw):
        with self._lock:
            self.stats.total_reads += 1

            if len(raw) != PACKET_LEN:
                self.stats.length_errors += 1
                return

            if raw[OFF_MAGIC0] != MAGIC_0 or raw[OFF_MAGIC1] != MAGIC_1 or raw[OFF_END] != END_MARKER:
                self.stats.magic_errors += 1
                return

            if raw[OFF_XOR] != xor_checksum(raw):
                self.stats.checksum_errors += 1
                return

            frame = parse_frame(raw)
            if frame is None:
                return

            self.stats.valid_frames += 1

            # Detect sequence gaps
            if self.stats.last_seq >= 0:
                expected = (self.stats.last_seq + 1) & 0xFF
                if frame.seq != expected:
                    self.stats.seq_gaps += 1
            self.stats.last_seq = frame.seq

            self.latest = frame

            # Push to chart history
            now = time.monotonic()
            self.temp_history.append(frame.temp_c)
            self.gas_history.append(frame.gas_raw)
            self.time_history.append(now)

    # ── simulation (for testing without hardware) ────────

    _sim_seq = 0
    _sim_t0 = time.monotonic()

    def _simulate_frame(self):
        """Generate a synthetic valid frame for UI development."""
        import math
        t = time.monotonic() - self._sim_t0

        # Simulate temperature oscillating 25–55 °C
        temp_c = 30.0 + 20.0 * math.sin(t * 0.1)
        temp_x10 = int(temp_c * 10) & 0xFFFF

        # Simulate gas oscillating 500–3000
        gas = int(1500 + 1200 * math.sin(t * 0.07 + 1.0)) & 0xFFFF

        adc0 = int(temp_x10 * 4095 / 3300) & 0xFFFF  # reverse LM35 calc
        adc1 = gas
        adc2 = int(2000 + 500 * math.sin(t * 0.05)) & 0xFFFF
        adc3 = int(1000 + 800 * math.cos(t * 0.03)) & 0xFFFF

        # Determine alarm status (bit positions from board.h §7)
        status = 0
        if temp_c >= TEMP_WARN_ON:
            status |= (1 << STATUS_BIT_TEMP_ALARM)
        if gas >= GAS_WARN_ON:
            status |= (1 << STATUS_BIT_GAS_ALARM)
        if temp_c >= TEMP_ALARM_ON or gas >= GAS_ALARM_ON:
            status |= (1 << STATUS_BIT_BUZZER)
        if temp_c >= TEMP_ALARM_ON:
            status |= (1 << STATUS_BIT_MOTOR)

        buf = [0] * PACKET_LEN
        buf[OFF_MAGIC0] = MAGIC_0
        buf[OFF_MAGIC1] = MAGIC_1
        buf[OFF_SEQ]    = self._sim_seq & 0xFF
        self._sim_seq  += 1
        buf[OFF_STATUS] = status
        buf[OFF_ADC0_L] = adc0 & 0xFF;        buf[OFF_ADC0_H] = (adc0 >> 8) & 0xFF
        buf[OFF_ADC1_L] = adc1 & 0xFF;        buf[OFF_ADC1_H] = (adc1 >> 8) & 0xFF
        buf[OFF_ADC2_L] = adc2 & 0xFF;        buf[OFF_ADC2_H] = (adc2 >> 8) & 0xFF
        buf[OFF_ADC3_L] = adc3 & 0xFF;        buf[OFF_ADC3_H] = (adc3 >> 8) & 0xFF
        buf[OFF_TEMP_L] = temp_x10 & 0xFF;    buf[OFF_TEMP_H] = (temp_x10 >> 8) & 0xFF
        buf[OFF_XOR]    = xor_checksum(buf)
        buf[OFF_END]    = END_MARKER
        return buf

# ════════════════════════════════════════════════════════════
#  GUI — DASHBOARD
# ════════════════════════════════════════════════════════════

class GaugeCard(tk.Frame):
    """
    A single sensor card with:
      - Title label
      - Large value label (colour-coded)
      - Alarm state label (NORMAL / WARN / ALARM)
      - Bar gauge
    """

    def __init__(self, parent, title="", unit="",
                 warn_threshold=0, alarm_threshold=0,
                 max_value=100, **kw):
        super().__init__(parent, bg=CLR_CARD, padx=16, pady=12, **kw)
        self.unit = unit
        self.warn_threshold = warn_threshold
        self.alarm_threshold = alarm_threshold
        self.max_value = max_value

        # Title
        self.lbl_title = tk.Label(
            self, text=title, font=("Segoe UI", 11, "bold"),
            fg=CLR_DIM, bg=CLR_CARD, anchor="w")
        self.lbl_title.pack(fill="x")

        # Value
        self.lbl_value = tk.Label(
            self, text="--.-", font=("Consolas", 28, "bold"),
            fg=CLR_TEXT, bg=CLR_CARD, anchor="w")
        self.lbl_value.pack(fill="x", pady=(4, 0))

        # Unit / sublabel
        self.lbl_unit = tk.Label(
            self, text=unit, font=("Segoe UI", 10),
            fg=CLR_DIM, bg=CLR_CARD, anchor="w")
        self.lbl_unit.pack(fill="x")

        # Bar gauge
        self.bar_frame = tk.Frame(self, bg=CLR_BG, height=8)
        self.bar_frame.pack(fill="x", pady=(8, 0))
        self.bar_fill = tk.Frame(self.bar_frame, bg=CLR_NORMAL, height=8)
        self.bar_fill.place(x=0, y=0, relheight=1.0, relwidth=0.0)

        # State label
        self.lbl_state = tk.Label(
            self, text="NORMAL", font=("Segoe UI", 9, "bold"),
            fg=CLR_NORMAL, bg=CLR_CARD, anchor="e")
        self.lbl_state.pack(fill="x", pady=(4, 0))

    def update_value(self, value, state="NORMAL", fmt="{:.1f}"):
        """Update displayed value, bar, and alarm colour."""
        self.lbl_value.config(text=fmt.format(value))

        colour = {
            "NORMAL": CLR_NORMAL,
            "WARN":   CLR_WARN,
            "ALARM":  CLR_ALARM,
        }.get(state, CLR_NORMAL)

        self.lbl_value.config(fg=colour)
        self.lbl_state.config(text=state, fg=colour)

        # Bar fill (clamped 0..1)
        ratio = max(0.0, min(1.0, value / self.max_value)) if self.max_value else 0
        self.bar_fill.config(bg=colour)
        self.bar_fill.place(x=0, y=0, relheight=1.0, relwidth=ratio)


class IndicatorDot(tk.Frame):
    """A simple ON/OFF indicator with label."""

    def __init__(self, parent, label="", on_colour=CLR_ACCENT, **kw):
        super().__init__(parent, bg=CLR_CARD, **kw)
        self.on_colour = on_colour

        self.dot = tk.Canvas(self, width=18, height=18,
                             bg=CLR_CARD, highlightthickness=0)
        self.dot.pack(side="left", padx=(0, 6))
        self._oval = self.dot.create_oval(2, 2, 16, 16, fill=CLR_OFF,
                                           outline="")

        self.lbl = tk.Label(self, text=label, font=("Segoe UI", 11),
                            fg=CLR_TEXT, bg=CLR_CARD)
        self.lbl.pack(side="left")

    def set_on(self, on):
        colour = self.on_colour if on else CLR_OFF
        self.dot.itemconfig(self._oval, fill=colour)


class DashboardApp:
    """Main application: assembles all widgets and runs the update loop."""

    def __init__(self, reader):
        self.reader = reader
        self.root = tk.Tk()
        self.root.title("Smart Greenhouse — Fire Alarm Dashboard")
        self.root.configure(bg=CLR_BG)
        self.root.minsize(880, 620)

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── UI construction ──────────────────────────────────

    def _build_ui(self):
        # ── Header ──
        hdr = tk.Frame(self.root, bg=CLR_BG)
        hdr.pack(fill="x", padx=16, pady=(12, 0))

        tk.Label(hdr, text="Smart Greenhouse Monitor",
                 font=("Segoe UI", 18, "bold"),
                 fg=CLR_TEXT, bg=CLR_BG).pack(side="left")

        self.lbl_conn = tk.Label(
            hdr, text="CONNECTED", font=("Segoe UI", 10, "bold"),
            fg=CLR_NORMAL, bg=CLR_BG)
        self.lbl_conn.pack(side="right")

        # ── Top row: gauges ──
        row1 = tk.Frame(self.root, bg=CLR_BG)
        row1.pack(fill="x", padx=16, pady=(12, 0))

        self.gauge_temp = GaugeCard(
            row1, title="TEMPERATURE (LM35)", unit="°C",
            warn_threshold=TEMP_WARN_ON, alarm_threshold=TEMP_ALARM_ON,
            max_value=80.0)
        self.gauge_temp.pack(side="left", fill="both", expand=True, padx=(0, 6))

        self.gauge_gas = GaugeCard(
            row1, title="GAS LEVEL (MQ-2)", unit="ADC raw (0-4095)",
            warn_threshold=GAS_WARN_ON, alarm_threshold=GAS_ALARM_ON,
            max_value=4095.0)
        self.gauge_gas.pack(side="left", fill="both", expand=True, padx=(6, 0))

        # ── Middle row: ADC raw + actuators ──
        row2 = tk.Frame(self.root, bg=CLR_BG)
        row2.pack(fill="x", padx=16, pady=(12, 0))

        # ADC raw card
        adc_card = tk.Frame(row2, bg=CLR_CARD, padx=16, pady=12)
        adc_card.pack(side="left", fill="both", expand=True, padx=(0, 6))

        tk.Label(adc_card, text="ADC RAW VALUES",
                 font=("Segoe UI", 11, "bold"),
                 fg=CLR_DIM, bg=CLR_CARD, anchor="w").pack(fill="x")

        self.lbl_adc = []
        adc_labels = [
            "CH0 - LM35 (Temperature)",
            "CH1 - MQ-2 (Gas)",
            "CH2 - Soil Moisture",
            "CH3 - Light Level",
        ]
        for i, name in enumerate(adc_labels):
            frm = tk.Frame(adc_card, bg=CLR_CARD)
            frm.pack(fill="x", pady=2)
            tk.Label(frm, text=name, font=("Segoe UI", 10),
                     fg=CLR_DIM, bg=CLR_CARD, width=28,
                     anchor="w").pack(side="left")
            val = tk.Label(frm, text="----", font=("Consolas", 12, "bold"),
                           fg=CLR_TEXT, bg=CLR_CARD, anchor="e")
            val.pack(side="right")
            self.lbl_adc.append(val)

        # Actuators card
        act_card = tk.Frame(row2, bg=CLR_CARD, padx=16, pady=12)
        act_card.pack(side="left", fill="both", expand=True, padx=(6, 0))

        tk.Label(act_card, text="ACTUATORS & ALARMS",
                 font=("Segoe UI", 11, "bold"),
                 fg=CLR_DIM, bg=CLR_CARD, anchor="w").pack(fill="x",
                                                            pady=(0, 8))

        self.ind_buzzer = IndicatorDot(act_card, "Buzzer",
                                        on_colour=CLR_BUZZER_ON)
        self.ind_buzzer.pack(fill="x", pady=3)

        self.ind_motor = IndicatorDot(act_card, "Motor / Fan",
                                       on_colour=CLR_MOTOR_ON)
        self.ind_motor.pack(fill="x", pady=3)

        self.ind_gas_alarm = IndicatorDot(act_card, "Gas Alarm",
                                           on_colour=CLR_ALARM)
        self.ind_gas_alarm.pack(fill="x", pady=3)

        self.ind_temp_alarm = IndicatorDot(act_card, "Temp Alarm",
                                            on_colour=CLR_ALARM)
        self.ind_temp_alarm.pack(fill="x", pady=3)

        # Sequence / overall state
        self.lbl_overall = tk.Label(
            act_card, text="STATE: NORMAL", font=("Segoe UI", 13, "bold"),
            fg=CLR_NORMAL, bg=CLR_CARD, anchor="center")
        self.lbl_overall.pack(fill="x", pady=(12, 0))

        # ── Chart row (if matplotlib available) ──
        if HAS_MATPLOTLIB:
            chart_frame = tk.Frame(self.root, bg=CLR_CARD)
            chart_frame.pack(fill="both", expand=True, padx=16, pady=(12, 0))

            self.fig = Figure(figsize=(8, 2.4), dpi=100, facecolor=CLR_CARD)
            self.ax_temp = self.fig.add_subplot(121)
            self.ax_gas  = self.fig.add_subplot(122)

            for ax, title, ylabel in [
                (self.ax_temp, "Temperature (°C)", "°C"),
                (self.ax_gas,  "Gas Level (ADC)",  "raw"),
            ]:
                ax.set_facecolor(CLR_BG)
                ax.set_title(title, color=CLR_DIM, fontsize=9, pad=6)
                ax.set_ylabel(ylabel, color=CLR_DIM, fontsize=8)
                ax.tick_params(colors=CLR_DIM, labelsize=7)
                for spine in ax.spines.values():
                    spine.set_color(CLR_DIM)
                    spine.set_linewidth(0.5)

            self.fig.tight_layout(pad=2.0)
            self.canvas = FigureCanvasTkAgg(self.fig, master=chart_frame)
            self.canvas.get_tk_widget().pack(fill="both", expand=True)
        else:
            self.fig = None
            # Fallback: info label
            tk.Label(self.root,
                     text="Install matplotlib for live charts:  "
                          "pip3 install matplotlib",
                     font=("Segoe UI", 9), fg=CLR_DIM,
                     bg=CLR_BG).pack(pady=(12, 0))

        # ── Footer: stats ──
        footer = tk.Frame(self.root, bg=CLR_BG)
        footer.pack(fill="x", padx=16, pady=(8, 12))

        self.lbl_stats = tk.Label(
            footer, text="Frames: 0  |  Errors: 0 (0.0%)  |  SEQ gaps: 0",
            font=("Consolas", 9), fg=CLR_DIM, bg=CLR_BG, anchor="w")
        self.lbl_stats.pack(side="left")

        self.lbl_sim = tk.Label(
            footer, text="", font=("Segoe UI", 9, "italic"),
            fg=CLR_WARN, bg=CLR_BG, anchor="e")
        self.lbl_sim.pack(side="right")

    # ── periodic UI update ───────────────────────────────

    def _schedule_update(self):
        self.root.after(UI_REFRESH_MS, self._ui_tick)

    def _ui_tick(self):
        frame, stats = self.reader.get_snapshot()

        if frame is None:
            # No data yet
            self.lbl_conn.config(text="WAITING FOR DATA", fg=CLR_WARN)
            self._schedule_update()
            return

        self.lbl_conn.config(text="CONNECTED", fg=CLR_NORMAL)

        # Temperature gauge
        temp_state = frame.alarm_level_temp()
        self.gauge_temp.update_value(frame.temp_c, temp_state, fmt="{:.1f}")

        # Gas gauge
        gas_state = frame.alarm_level_gas()
        self.gauge_gas.update_value(frame.gas_raw, gas_state, fmt="{:.0f}")

        # ADC raw values
        for i in range(4):
            self.lbl_adc[i].config(text=f"{frame.adc[i]:>5d}")

        # Actuator indicators
        self.ind_buzzer.set_on(frame.buzzer)
        self.ind_motor.set_on(frame.motor)
        self.ind_gas_alarm.set_on(frame.gas_alarm)
        self.ind_temp_alarm.set_on(frame.temp_alarm)

        # Overall state
        if frame.temp_alarm or frame.gas_alarm:
            # Check which is worse using thresholds
            worst = "WARN"
            if frame.temp_c >= TEMP_ALARM_ON or frame.gas_raw >= GAS_ALARM_ON:
                worst = "ALARM"
            colour = CLR_ALARM if worst == "ALARM" else CLR_WARN
            self.lbl_overall.config(text=f"STATE: {worst}", fg=colour)
        else:
            self.lbl_overall.config(text="STATE: NORMAL", fg=CLR_NORMAL)

        # Stats footer
        self.lbl_stats.config(
            text=(f"Frames: {stats.valid_frames}  |  "
                  f"Errors: {stats.error_total} ({stats.error_rate_pct:.1f}%)  |  "
                  f"SEQ gaps: {stats.seq_gaps}  |  "
                  f"SEQ: {frame.seq}"))

        # Chart
        if self.fig is not None:
            self._update_chart()

        self._schedule_update()

    def _update_chart(self):
        times, temps, gases = self.reader.get_history()
        if len(times) < 2:
            return

        # Convert time to "seconds ago"
        now = time.monotonic()
        t_rel = [(t - now) for t in times]

        self.ax_temp.clear()
        self.ax_temp.set_facecolor(CLR_BG)
        self.ax_temp.set_title("Temperature (°C)", color=CLR_DIM,
                               fontsize=9, pad=6)
        self.ax_temp.plot(t_rel, temps, color=CLR_ACCENT, linewidth=1.2)
        self.ax_temp.axhline(y=TEMP_WARN_ON, color=CLR_WARN,
                             linewidth=0.8, linestyle="--", alpha=0.7)
        self.ax_temp.axhline(y=TEMP_ALARM_ON, color=CLR_ALARM,
                             linewidth=0.8, linestyle="--", alpha=0.7)
        self.ax_temp.tick_params(colors=CLR_DIM, labelsize=7)
        for spine in self.ax_temp.spines.values():
            spine.set_color(CLR_DIM)
            spine.set_linewidth(0.5)

        self.ax_gas.clear()
        self.ax_gas.set_facecolor(CLR_BG)
        self.ax_gas.set_title("Gas Level (ADC raw)", color=CLR_DIM,
                              fontsize=9, pad=6)
        self.ax_gas.plot(t_rel, gases, color="#fab387", linewidth=1.2)
        self.ax_gas.axhline(y=GAS_WARN_ON, color=CLR_WARN,
                            linewidth=0.8, linestyle="--", alpha=0.7)
        self.ax_gas.axhline(y=GAS_ALARM_ON, color=CLR_ALARM,
                            linewidth=0.8, linestyle="--", alpha=0.7)
        self.ax_gas.tick_params(colors=CLR_DIM, labelsize=7)
        for spine in self.ax_gas.spines.values():
            spine.set_color(CLR_DIM)
            spine.set_linewidth(0.5)

        self.fig.tight_layout(pad=2.0)
        self.canvas.draw_idle()

    # ── lifecycle ────────────────────────────────────────

    def _on_close(self):
        self.reader.stop()
        self.root.destroy()

    def run(self):
        """Start the reader and enter Tkinter mainloop."""
        if self.reader.simulate:
            self.lbl_sim.config(text="SIMULATION MODE - no real SPI")

        if not self.reader.start():
            messagebox.showerror(
                "SPI Error",
                "Cannot open SPI device.\n\n"
                "- Is SPI enabled?  sudo raspi-config -> Interface -> SPI\n"
                "- Device exists?   ls /dev/spidev0.*\n"
                "- Permission?      sudo usermod -aG spi $USER\n\n"
                "Starting in SIMULATION mode instead.")
            self.reader.simulate = True
            self.reader.start()
            self.lbl_sim.config(text="SIMULATION MODE - no real SPI")

        self._schedule_update()
        self.root.mainloop()


# ════════════════════════════════════════════════════════════
#  ENTRY POINT
# ════════════════════════════════════════════════════════════

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Smart Greenhouse SPI Dashboard")
    parser.add_argument("--simulate", "-s", action="store_true",
                        help="Run with simulated data (no SPI hardware)")
    parser.add_argument("--bus", type=int, default=SPI_BUS,
                        help=f"SPI bus number (default: {SPI_BUS})")
    parser.add_argument("--dev", type=int, default=SPI_DEV,
                        help=f"SPI device number (default: {SPI_DEV})")
    parser.add_argument("--speed", type=int, default=SPI_SPEED_HZ,
                        help=f"SPI clock speed Hz (default: {SPI_SPEED_HZ})")
    args = parser.parse_args()

    reader = SpiReader(
        bus=args.bus,
        dev=args.dev,
        hz=args.speed,
        simulate=args.simulate,
    )

    app = DashboardApp(reader)
    app.run()


if __name__ == "__main__":
    main()
