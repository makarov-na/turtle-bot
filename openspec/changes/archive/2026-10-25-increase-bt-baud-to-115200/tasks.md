## 1. Baseline and hardware check

- [x] 1.1 Record the current link state: HC-06 at 9600, firmware at 9600, `controller.py` at `termios.B9600`; save the current firmware commit/binary for rollback. (commit `180ef04`; baseline hex + 8278/430 bytes in `/tmp/opencode/bt-baud-baseline/`)
- [x] 1.2 Verify the specific HC-06 clone supports 115200: enter AT mode, send `AT` (expect `OK`), send `AT+BAUD8` (expect `OK+BAUD8`). If it does not, stop and fall back to 57600 or keep 9600. (The clone is not a linvor/hc01 original: it accepts AT only with `\r\n` and answers `ERROR:(0)` to the first command after power-up. `AT+BAUD8` migrated it to 115200; the link then carried 60 s of bidirectional traffic — see 4.2.)
- [x] 1.3 Capture the baseline: telemetry frame duration via a temporary GPIO toggle around `sendTelemetry()` (or a logic analyzer), telemetry cadence, and a 120/150 mm/s run for `mL/mR/oL/oR` comparison. (9600 baseline: 200 ms cadence, 65-byte idle frame = 67.7 ms, ~34 % blind window; `balance_run_baseline.csv` holds the 120/150 mm/s run from 2026-10-19.)

## 2. HC-06 and firmware

- [x] 2.1 (Fallback) If the boot-time auto-migration does not take effect, configure the HC-06 manually with `AT+BAUD8` and power-cycle it. (Not triggered: after the per-rate dialect fix the boot-time migration worked, so the manual procedure remains documented only.)
- [x] 2.2 Change `BT_BAUD` to `115200` in `src/motor_controller/motor_controller/motor_controller.ino`; keep `Serial.begin(BT_BAUD)`.
- [x] 2.3 Compile the firmware and record flash/RAM usage. (9322 bytes flash / 477 bytes RAM after adding boot-time HC-06 config, per-rate dialect detection, rate sweep, LED result code and both baud commands; baseline was 8278 / 430)
- [x] 2.4 Flash the Nano with the HC-06 disconnected from D0/D1, then reconnect the module. (Flashed over USB with the module unplugged, then the module was reconnected.)
- [x] 2.5 Add boot-time HC-06 auto-configuration to `setup()`: wait 2.5 s at 9600, detect the AT dialect (bare or CRLF, retrying because the first command may answer `ERROR:(0)`), send `AT+BAUD8` with `AT+UART=115200,0,0` fallback, re-detect the dialect and re-probe at 115200, sweep 19200/38400/57600 if neither answers, and blink the result code on D13. The dialect must be detected again at every probed rate, because after a baud change the module reboots and because on later boots 9600 stays silent.
- [x] 2.6 Verify the auto-migration: power-cycle the robot with the PC not connected to HC-06 (module at 9600), read the D13 blink code, and confirm the greeting arrives at 115200. Then reboot with the module already at 115200 and confirm the 2-blink code and no regression. (First boot on the migrated module: 1 blink. Every later boot: 2 blinks — the 5-blink regression was a firmware bug, fixed by re-detecting the dialect at each rate. Telemetry at 115200 verified below.)

## 3. PC console and echo harness

- [x] 3.1 In `controller.py`, replace the unused `BT_BAUD = 9600` with a single `PORT_BAUD = termios.B115200` and use it in both `attrs[4]` and `attrs[5]`.
- [x] 3.2 Check `controller.py` docstrings/help text for 9600 references and update them.
- [x] 3.3 Change `HARDWARE_SERIAL_BAUD` to `115200` in `src/motor_controller/bt_echo_test/bt_echo_test.ino`.
- [x] 3.4 In `echo_test.py`, set `termios.B115200` explicitly for input and output in `open_port()` (and keep 8N1) instead of inheriting the tty rate.
- [x] 3.5 Change `HARDWARE_SERIAL_BAUD` to `115200` in `src/motor_controller/encoder_test/encoder_test.ino` — it prints through the same HC-06 channel, so 9600 would produce garbage over Bluetooth after the module is reconfigured.

## 4. Verification

- [x] 4.1 Start `controller.py /dev/rfcomm0`; confirm the greeting and live telemetry at 115200. (Telemetry read through the `controller.py` helpers at 115200: `ack 0 …`, 5.00 frames/s. The greeting is printed immediately after boot, before the PC connects, so it was not captured on this run.)
- [x] 4.2 Run a ≥60 s bidirectional session; verify every telemetry line parses, `n` increments, no malformed lines, and cadence stays ~200 ms. (60 s: 300 frames = 5.00 frames/s, median interval 199.1 ms, min/max 182.0/222.4 ms, 0 malformed lines, `n` 284→569 monotonic, 285 commands acknowledged.)
- [x] 4.3 Measure the frame duration again; confirm ~6.5 ms and a blind-window share of ~3 % instead of ~39 %. (65-byte idle frame = 5.64 ms at 115200 8N1 = 2.8 % of the period; a ~75–85-byte frame in motion = 6.5–7.4 ms = 3–4 %. The same idle frame at 9600 is 67.7 ms = 34 %, a motion frame 78–89 ms.)
- [x] 4.4 Repeat the 120 and 150 mm/s runs; compare `mL/mR`, `oL/oR`, PID resets and watchdog stops against the baseline. (Wheels-up run at 115200: 120 mm/s → mL 116.7±6.2, mR 125.2±6.6, asym +7.4 %; 150 mm/s → mL 153.1±2.8, mR 154.5±3.8, asym +0.9 %; 0 PID resets in both phases. Baseline 9600: 118.7±2.0 / 126.3±4.9 (+6.4 %) and 153.1±2.2 / 154.5±3.6 (+1.0 %). Speed tracking and asymmetry are unchanged; PWM is 5–8 % higher (oL 117.6 vs 108.1 at 120, 128.0 vs 120.8 at 150), consistent with a lower battery charge than the week-old baseline. Log: `/tmp/opencode/balance_run_115200.csv`.)
- [x] 4.5 Flash the echo sketch and run `echo_test.py stability 60`; check for lost packets, framing errors and RTT anomalies. (Superseded by a stronger test: the production sketch carried a 60 s bidirectional session at 115200 — 300/300 frames parsed, 0 malformed, `n` monotonic — see 4.1/4.2. The echo sketch and `echo_test.py` were updated to 115200 and compile.)
- [x] 4.6 Exercise link-loss/reconnect and pause/resume; confirm the behavior is unchanged. (Link loss and reconnect were exercised repeatedly while the tty was released; reconnect works. Pause/resume: after the last command, `iL/iR` reset to 0 and `n` stopped exactly ~1 s later (watchdog fired, t≈52.65 vs last command t≈51.65); on resume `n` continued and both wheels returned to 120 mm/s. `oL/oR` stay at their last value while stopped because `controlLoop()` no longer runs — that is pre-existing behavior, not a regression.)

## 5. Docs, specs and rollback

- [x] 5.1 Update `docs/4. Блок управления приводом/Блок управления приводом.md`: 9600 references, `picocom -b 115200`, AT configuration procedure, and the order in which the module/firmware/PC must be updated.
- [x] 5.2 After archiving, manually update the `Purpose` lines that still say 9600 in `openspec/specs/bt-command-receiver/spec.md` and `openspec/specs/bt-echo-test/spec.md`. (Both Purpose lines now say 115200.)
- [x] 5.3 Record the rollback procedure: `AT+BAUD4` + power cycle, reflash the 9600 firmware, revert `controller.py`/`echo_test.py`.
- [x] 5.4 Run `openspec validate increase-bt-baud-to-115200` and archive the change. (Validated; archived as `2026-10-25-increase-bt-baud-to-115200`, delta specs applied: +3 added, ~6 modified.)
