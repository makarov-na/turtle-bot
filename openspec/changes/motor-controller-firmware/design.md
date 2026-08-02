## Context

The robot uses a distributed architecture: a single-board computer (RPi5 / Orange Pi 5+) plans high-level motion, and an Arduino-based motor controller executes it. Per the component architecture decision, the two are linked over USB (virtual UART); that integration protocol is the *next* step (step 6 of the general plan). This change delivers the standalone drive module (step 5): firmware for the Arduino Nano + L298N + DC motors with encoders.

Current state: `src/motor_controller/motor_controller/motor_controller.ino` is a demo running a fixed sequence (ramp forward, brake, ramp backward, stop) at identical PWM on both wheels. It has no encoders, no command surface, and its "active brake" reverses polarity (a hard jolt backwards). It is used only as a wiring/direction reference; nothing is ported.

Wiring reference (from `docs/4. Блок управления приводом/spec.md`):
- Right motor: D2-IN1, D3-IN2, D5-ENA; encoder D10, D11
- Left motor: D4-IN3, D7-IN4, D6-ENB; encoder D8, D9

## Goals / Non-Goals

**Goals:**
- Body-frame `set_speed(v, ω)` command surface ready for the future protocol
- Accurate wheel speed via encoder feedback and per-wheel PID
- Smooth motion (acceleration ramp) and safe braking (`power_stop`)
- Curvature-preserving clipping when commands exceed physical limits
- Minimal, wiring-verification demo loop

**Non-Goals:**
- Integration protocol with the single-board computer (step 6)
- Telemetry publishing (encoder data upstream) — deferred per current plan
- Odometry, dead-reckoning, turn-by-angle commands
- Sensor block or any other Arduino controller
- PCB layout / schematic design (tracked separately, KiCad files exist in `docs/4. .../`)

## Decisions

### 1. Body-frame (v, ω) command surface
The module accepts `(v, ω)` at the midpoint of the wheel axis, not per-wheel speeds or raw PWM.

*Alternatives considered:* per-wheel speed commands (leaks the left/right physical mirroring to the caller), raw PWM (no semantics, caller must do all math).

*Why chosen:* mirrors the future protocol and ROS `cmd_vel` style, keeps wiring/mounting specifics inside the controller, and the per-wheel inversion (the left motor is mounted "backward") lives in one mapping layer.

### 2. Closed-loop PID on the controller
Each wheel measures encoder ticks, converts to linear speed, and runs a PID controller to set PWM.

*Alternatives considered:* open-loop PWM mapping — simpler but cannot give the accurate speed/rotation the drive block is intended for (see `docs/4. Блок управления приводом/Блок управления приводом.md`: encoders exist precisely to set real rotation speed).

*Why chosen:* the whole point of having encoders on the module is accurate wheel speed; keeping the loop local also keeps the upstream link low-rate.

### 3. Curvature-preserving clipping (Option A)
When `max(|v_l|, |v_r|) > v_max`, scale both wheel targets by `k = v_max / max(...)`.

*Alternatives considered:* per-wheel clamping (breaks the commanded curvature — the robot silently leaves the trajectory), clamp ω only (preserves forward speed but alters the curve).

*Why chosen:* a scaled command keeps the same curve at a lower pace; autonomous planners already respect limits, so clipping is a safety net, not a working mode.

### 4. L298N dynamic braking for power_stop
`power_stop()` drives both IN lines high with enable active (L298N fast motor stop) for a bounded brake duration, then disables.

*Alternatives considered:* the prototype's reverse-polarity brake (a violent reversal, not a stop).

*Why chosen:* reverse-polarity "braking" is dangerous and wrong for L298N; fast motor stop is the manufacturer-defined braking mode.

### 5. Acceleration ramp on the controller
Targets are ramped from the current speed over a configured duration; PID tracks the ramped target.

*Alternatives considered:* no ramp (instantaneous steps strain the drivetrain and cause jerky motion).

*Why chosen:* smooth motion, consistent with the prototype's intent and good practice for DC drives.

### 6. Parameters as named constants
Wheel diameter, wheelbase, v_max, ramp duration, brake duration, PID gains are compile-time constants in the sketch.

*Alternatives considered:* EEPROM/runtime configuration — no protocol yet to deliver values.

*Why chosen:* keeps the module self-contained until step 6 introduces a configuration channel. Constants are centralized and documented.

### 7. Derived ω_max
`ω_max = 2·v_max / B`, no separate setting.

*Rationale:* pure rotation at `v = 0` gives `|v_l| = |v_r| = ω·B/2`, so a single max-speed setting bounds both linear and angular motion.

### 8. Single pin-change interrupt for all encoders
All four encoder pins (D8–D11) belong to PORTB, so one `PCINT0` vector with a PCMSK0 mask serves both wheels.

*Alternative:* hardware interrupts INT0/INT1 on D2/D3 — not usable, those pins are taken by motor direction lines.

*Why chosen:* Arduino Nano has only two external interrupts; pin-change interrupts on PORTB handle all four encoder channels with one ISR. Debouncing is handled in software (see risks).

## Structure

```
┌──────────────────────────────────────────────┐
│ loop()                                       │
│  ├─ pid_loop(): sample encoders, compute     │
│  │   speed, PID → PWM (periodic, e.g. 10 ms) │
│  ├─ ramp_update(): move ramped target toward │
│  │   commanded (v, ω) target                 │
│  └─ demo_state(): set_speed(0,0) → 10 s →    │
│      stop()                                  │
├──────────────────────────────────────────────┤
│ Wheel (per wheel)                            │
│  pins (ENA, IN1, IN2, encA, encB)           │
│  encoder tick counter, measured speed,       │
│  target speed, PID state                     │
│  set_pwm/dir, brake(), coast()               │
├──────────────────────────────────────────────┤
│ Kinematics                                   │
│  set_speed(v, ω) → (v_l, v_r) with mirror    │
│  → clip(curvature-preserving) → wheel rpm    │
├──────────────────────────────────────────────┤
│ ISR: PCINT0_vect — quadrature decode         │
└──────────────────────────────────────────────┘
```

## Risks / Trade-offs

- **Encoder noise / false ticks** → software debounce in the PCINT ISR (require a minimum tick interval; optionally RC filter on the encoder lines on the schematic).
- **`v_max` set at or above the physical wheel max** leaves no headroom for PID → configure `v_max` below the measured physical maximum.
- **Stall / high-current during `power_stop`** → bounded brake duration and a coast state after braking.
- **PID gains are hardware-specific** → tuning performed on the bench; gains are named constants to tune without code structure changes.
- **All four encoders share one ISR** → keep the ISR minimal (tick counting only); all filtering lives in the periodic loop.
- **No telemetry** means clipping/limiting is silent → acceptable for this step; surfaces as a protocol requirement in step 6 if needed.

## Migration Plan

This is a rewrite of a demo sketch; there is no production behavior to migrate. Rollback: git history (`motor_controller.ino` prototype remains as reference). The new sketch is written as a new file; the prototype is retained for wiring/direction reference.

## Open Questions

- Exact PID gains, ramp duration, and brake duration — to be set during bench tuning; defaults defined as constants.
- Whether encoder measurement should count edges or full cycles — decided at implementation time based on the encoder's ticks-per-revolution spec.
