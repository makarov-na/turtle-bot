## 1. Sketch skeleton and configuration

- [x] 1.1 Create a new sketch file for the motor controller firmware, keeping the prototype untouched
- [x] 1.2 Define pin assignments (D2/D3/D5 right motor, D4/D7/D6 left motor, D8-D11 encoders) as named constants
- [x] 1.3 Define configuration constants: wheel diameter (mm), wheelbase (mm), v_max (mm/s), ramp duration, brake duration
- [x] 1.4 Derive `omega_max = 2 * v_max / wheelbase` as a constant
- [x] 1.5 Implement `setup()` that initializes pins and leaves motors disabled (boot safety)

## 2. Encoder reading

- [x] 2.1 Implement a single `PCINT0` interrupt service routine decoding quadrature ticks on D8-D11 with software debounce
- [x] 2.2 Implement per-wheel tick counters with direction (forward/reverse)
- [x] 2.3 Implement wheel linear-speed measurement from tick counts over the PID loop period

## 3. Kinematics and command surface

- [x] 3.1 Implement `set_speed(linear_speed, angular_speed)` storing the commanded (v, ω) target
- [x] 3.2 Implement differential drive mapping `v_l = v - ω·B/2`, `v_r = v + ω·B/2`
- [x] 3.3 Apply the left-motor mounting mirror so a positive `v` drives the robot forward
- [x] 3.4 Implement curvature-preserving clipping: scale both wheel targets by `k = v_max / max(|v_l|, |v_r|)` when exceeded
- [x] 3.5 Convert wheel linear speeds to wheel rotation speeds for the PID loop using the wheel diameter

## 4. Closed-loop speed control

- [x] 4.1 Implement a per-wheel PID controller with encoder feedback producing PWM output
- [x] 4.2 Implement a periodic control loop (fixed sample period) reading encoders and updating PID output
- [x] 4.3 Implement PWM and direction output to the L298N per wheel

## 5. Acceleration ramp

- [x] 5.1 Implement a ramp that moves the ramped target from the current speed toward the commanded target over the configured duration
- [x] 5.2 Ensure the ramp applies to both wheels and the PID tracks the ramped target, not the commanded one

## 6. Braking and stopping

- [x] 6.1 Implement `stop()` that disables the driver (free coast)
- [x] 6.2 Implement `power_stop()` using the L298N fast motor stop (both IN high, enable active) for the configured brake duration, then disable

## 7. Demo loop and verification

- [x] 7.1 Implement the minimal demo loop: `set_speed(0, 0)`, then `stop()` after 10 seconds
- [x] 7.2 Verify motors remain off on boot and wheels can be turned by hand
- [x] 7.3 Bench-verify motor direction mapping against the prototype wiring reference (forward = both wheels drive robot forward)
- [x] 7.4 Bench-verify curvature-preserving clipping with a wheel speed above v_max
- [x] 7.5 Bench-verify `power_stop()` stops the wheels quickly and releases after the brake duration

## 8. Diagnosis spike: asymmetric drive (right fast / left barely turns)

Symptom on the bench: the robot turns instead of driving straight — right wheel spins fast, left barely rotates. The PID/encoder code is symmetric across wheels, so the asymmetry must come from either (a) left encoder feedback lying high → PID throttles the left wheel, or (b) the left drive channel being physically weak → PID saturates but the wheel cannot reach speed. Sign errors (swapped A/B, wrong mirror) were excluded by reasoning: they make a wheel spin *fast*, not slow.

Decision tree (cheapest test first):

```
sound test: run demo, listen to the left wheel
├─ whining while barely turning  →  PID saturated  →  weak hardware
│                                     (motor / L298N channel B / gearbox)
└─ quiet while barely turning    →  PID throttling  →  feedback lies (encoder)
                                        │
                    open-loop test: identical fixed PWM on both wheels
                    ├─ asymmetric  →  hardware (drivers/motors differ)
                    └─ symmetric   →  closed-loop / encoder issue
                                        │
                    encoder swap test: swap D8/D9 ↔ D10/D11 connectors
                    ├─ problem follows connector  →  left encoder/wiring
                    └─ stays on left             →  PID / decoding / config
```

- [x] 8.1 Static wiring audit of the left channel (IN3 D4, IN4 D7, ENB D6, enc D8/D9) against the wiring reference in `docs/4. .../spec.md`; check jumpers, solder, common GND, encoder pull-ups
- [x] 8.2 Sound test (no code): run the forward demo and classify the left wheel by sound — whining while barely turning means PID is saturated (weak hardware); quiet means PID throttles it (feedback lies)
- [x] 8.3 Build a temporary diagnostic sketch with two modes: open loop (both wheels at identical fixed PWM, no feedback/PID) and closed loop with serial telemetry (print per-wheel ramped vs measured linear speed and PWM every ~200 ms)
- [x] 8.4 Encoder-swap test (physical): swap the left/right encoder connectors and rerun; note whether the slow wheel follows the connector or stays on the left
- [x] 8.5 Manual ticks-per-rev calibration: spin each wheel N full turns by hand, read the accumulated tick counter via serial; compare both sides against `TICKS_PER_WHEEL_REV = 2464` and against each other
- [x] 8.6 Record root cause and resolution: if code/config (per-wheel CPR, debounce, sign) amend this change or open a follow-up; if hardware (motor / L298N / wiring) fix and re-run 7.3
