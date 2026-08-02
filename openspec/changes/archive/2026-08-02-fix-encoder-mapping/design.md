## Context

The motor controller firmware (`src/motor_controller/motor_controller_firmware/motor_controller_firmware.ino`) reads encoder ticks in a single `PCINT0_vect` ISR on PORTB bits 0-3 (pins D8-D11). It currently assigns bits 0-1 (D8/D9) to the left wheel and bits 2-3 (D10/D11) to the right wheel. Bench testing shows the robot turns sharply instead of driving straight.

Diagnosis: the physical wiring is the opposite — the left motor's encoder is connected to D10/D11 and the right motor's to D8/D9. The firmware (and the wiring reference in `docs/4. Блок управления приводом/spec.md`) documents the reverse. This crosses the PID feedback: the left PID drives the left motor but reads the right wheel's encoder, and vice versa. A crossed loop converts small drive-side asymmetries into a large wheel-speed difference, matching the observed "right fast, left barely turns".

The physical wiring is treated as ground truth (the user built and verified it); the firmware and docs must be corrected to match.

## Goals / Non-Goals

**Goals:**
- Correct the encoder pin mapping so each PID feedback channel reads its own wheel's encoder.
- Make the wiring reference in `docs/4. Блок управления приводом/spec.md` agree with the physical wiring.
- Provide a bench procedure to confirm each encoder's direction sign matches the firmware convention.
- Restore straight forward motion in the demo.

**Non-Goals:**
- No changes to kinematics, PID gains, ramp, braking, or clipping logic.
- No rewiring of the bench setup (connectors stay where they are).
- No changes to the `motor-controller-firmware` change's existing capabilities; this change is a focused correction.

## Decisions

### 1. Correct the firmware by swapping the encoder pin mapping
Swap `PIN_ENC_L_A/B` to D10/D11 and `PIN_ENC_R_A/B` to D8/D9, and change the ISR so `updateEncoder(WHEEL_LEFT, (pins >> 2) & 0x03)` and `updateEncoder(WHEEL_RIGHT, pins & 0x03)`.

*Alternatives considered:* rewiring the bench to match the docs. Rejected — the physical wiring is already installed and the docs were the source of the error; changing code is a two-line edit and keeps hardware untouched.

*Why chosen:* minimal, hardware-preserving fix; aligns code with verified physical reality.

### 2. Correct the docs wiring reference
Update `docs/4. Блок управления приводом/spec.md` to state: right motor encoder D8/D9, left motor encoder D10/D11 (matching the firmware after fix).

*Why chosen:* the doc is the future source of truth for wiring; leaving it wrong would reintroduce the bug on any rebuild.

### 3. Keep `motorSign` per wheel but verify sign on the bench
The mirror compensation (`motorSign = -1` left, `+1` right) is unchanged. After the mapping fix, each wheel's measured speed sign depends on how the encoder channels (A/B) are wired and how the motor is mounted. If the sign is wrong, the PID saturates and the wheel runs away at full speed in the wrong direction — an unmistakable bench signal.

*Why chosen:* the sign cannot be derived from the docs alone; it must be validated empirically. The existing `motorSign` field is the intended place to invert per-wheel sign, so no structural change is needed.

## Risks / Trade-offs

- [Encoder A/B channels swapped on one wheel] → The wheel spins at full speed in the wrong direction (runaway). Detect on bench; fix by swapping the two encoder wires for that wheel or negating that wheel's `motorSign`.
- [The docs edit diverges from any future archived spec] → `docs/4. ...` is the standalone hardware reference and is authoritative for wiring; the firmware change mirrors it.
- [Physical drive asymmetry (left motor heavier) remains after the fix] → The closed loop should now compensate it since feedback is uncrossed; if a residual turn persists, revisit `v_max`/demo target (out of scope here) rather than re-swapping pins.
- [Swapped pins in ISR bits but forgetting the `PIN_ENC_*` constants] → The constants are only used for `pinMode`/setup; a mismatch is harmless at runtime but confusing. Change both consistently.

## Migration Plan

1. Edit `motor_controller_firmware.ino` (encoder constants + ISR mapping).
2. Edit `docs/4. Блок управления приводом/spec.md` (encoder pin reference).
3. Flash the sketch, run the forward demo, observe straight motion and correct direction (both wheels move the robot forward, no runaway).
4. Rollback: revert the sketch via git; the change is two isolated edits.

## Open Questions

- Exact encoder A/B polarity per wheel — resolved empirically on the bench (may require swapping the two encoder wires or adjusting `motorSign`).
- Whether any residual wheel-speed asymmetry is tolerable or needs a lower demo target — assessed after the mapping fix.
