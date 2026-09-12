#!/usr/bin/env python3
# Эхо-тест Bluetooth-канала (HC-06) для скетча bt_echo_test.ino.
#
# Проверяет связь ПК <-> HC-06 <-> Nano: шлёт пинги каждые 200 мс и ждёт
# ответов "echo <n>: ping <k>" от бота, измеряя RTT и ловя разрывы линка.
#
# Использование:
#   python3 echo_test.py rtt 50             L2: 50 пингов каждые 200 мс
#   python3 echo_test.py stability 60       L3: 60 с непрерывных пингов
#   python3 echo_test.py rtt 50 /dev/ttyUSB0   уточнить порт
#
# Скрипт открывает порт в raw-режиме и сам отключает локальный termios-echo:
# без этого терминал ПК возвращал бы введённую строку без участия бота
# (ложное эхо). Ответы считаются только для строк, начинающихся с "echo ".

import argparse
import os
import select
import sys
import termios
import time

PORT_DEFAULT = "/dev/rfcomm0"
PING_PERIOD_S = 0.2
NO_REPLY_DEAD_S = 3.0  # нет ответа дольше этого -> линк считаем оборванным


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] &= ~(termios.BRKINT | termios.ISTRIP | termios.INLCR
                  | termios.IGNCR | termios.ICRNL | termios.IXON)
    attrs[1] &= ~termios.OPOST
    attrs[2] &= ~(termios.CSIZE | termios.PARENB)
    attrs[2] |= termios.CS8
    attrs[3] &= ~(termios.ECHO | termios.ICANON | termios.ISIG)
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_available(fd, pending=b""):
    pending += os.read(fd, 4096)
    lines = pending.split(b"\n")
    return lines[:-1], lines[-1]


def parse_reply(line):
    text = line.decode(errors="replace").strip()
    if not text.startswith("echo "):
        return None
    parts = text.split(": ")
    if len(parts) != 2 or not parts[0][5:].isdigit():
        return None
    return int(parts[0][5:]), parts[1]


def now_ms():
    return time.monotonic() * 1000.0


def main():
    ap = argparse.ArgumentParser(description="Эхо-тест BT-канала HC-06")
    ap.add_argument("mode", choices=["rtt", "stability"],
                    help="rtt: N пингов с замером RTT; stability: T секунд")
    ap.add_argument("arg", type=float)
    ap.add_argument("port", nargs="?", default=PORT_DEFAULT)
    args = ap.parse_args()

    print(f"Канал: {args.port}, режим {args.mode}, период {PING_PERIOD_S:.1f} с")
    try:
        fd = open_port(args.port)
    except OSError as exc:
        print(f"Ошибка: не удалось открыть {args.port}: {exc}")
        print("Проверь привязку: sudo rfcomm bind 0 00:21:13:00:0C:E3 1")
        return 1

    sent = 0
    received = 0
    send_times = {}
    pending = b""
    rtts = []
    first_break_s = None
    last_reply_ms = now_ms()
    start_ms = now_ms()
    finish_ms = start_ms + (args.arg * 1000.0)
    broke = False

    while True:
        now = now_ms()
        if args.mode == "rtt" and sent >= int(args.arg):
            break
        if args.mode == "stability" and now >= finish_ms:
            break

        if now - (send_times.get(sent) or 0) >= PING_PERIOD_S * 1000.0:
            sent += 1
            line = f"ping {sent}\n".encode()
            try:
                os.write(fd, line)
                send_times[sent] = now
            except OSError as exc:
                first_break_s = (now - start_ms) / 1000.0
                broke = True
                print(f"[{first_break_s:.2f}s] обрыв на записи: {exc}")
                break

        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                chunks = read_available(fd, pending)
            except OSError as exc:
                first_break_s = (now - start_ms) / 1000.0
                broke = True
                print(f"[{first_break_s:.2f}s] обрыв на чтении: {exc}")
                break
            pending = chunks[1]
            for raw in chunks[0]:
                parsed = parse_reply(raw)
                if parsed is None:
                    continue
                n, body = parsed
                if body.startswith("ping "):
                    try:
                        k = int(body[5:])
                    except ValueError:
                        continue
                elif body == "test ready":
                    continue
                else:
                    continue
                rtt = now - send_times.get(k, now)
                received += 1
                rtts.append(rtt)
                last_reply_ms = now

        if now - last_reply_ms >= NO_REPLY_DEAD_S * 1000.0:
            first_break_s = (now - start_ms) / 1000.0
            broke = True
            print(f"[{first_break_s:.2f}s] нет ответов {NO_REPLY_DEAD_S:.0f} с — линк оборван")
            break

    os.close(fd)

    dur = (time.monotonic() * 1000.0 - start_ms) / 1000.0
    lost = sent - received
    print("---")
    print(f"отправлено: {sent}, принято: {received}, потеряно: {lost}")
    if rtts:
        print(f"RTT: средний {sum(rtts)/len(rtts):.0f} мс, "
              f"мин {min(rtts):.0f} мс, макс {max(rtts):.0f} мс")
    print(f"длительность: {dur:.1f} с, первый обрыв: "
          f"{first_break_s or 'нет'}")
    if broke:
        print("ИТОГ: ОБРЫВ ЛИНКА")
        return 2
    if dur >= 60.0:
        print("ИТОГ: линк прожил ≥60 с при двунаправленном трафике")
    print("ИТОГ: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())