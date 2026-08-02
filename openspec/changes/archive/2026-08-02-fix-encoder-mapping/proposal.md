## Why

Bench testing revealed the robot turns sharply instead of driving straight: the right wheel spins fast while the left barely turns. The firmware, the wiring documentation, and the physical wiring disagree on which encoder belongs to which wheel — the firmware reads the *left* encoder from pins D8/D9 and the *right* from D10/D11, but physically the left motor's encoder is connected to D10/D11 and the right motor's to D8/D9. This crosses the PID feedback: each wheel's controller chases the other wheel's measured speed, which amplifies any small drive asymmetry into a violent turn. The root cause is a documentation/code mapping error, not a PID or motor fault.

## What Changes

- Correct the encoder pin mapping in the firmware so the left PID reads the left wheel's encoder and the right PID reads the right wheel's encoder:
  - `PIN_ENC_L_A/B` become D10/D11, `PIN_ENC_R_A/B` become D8/D9.
  - The `PCINT0_vect` ISR feeds bits `(pins >> 2) & 0x03` (D10/D11) to the left wheel and `pins & 0x03` (D8/D9) to the right wheel.
- Fix the wiring reference in `docs/4. Блок управления приводом/spec.md` to state: left motor encoder D10/D11, right motor encoder D8/D9.
- Verify on the bench that each encoder's direction sign relative to its motor matches the firmware expectation (a mismatched sign makes the wheel run away at full speed in the wrong direction) and invert `motorSign` per wheel if needed.

## Capabilities

### New Capabilities
- `encoder-mapping`: The controller SHALL associate each PID feedback channel with the physically correct wheel encoder, and the mapping SHALL be documented consistently between firmware, wiring reference, and hardware.

### Modified Capabilities
<!-- Existing capabilities whose REQUIREMENTS are changing (not just implementation).
     Only list here if spec-level behavior changes. Each needs a delta spec file.
     Use existing spec names from openspec/specs/. Leave empty if no requirement changes. -->
- (none yet — `motion-control` and `motor-braking` are defined only inside the in-progress `motor-controller-firmware` change; the encoder-mapping requirement is captured here as a new capability)

## Impact

- `src/motor_controller/motor_controller_firmware/motor_controller_firmware.ino` — encoder pin constants and PCINT0 ISR mapping.
- `docs/4. Блок управления приводом/spec.md` — encoder wiring reference (left D10/D11, right D8/D9).
- Bench setup: encoder connectors physically stay as they are; no rewiring required once firmware and docs are corrected.
- No changes to kinematics, PID gains, ramp, or braking logic.
