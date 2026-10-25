## Why

The HC-06 link currently runs at 9600 baud. A telemetry frame of ~75 bytes takes about 78 ms to transmit, and `Serial.print` is blocking, so the control loop is blind for roughly 39 % of every 200 ms period. The fixes in `decouple-telemetry-from-commands` and `balance-wheel-speeds` made this safe, but it remains the main structural limit on control quality: during the frame the PWM is held and disturbances are not corrected.

The 9600 rate was chosen during the initial bring-up, not because the hardware tops out there. The HC-06, the ATmega328P UART and the existing level shifter/divider support 115200. Raising the link speed cuts the frame to ~6.5 ms (about 3 % of the period) without changing the protocol, frame format, control loop or wiring.

## What Changes

- **BREAKING (operational):** switch the HC-06 UART to 115200. The module keeps the setting across power cycles. The firmware migrates it automatically on the first boot when the module is in AT mode (not connected over Bluetooth); the manual `AT+BAUD8` procedure remains the fallback. The firmware and the PC console must still be updated together; a mixed configuration will not communicate.
- Firmware: `BT_BAUD` changes from 9600 to 115200; `Serial.begin(BT_BAUD)` uses the new rate. At boot the sketch waits 2.5 s at 9600, detects the module's AT dialect (bare for a genuine HC-06, `\r\n` for the installed clone), sends the baud-change command (`AT+BAUD8`, falling back to `AT+UART=115200,0,0`) and verifies 115200. No changes to the command syntax, telemetry frame format, PID, kinematics or watchdog.
- PC console: `controller.py` opens the port at 115200 instead of the hardcoded `termios.B9600`. The currently unused `BT_BAUD` constant is either used as the single source of truth or removed.
- Echo test: `bt_echo_test.ino` and the diagnostic `encoder_test.ino` run at 115200; `echo_test.py` sets the port speed explicitly instead of inheriting whatever the tty currently has.
- Docs: update the 9600 references and `picocom -b 9600` examples.
- Verification: measure the telemetry frame duration and the control-loop gap before/after; confirm the blind window drops from ~78 ms to ~6.5 ms, with no byte loss or framing errors over a ≥60 s bidirectional run.

## Capabilities

### New Capabilities

<!-- нет -->

### Modified Capabilities

- `bluetooth-telemetry`: greeting, hardware-serial and periodic-frame requirements move from 9600 to 115200; add a requirement that the HC-06 module UART is configured to the same rate as the firmware.
- `bt-command-receiver`: command reception over Hardware Serial moves from 9600 to 115200; parsing and command syntax are unchanged.
- `pc-keyboard-control`: the console connects at 115200 instead of 9600; dashboard, keys and reconnect behavior are unchanged.
- `bt-echo-test`: the echo sketch and its verification procedure use 115200; the Python harness sets the baud explicitly.

## Impact

- `src/motor_controller/motor_controller/motor_controller.ino` — `BT_BAUD`, `Serial.begin`.
- `src/motor_controller/motor_controller/controller.py` — `termios.B115200`; dead `BT_BAUD` constant.
- `src/motor_controller/bt_echo_test/bt_echo_test.ino` — hardware serial baud.
- `src/motor_controller/encoder_test/encoder_test.ino` — hardware serial baud (diagnostic sketch on the same HC-06 channel; otherwise it would print garbage over Bluetooth after the module moves to 115200).
- `src/motor_controller/bt_echo_test/echo_test.py` — explicit `termios.B115200`.
- `openspec/specs/bluetooth-telemetry/spec.md`, `bt-command-receiver/spec.md`, `pc-keyboard-control/spec.md`, `bt-echo-test/spec.md` — delta specs in this change.
- `docs/4. Блок управления приводом/Блок управления приводом.md` — 9600 references, `picocom -b`.
- Hardware: HC-06 AT configuration only; no wiring changes. Reflashing the Nano still requires disconnecting the HC-06 from D0/D1.

## Risks

- The ATmega328P at 16 MHz generates 115200 with about +2.1 % error. This is normally within UART tolerance, but the HC-06 clock and the level shifter add their own error. If framing errors appear, fall back to 57600 or 38400.
- 230400 is not the next safe step: the same UART divisor gives about −3.5 % error, and 460800 is far worse (~+8.5 %). 115200 is the reliable maximum for this MCU.
- The change does not reduce the 200 ms telemetry period; it only shortens the blocking transmission inside it. The keepalive and RFCOMM stability behavior stay as verified in `bt-echo-test`.
- A mixed configuration (HC-06 at 115200, firmware at 9600, or vice versa) looks like a dead link. The migration plan and docs must make the order explicit.
- AT commands are accepted only while the module has no active Bluetooth connection. If the PC connects during the first boot, the auto-migration is skipped and the link stays dead until the robot is power-cycled with the module unconnected (or the manual `AT+BAUD8` procedure is used).
