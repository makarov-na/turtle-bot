#!/usr/bin/env python3
"""Консольный пульт управления ботом по Bluetooth (HC-06).

Запуск:
    python3 controller.py /dev/rfcomm0

Перед запуском убедись, что RFCOMM-канал жив:
    - бот включён, HC-06 спарен с ПК (PIN 1234);
    - канал привязан: sudo rfcomm bind 0 00:21:13:00:0C:E3 1

Кнопки:
    Up    — линейная скорость +10%
    Down  — линейная скорость -10% (ниже 0 — задний ход)
    Left  — угловая скорость +10%
    Right — угловая скорость -10%
    Space — пауза: мгновенный стоп и заморозка телеметрии; ещё раз — продолжить
    q     — выход (перед выходом боту уходит set_speed 0.0 0.0)

Интерфейс — полноэкранная панель (alternate screen buffer): статус с
абсолютными значениями команды, живая таблица последних 20 ack-ответов
бота (свежие сверху, последний подсвечен инверсией) и футер-подсказки.
На выходе экран восстанавливается.

Текущая команда повторяется каждые 200 мс (heartbeat, питает watchdog
прошивки). Используется только стандартная библиотека Python.
"""

import argparse
import errno
import fcntl
import os
import re
import select
import shutil
import sys
import termios
import time
import tty
from collections import deque
from dataclasses import dataclass

HEARTBEAT_MS = 200
STEP_PCT = 10.0
CLAMP_MIN = -100.0
CLAMP_MAX = 100.0
BT_BAUD = 9600
LINK_TIMEOUT_S = 5.0

V_MAX_MM_S = 300.0
WHEELBASE_MM = 185.0
# Лимиты должны совпадать с константами V_MAX_MM_S и WHEELBASE_MM в
# src/motor_controller/motor_controller/motor_controller.ino
OMEGA_MAX_RAD_S = 2.0 * V_MAX_MM_S / WHEELBASE_MM

KEY_UP = (b"\x1b[A", b"\x1bOA")
KEY_DOWN = (b"\x1b[B", b"\x1bOB")
KEY_RIGHT = (b"\x1b[C", b"\x1bOC")
KEY_LEFT = (b"\x1b[D", b"\x1bOD")

SPACE = b" "

STOP_COMMAND = "set_speed 0.0 0.0\n"

RECONNECT_ATTEMPTS = 5
RECONNECT_DELAY_S = 0.5

TELEMETRY_LINES = 20

# Анти-грязь: если вдруг пойдёт мусор без перевода строки, буфер не растёт.
MAX_RX_BUFFER = 512

serial_fd = None


def write_all(fd, data):
    while data:
        n = os.write(fd, data)
        data = data[n:]


def format_command(linear_pct, angular_pct):
    linear_mm_s = linear_pct / 100.0 * V_MAX_MM_S
    omega_rad_s = angular_pct / 100.0 * OMEGA_MAX_RAD_S
    return "set_speed {:.1f} {:.2f}\n".format(linear_mm_s, omega_rad_s)


def send_command(fd, linear_pct, angular_pct):
    try:
        write_all(fd, format_command(linear_pct, angular_pct).encode())
    except OSError as exc:
        raise LinkLost() from exc


def outgoing_speed(state):
    if state.get("paused"):
        return 0.0, 0.0
    return state["linear"], state["angular"]


class LinkLost(Exception):
    pass


def status_line(linear_pct, angular_pct):
    linear_mm_s = linear_pct / 100.0 * V_MAX_MM_S
    omega_rad_s = angular_pct / 100.0 * OMEGA_MAX_RAD_S
    return ("lin {:+.0f}% ({:+.0f} мм/с)  ang {:+.0f}% ({:+.2f} рад/с)"
            .format(linear_pct, linear_mm_s, angular_pct, omega_rad_s))


# ---------------------------------------------------------------------------
# Телеметрия: парсинг ack и кольцевой буфер последних значений.
# Формат строки с бота (motor_controller.ino):
#   ack <n> <v> <w> mL=<ml> mR=<mr> oL=<ol> oR=<or> iL=<il> iR=<ir>
# mL/mR — измеренная скорость (мм/с), oL/oR — выход ШИМ (±255),
# iL/iR — интеграл ПИД (±limit). Невалидные строки возвращают None.
# ---------------------------------------------------------------------------
_ACK_RE = re.compile(
    r"^ack\s+(\d+)\s+([-+]?\d*\.?\d+)\s+([-+]?\d*\.?\d+)"
    r"\s+mL=([-+]?\d*\.?\d+) mR=([-+]?\d*\.?\d+)"
    r"\s+oL=([-+]?\d*\.?\d+) oR=([-+]?\d*\.?\d+)"
    r"\s+iL=([-+]?\d*\.?\d+) iR=([-+]?\d*\.?\d+)\s*$"
)


@dataclass
class Ack:
    n: int
    v: float
    w: float
    mL: float
    mR: float
    oL: float
    oR: float
    iL: float
    iR: float


def parse_ack(line):
    m = _ACK_RE.match(line)
    if not m:
        return None
    n, v, w, mL, mR, oL, oR, iL, iR = m.groups()
    return Ack(int(n), float(v), float(w), float(mL), float(mR),
               float(oL), float(oR), float(iL), float(iR))


class TelemetryRing:
    def __init__(self, maxlen=TELEMETRY_LINES):
        self._items = deque(maxlen=maxlen)

    def push(self, ack):
        self._items.appendleft(ack)

    def clear(self):
        self._items.clear()

    def items(self):
        return list(self._items)


# ---------------------------------------------------------------------------
# Рендер панели (ANSI, alternate screen buffer). Чистые функции: без ввода-
# вывода, тестируются без TTY.
# ---------------------------------------------------------------------------
_ESC = "\x1b"
_TABLE_HEADER = "  #      v      w     mL     mR     oL     oR     iL     iR"


def screen_on():
    sys.stdout.write("{}[?1049h{}[?25l".format(_ESC, _ESC))
    sys.stdout.flush()


def screen_off():
    sys.stdout.write("{}[?25h{}[?1049l".format(_ESC, _ESC))
    sys.stdout.flush()


def terminal_cols():
    try:
        cols = shutil.get_terminal_size(fallback=(80, 24)).columns
    except Exception:
        cols = 0
    return cols if cols > 0 else 80


def crop(line, cols):
    if cols < 0:
        cols = 0
    line = line[:cols]
    return line + " " * (cols - len(line))


def invert_line(line):
    return "{esc}[7m{line}{esc}[0m".format(esc=_ESC, line=line)


def format_row(ack):
    return "{:>4d} {:>6.1f} {:>6.2f} {:>6.1f} {:>6.1f} {:>6.1f} {:>6.1f} {:>6.1f} {:>6.1f}".format(
        ack.n, ack.v, ack.w, ack.mL, ack.mR, ack.oL, ack.oR, ack.iL, ack.iR)


def render_panel(status, rows, footer, cols=80):
    """Статус + таблица последних ack (свежие сверху) + футер.

    Возвращает строку с ESC[H у начала; финал ESC[J затирает остатки
    предыдущей отрисовки. rows — итерация ack, индекс 0 = самый свежий.
    """
    lines = [crop(status, cols), ""]
    lines.append(_TABLE_HEADER)
    data_rows = 0
    first = True
    for ack in rows:
        row = crop(format_row(ack), cols)
        if first:
            row = invert_line(row)
            first = False
        lines.append(row)
        data_rows += 1
    if data_rows == 0:
        lines.append(crop("(нет ack — ждём телеметрию)", cols))
        data_rows = 1
    for _ in range(max(0, TELEMETRY_LINES - data_rows)):
        lines.append(crop("", cols))
    lines.append("")
    for f in footer.split("\n"):
        lines.append(crop(f, cols))
    body = "\r\n".join(lines)
    return "{}[H".format(_ESC) + body + "{}[J".format(_ESC)


def send_stop():
    global serial_fd
    if serial_fd is not None:
        try:
            write_all(serial_fd, STOP_COMMAND.encode())
        except OSError:
            pass


def shutdown():
    send_stop()
    global serial_fd
    if serial_fd is not None:
        try:
            os.close(serial_fd)
        except OSError:
            pass
        serial_fd = None


def open_serial(port, probe=STOP_COMMAND):
    global serial_fd
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        probe_bytes = probe.encode()
        end = time.monotonic() + LINK_TIMEOUT_S
        while True:
            try:
                os.write(fd, probe_bytes)
                break
            except BlockingIOError:
                select.select([], [fd], [], 0.2)
            except OSError as exc:
                if exc.errno not in (errno.EAGAIN, errno.EWOULDBLOCK):
                    raise
                select.select([], [fd], [], 0.2)
            if time.monotonic() >= end:
                os.close(fd)
                raise TimeoutError(port)

        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        attrs = termios.tcgetattr(fd)
        attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK
                      | termios.ISTRIP | termios.INLCR | termios.IGNCR
                      | termios.ICRNL | termios.IXON)
        attrs[1] = 0
        attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
        attrs[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD
        attrs[3] = 0
        attrs[4] = termios.B9600
        attrs[5] = termios.B9600
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
    except Exception:
        if serial_fd is None:
            try:
                os.close(fd)
            except OSError:
                pass
        raise
    serial_fd = fd
    return fd


def read_key(fd):
    data = os.read(fd, 1)
    if data == b"\x1b":
        rest = b""
        while len(rest) < 2:
            r, _, _ = select.select([sys.stdin], [], [], 0.05)
            if not r:
                break
            chunk = os.read(fd, 1)
            if not chunk:
                break
            rest += chunk
        return data + rest
    return data


def clamp(value):
    return max(CLAMP_MIN, min(CLAMP_MAX, value))


FOOTER_HINTS = "lin: ↑/↓ · ang: ←/→ · Space — пауза · q — выход"


def draw_panel(ring, linear_pct, angular_pct, footer):
    sys.stdout.write(render_panel(status_line(linear_pct, angular_pct),
                                  ring.items(), footer, terminal_cols()))
    sys.stdout.flush()


def run(fd, state, ring):
    paused = state["paused"]
    last_sent = 0.0
    rx = b""
    draw_panel(ring, state["linear"], state["angular"], FOOTER_HINTS)
    while True:
        now = time.monotonic()
        out_linear, out_angular = outgoing_speed(state)
        if now - last_sent >= HEARTBEAT_MS / 1000.0:
            send_command(fd, out_linear, out_angular)
            last_sent = now

        timeout = max(0.0, last_sent + HEARTBEAT_MS / 1000.0 - now)
        r, _, _ = select.select([sys.stdin, fd], [], [], timeout)
        now = time.monotonic()

        if fd in r:
            new_acks = False
            try:
                while True:
                    data = os.read(fd, 4096)
                    if not data:
                        break
                    if len(rx) + len(data) > MAX_RX_BUFFER:
                        rx = rx[-(MAX_RX_BUFFER // 4):]
                    rx += data
                    while b"\n" in rx:
                        line, rx = rx.split(b"\n", 1)
                        ack = parse_ack(line.decode(errors="replace").strip())
                        if ack is not None:
                            ring.push(ack)
                            new_acks = True
            except BlockingIOError:
                pass
            except OSError as exc:
                raise LinkLost() from exc
            if new_acks and not paused:
                draw_panel(ring, state["linear"], state["angular"], FOOTER_HINTS)

        if sys.stdin in r:
            key = read_key(sys.stdin.fileno())
            if key == SPACE:
                paused = not paused
                state["paused"] = paused
                if paused:
                    send_command(fd, 0.0, 0.0)
                else:
                    send_command(fd, state["linear"], state["angular"])
                    draw_panel(ring, state["linear"], state["angular"], FOOTER_HINTS)
                last_sent = time.monotonic()
            elif key in (b"q", b"Q", b"\x03"):
                # \x03 = Ctrl+C: в raw-режиме терминала ISIG отключён, поэтому
                # Ctrl+C приходит как обычный байт 0x03, а не как SIGINT.
                return
            elif not paused:
                changed = True
                if key in KEY_UP:
                    state["linear"] = clamp(state["linear"] + STEP_PCT)
                elif key in KEY_DOWN:
                    state["linear"] = clamp(state["linear"] - STEP_PCT)
                elif key in KEY_LEFT:
                    state["angular"] = clamp(state["angular"] + STEP_PCT)
                elif key in KEY_RIGHT:
                    state["angular"] = clamp(state["angular"] - STEP_PCT)
                else:
                    changed = False
                if changed:
                    send_command(fd, state["linear"], state["angular"])
                    last_sent = time.monotonic()
                    draw_panel(ring, state["linear"], state["angular"], FOOTER_HINTS)


def main():
    global serial_fd
    parser = argparse.ArgumentParser(
        description="Консольный пульт управления ботом через HC-06")
    parser.add_argument("port", help="последовательный порт, например /dev/rfcomm0")
    args = parser.parse_args()

    if not sys.stdin.isatty():
        sys.stderr.write("Ошибка: требуется интерактивный терминал (stdin не TTY)\n")
        return 1

    old_attrs = termios.tcgetattr(sys.stdin.fileno())
    state = {"linear": 0.0, "angular": 0.0, "paused": False}
    ring = TelemetryRing()
    try:
        tty.setraw(sys.stdin.fileno())
        screen_on()

        connected = False
        while True:
            if not connected:
                draw_panel(ring, state["linear"], state["angular"],
                           "Подключение к {} ...".format(args.port))
                try:
                    fd = open_serial(args.port, probe=format_command(*outgoing_speed(state)))
                except TimeoutError:
                    draw_panel(ring, state["linear"], state["angular"],
                               "Ошибка: нет связи с ботом через {}.\n"
                               "Проверь:\n"
                               "  - бот включён и HC-06 спарен с ПК (PIN 1234);\n"
                               "  - канал привязан: sudo rfcomm bind 0 <MAC> 1".format(args.port))
                    time.sleep(1.2)
                    return 1
                except OSError as exc:
                    draw_panel(ring, state["linear"], state["angular"],
                               "Ошибка: не удалось открыть порт {}: {}".format(args.port, exc))
                    time.sleep(1.2)
                    return 1
                connected = True

            draw_panel(ring, state["linear"], state["angular"],
                       "Связь есть. Ожидание команд...")
            try:
                run(fd, state, ring)
                return 0
            except LinkLost:
                draw_panel(ring, state["linear"], state["angular"],
                           "Связь потеряна, переподключение...")
                shutdown()
                connected = False
                reconnected = False
                for _ in range(RECONNECT_ATTEMPTS):
                    time.sleep(RECONNECT_DELAY_S)
                    try:
                        fd = open_serial(args.port, probe=format_command(*outgoing_speed(state)))
                        reconnected = True
                        break
                    except (TimeoutError, OSError):
                        pass
                if reconnected:
                    # Новое соединение — мёртвую телеметрию старой линии
                    # не смешиваем со свежей.
                    connected = True
                    ring.clear()
                    continue
                draw_panel(ring, state["linear"], state["angular"],
                           "Не удалось восстановить связь за {} попыток.\n"
                           "Типично при просадке 5V во время работы моторов: HC-06\n"
                           "должен питаться от отдельного 5V и не проваливаться при токе L298N.\n"
                           "Поднять связь заново: bluetoothctl connect <MAC>".format(RECONNECT_ATTEMPTS))
                time.sleep(1.2)
                return 1
    except KeyboardInterrupt:
        send_stop()
    finally:
        screen_off()
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_attrs)
        shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())