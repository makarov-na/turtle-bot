## 1. Firmware encoder mapping fix

- [x] 1.1 Swap the encoder pin constants in the sketch: `PIN_ENC_L_A/B` = D10/D11, `PIN_ENC_R_A/B` = D8/D9
- [x] 1.2 Update the `PCINT0_vect` ISR: feed `(pins >> 2) & 0x03` (D10/D11) to the left wheel and `pins & 0x03` (D8/D9) to the right wheel
- [x] 1.3 Keep `pinMode` setup and `PCMSK0` mask consistent with the corrected mapping (all four of D8-D11 still INPUT_PULLUP and masked)

## 2. Documentation fix

- [x] 2.1 Update `docs/4. Блок управления приводом/spec.md` encoder wiring: left motor encoder D10/D11, right motor encoder D8/D9

## 3. Bench verification

- [x] 3.1 Flash the sketch and run the forward demo; verify the robot drives straight (no sharp turn)
- [x] 3.2 Verify wheel direction: both wheels rotate so the robot moves forward, with no runaway at full speed in the wrong direction
- [x] 3.3 If a wheel reads direction inverted, flip that wheel's `motorSign` (or swap its two encoder wires) and re-verify
- [x] 3.4 Confirm `power_stop()` and the demo's timed stop still behave as before the mapping change
