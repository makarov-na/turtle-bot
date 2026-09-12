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
    q     — выход (перед выходом боту уходит set_speed 0.0 0.0)

Текущая команда повторяется каждые 200 мс (heartbeat, питает watchdog
прошивки). Используется только стандартная библиотека Python.
"""

import argparse
import errno
import fcntl
import os
import select
import sys
import termios
import time
import tty

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

STOP_COMMAND = "set_speed 0.0 0.0\n"

RECONNECT_ATTEMPTS = 5
RECONNECT_DELAY_S = 0.5

serial_fd = None


def out(msg):
    sys.stdout.write("\r\n" + msg + "\r\n")
    sys.stdout.flush()


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


class LinkLost(Exception):
    pass


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


def status_line(linear_pct, angular_pct):
    return "\rlin {:+.0f}%  ang {:+.0f}%   ".format(linear_pct, angular_pct)


def run(fd, state):
    linear_pct = state["linear"]
    angular_pct = state["angular"]
    last_sent = 0.0
    while True:
        now = time.monotonic()
        if now - last_sent >= HEARTBEAT_MS / 1000.0:
            send_command(fd, linear_pct, angular_pct)
            last_sent = now

        sys.stdout.write(status_line(linear_pct, angular_pct))
        sys.stdout.flush()

        timeout = max(0.0, last_sent + HEARTBEAT_MS / 1000.0 - now)
        r, _, _ = select.select([sys.stdin, fd], [], [], timeout)
        if fd in r:
            # Keepalive-телеметрия бота ("ack ...") приходит на каждый наш
            # пинг. Дренируем вход: иначе tty-буфер заполнится и упрётся
            # во флоу-контроль канала. Содержимое пока не разбираем.
            try:
                while True:
                    data = os.read(fd, 4096)
                    if not data:
                        break
            except BlockingIOError:
                pass
            except OSError as exc:
                raise LinkLost() from exc
        if sys.stdin in r:
            key = read_key(sys.stdin.fileno())
            changed = True
            if key in KEY_UP:
                linear_pct = clamp(linear_pct + STEP_PCT)
            elif key in KEY_DOWN:
                linear_pct = clamp(linear_pct - STEP_PCT)
            elif key in KEY_LEFT:
                angular_pct = clamp(angular_pct + STEP_PCT)
            elif key in KEY_RIGHT:
                angular_pct = clamp(angular_pct - STEP_PCT)
            elif key in (b"q", b"Q"):
                state["linear"] = linear_pct
                state["angular"] = angular_pct
                return
            else:
                changed = False
            if changed:
                state["linear"] = linear_pct
                state["angular"] = angular_pct
                send_command(fd, linear_pct, angular_pct)
                last_sent = time.monotonic()


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
    state = {"linear": 0.0, "angular": 0.0}
    try:
        tty.setraw(sys.stdin.fileno())

        out("Стрелки: Up/Down — скорость, Left/Right — поворот, q — выход")

        while True:
            out("Подключение к {} ...".format(args.port))
            try:
                fd = open_serial(args.port, probe=format_command(state["linear"], state["angular"]))
            except TimeoutError:
                out("Ошибка: нет связи с ботом через {}. Проверь:".format(args.port))
                out("  - бот включён и HC-06 спарен с ПК (PIN 1234);")
                out("  - канал привязан: sudo rfcomm bind 0 <MAC> 1")
                return 1
            except OSError as exc:
                out("Ошибка: не удалось открыть порт {}: {}".format(args.port, exc))
                return 1

            out("Связь есть. Ожидание команд...")
            try:
                run(fd, state)
                return 0
            except LinkLost:
                out("Связь потеряна, переподключение...")
                shutdown()
                reconnected = False
                for _ in range(RECONNECT_ATTEMPTS):
                    time.sleep(RECONNECT_DELAY_S)
                    try:
                        fd = open_serial(args.port, probe=format_command(state["linear"], state["angular"]))
                        reconnected = True
                        break
                    except (TimeoutError, OSError):
                        pass
                if not reconnected:
                    out("Не удалось восстановить связь за {} попыток.".format(RECONNECT_ATTEMPTS))
                    out("Типично при просадке 5V во время работы моторов: HC-06 "
                        "должен питаться от отдельного 5V и не проваливаться при токе L298N.")
                    out("Поднять связь заново: bluetoothctl connect <MAC>")
                    return 1
    except KeyboardInterrupt:
        send_stop()
    finally:
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_attrs)
        shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())