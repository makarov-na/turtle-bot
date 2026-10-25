## Context

The HC-06 link currently runs at 9600 baud on Hardware Serial D0/D1. The periodic telemetry frame is about 75 bytes, so at 9600 8N1 it occupies the UART for

```
75 bytes × 10 bits/byte ÷ 9600 bit/s ≈ 78 ms
```

`Serial.print` is blocking, so `controlLoop()` cannot run during that time. With a 200 ms telemetry period, the control loop is blind for about 39 % of wall time; the PWM is held and disturbances are corrected only after the frame finishes. The existing `motion-control` requirement already says a long telemetry iteration must not be mistaken for a pause, so the behavior is safe, but the blind window is still the main structural limit on control quality.

The bottleneck is the UART between the HC-06 and the Arduino, not the Bluetooth radio. The PC-side `/dev/rfcomm0` is a virtual tty: its `speed 9600` setting is nominal and does not determine the RFCOMM throughput. The HC-06's own UART rate is a persistent module setting; the firmware and the module must agree.

The ATmega328P runs at 16 MHz. With the Arduino core's U2X divisor, 115200 is generated as ~117647 baud (+2.1 % error). 230400 becomes ~222222 baud (−3.5 %), and 460800 becomes ~500000 baud (+8.5 %). 115200 is the highest standard rate with a comfortable UART error. The HC-06 datasheet/AT set allows up to 1382400, but the MCU, not the module, is the limiting factor here.

## Goals / Non-Goals

**Goals:**

- Run the HC-06 UART, the firmware and the PC tools at 115200 8N1.
- Reduce the blocking telemetry transmission from ~78 ms to ~6.5 ms, i.e. from ~39 % to ~3 % of the 200 ms period.
- Keep the wire protocol, telemetry frame format, command syntax, PID, kinematics, watchdog and 200 ms period unchanged.
- Make the baud rate a single documented value per side, with an explicit HC-06 configuration procedure.
- Verify no byte loss or framing errors over a ≥60 s bidirectional run, and confirm the control behavior does not regress.

**Non-Goals:**

- Changing the telemetry period, frame contents or field widths.
- Replacing blocking `Serial.print` with a non-blocking TX ring buffer. That is a separate change; 115200 already removes most of the blind window.
- Raising the rate to 230400/460800. The divisor error is too large for the 16 MHz ATmega328P.
- Changing wiring, the level shifter/divider, the motor driver or the control loop.
- Changing `V_MAX_MM_S`, PID gains or the acceleration behavior.

## Decisions

### 1. 115200, not 230400 or higher

At 16 MHz the standard rates land as follows (Arduino core, U2X enabled):

| Rate | Divisor result | Actual | Error |
|---:|---:|---:|---:|
| 9600 | 103 | 9615 | +0.2 % |
| 57600 | 16 | 58824 | +2.1 % |
| 115200 | 16 | 117647 | +2.1 % |
| 230400 | 8 | 222222 | −3.5 % |
| 460800 | 3 | 500000 | +8.5 % |

115200 is the highest standard rate with the same comfortable error as 57600, and it cuts the frame by 12×. 230400 is only 2× better but eats most of the UART tolerance; combined with the HC-06 clock and the level shifter it is not worth the risk. 460800 is not usable at all.

*Alternative:* 57600. Safer, but only halves the frame to ~39 ms and leaves ~20 % blind time; not enough to justify the migration.

### 2. Configure the HC-06 with `AT+BAUD8`; the firmware does it automatically at boot

The real change is the HC-06 UART rate. The module keeps it across power cycles, so it has to be switched once. Two paths:

**Primary — boot-time auto-configuration.** `setup()` opens Hardware Serial at 9600 and waits 2.5 s for the module to boot into AT mode. It then probes `AT` up to three times (300 ms each) to confirm that the module is listening, and afterwards sends `AT+BAUD8` (`OK+BAUD8`) **unconditionally** — a missed or late `OK` must not prevent the migration. Finally it reopens Hardware Serial at 115200, waits 100 ms and probes `AT` again. If neither 9600 nor 115200 answers, the firmware sweeps 19200/38400/57600, and at the first rate that answers it sends `AT+BAUD8`. The normal sequence adds ~3.6–4 s to boot; the sweep only runs when the module was left at an intermediate rate and can add ~2 s more. The probe only succeeds in AT mode, i.e. when the module has no active Bluetooth connection; that is exactly the state at robot power-up before the PC connects.

**Module dialect (the reason the first attempts failed).** The installed module is an HC-06 clone, not a linvor/hc01 original. A raw capture shows: bare `AT` gets no reply, while `AT\r\n` gets `ERROR:(0)\r\n` on the first command after power-up and `OK\r\n` on subsequent ones. Genuine HC-06 modules accept bare commands, so the firmware auto-detects the dialect: it first tries `AT` without a terminator, then `AT\r\n` up to three times, and uses the winning dialect for the commands at that rate. The baud change is `AT+BAUD8`, with `AT+UART=115200,0,0` as a fallback for HC-05-compatible clones. The manual procedure must match the module: this clone needs “Both NL & CR” in the terminal.

**Dialect detection runs again at every probed rate (the reason every boot after the first gave 5 blinks).** The first build detected the dialect only at 9600. Once the module had been migrated, 9600 stayed silent, and the 115200 probe still sent *bare* `AT` — which this clone ignores — so the firmware concluded “no AT answer anywhere” and blinked 5. Because a baud change also reboots the module (its first command may again answer `ERROR:(0)`), `btUseCrlf` is reset and the dialect is re-detected at 9600, at 115200 and at each intermediate rate before any command is sent.

**Fallback — manual AT procedure** (same as before), for the case where the module is connected during the first boot or the clone does not support the auto sequence:

1. Power the HC-06 without pairing it (AT mode), open the serial line at the current rate (9600).
2. Send `AT`, expect `OK`.
3. Send `AT+BAUD8`, expect `OK+BAUD8`.
4. Power-cycle the module. The setting is persistent.
5. After this, the module talks at 115200 to the Arduino.

The boot-time path writes a few bytes to the module on every start. If the module is already in data mode (PC connected), those bytes are forwarded over the air; the console ignores anything that is not an `ack`, so this is harmless. The unconditional `AT+BAUD8` also covers the case where the module's reply to `AT` is lost: the command still arrives. If the module is still at 9600 and was connected during boot, the migration is skipped and the link is dead until the next unconnected power-up — this is documented as the main caveat.

Because Bluetooth cannot be used to inspect a failed migration, `setup()` reports the outcome with a blink code on the built-in LED: 1 = answered at 9600 and migrated, 2 = already at 115200, 3 = answered at an intermediate rate and migrated, 4 = no answer anywhere. This makes a wrong sketch (no blinks at all) and an unreachable AT mode (four blinks) distinguishable without a serial console.

*Alternative:* manual AT only. Safe but needs a USB-UART adapter or a temporary bridge sketch; rejected because it makes a one-line migration unnecessarily hardware-dependent. *Alternative:* leave the HC-06 at 9600 and only raise the firmware/PC side — impossible; the module↔Arduino UART is the bottleneck. *Alternative:* use a Bluetooth module with a faster UART (ESP32/SPP) — a hardware change, out of scope.

The PC's `/dev/rfcomm0` `termios` speed remains nominal, but `controller.py` sets `B115200` for consistency and to avoid a misleading configuration. The actual RFCOMM throughput is unchanged.

### 3. One baud value per side, no dead constants

- Firmware: `#define BT_BAUD 115200`; `Serial.begin(BT_BAUD)`.
- `controller.py`: replace the currently unused `BT_BAUD = 9600` with a single `PORT_BAUD = termios.B115200` and use it in both `attrs[4]` and `attrs[5]`. The old constant looked configurable but had no effect; removing it prevents a future “changed the constant, nothing happened” trap.
- `bt_echo_test.ino`: `HARDWARE_SERIAL_BAUD 115200`.
- `encoder_test.ino`: `HARDWARE_SERIAL_BAUD 115200`. It is a diagnostic sketch, but it prints through the same HC-06 channel, so leaving it at 9600 would make its Bluetooth output unreadable after the module is reconfigured.
- `echo_test.py`: set `termios.B115200` explicitly in `open_port()` instead of inheriting the tty's current setting. The harness is a test tool; an inherited rate would make a 9600 module look like a broken test.

*Alternative:* a shared Python module for both scripts. Not worth it for two small tools; duplication is limited to one termios assignment.

### 4. Verify with a temporary GPIO timing probe, then a 60 s soak

The frame duration is not directly visible from the PC because RFCOMM buffers. For the before/after measurement, use a temporary test build that toggles a free pin (for example D8) around `sendTelemetry()` and measure the pulse with a logic analyzer or scope. The probe is removed before the final firmware.

Data integrity: run the real firmware for ≥60 s at 5 Hz commands, parse every line, and check that `n` increments, no line is malformed, and the telemetry cadence stays at 200 ms. Repeat with `echo_test.py stability 60` after reflashing the echo sketch. Then compare 120 and 150 mm/s runs against the current baseline: `mL/mR`, `oL/oR`, PID resets and watchdog stops should not regress.

### 5. Rollback is an AT command plus a reflash

The HC-06 setting persists across power cycles, so rollback must include `AT+BAUD4` (9600) and a power cycle, plus reverting `BT_BAUD` and the PC/echo-test changes. Keep the old firmware binary or the git commit for a quick reflash.

## Risks / Trade-offs

- **[2.1 % divisor error + HC-06 clock + level shifter]** → framing errors or dropped bytes. Mitigation: 60 s soak; if errors appear, fall back to 57600, then 38400. The level shifter/divider should be checked with a scope at 115200.
- **[Some HC-06 clones do not support `AT+BAUD8`]** → the module may answer `ERROR` or not answer. Mitigation: verify `OK+BAUD8` (or the greeting at 115200); if unsupported, use the highest rate the clone accepts, or keep 9600 and revisit the non-blocking TX buffer instead.
- **[The module's `AT` reply is lost]** → the probe cannot confirm AT mode. Mitigation: `AT+BAUD8` is sent unconditionally, so a module that is listening still migrates; the greeting at 115200 is the real check.
- **[PC connects during the first boot]** → the module is in data mode, the boot-time `AT+BAUD8` is ignored, and a 9600 module stays 9600 while the firmware runs at 115200. Mitigation: power the robot with Bluetooth disconnected; if missed, power-cycle the robot unconnected or use the manual AT procedure. The auto-migration is best-effort, not a substitute for the documented fallback.
- **[Some clones apply the new UART rate only after a power cycle]** → the first boot after flashing may still show no link even though `AT+BAUD8` was accepted. Mitigation: the setting is already persistent, so one more power cycle brings the link up at 115200; the greeting is the check.
- **[Mixed configuration]** → the link looks dead: firmware at 115200, module at 9600, or vice versa. Mitigation: document the order (module first, then firmware, then PC) and keep the startup greeting; a missing greeting is the first diagnostic.
- **[PC `/dev/rfcomm0` speed is virtual]** → changing only the tty speed does nothing. Mitigation: document that the real change is the module's UART; `controller.py` still sets `B115200` for consistency.
- **[Reflash requires disconnecting HC-06 from D0/D1]** → the existing bootloader constraint remains. Mitigation: keep the current procedure in the docs.
- **[Blocking print remains]** → even at 115200, a frame blocks ~6.5 ms. This is acceptable; the non-blocking TX buffer is a separate follow-up if 3 % blind time ever becomes a problem.

## Migration Plan

1. Record the current HC-06 rate (9600).
2. Disconnect the HC-06 from D0/D1, flash the firmware with `BT_BAUD = 115200`, reconnect the module.
3. Power-cycle the robot with the PC **not** connected to HC-06: the firmware waits 2.5 s, probes `AT`, sends `AT+BAUD8` unconditionally and continues at 115200. Boot takes ~4 s.
4. Update `controller.py` (`PORT_BAUD = termios.B115200`), `bt_echo_test.ino`, `encoder_test.ino` and `echo_test.py`.
5. Start `controller.py /dev/rfcomm0`; the greeting and telemetry at 115200 confirm the migration. If they are absent, fall back to the manual `AT`/`AT+BAUD8` procedure and re-test.
6. Run the 60 s soak and the 120/150 mm/s comparison.
7. Run `echo_test.py stability 60` against the echo sketch at 115200.
8. Update the docs and the main specs, then archive the change.

**Rollback:** `AT+BAUD4` on the HC-06 + power cycle; reflash the 9600 firmware; revert `controller.py`/`echo_test.py`.

## Open Questions

- Does the specific HC-06 clone in this build support `AT+BAUD8`? Test before changing the firmware.
- Is the current D1→HC-06 divider clean at 115200, or does it need a scope check / a proper level shifter?
- Once the frame is ~6.5 ms, is the 200 ms telemetry period still the right trade-off, or should a later change shorten it? Out of scope here.
- Is a non-blocking TX ring buffer worth doing as a follow-up, or is ~3 % blind time low enough? Decide after the soak and a floor run.
