## Why

The drive block is the first implemented component of the robot. The current sketch (`src/motor_controller/motor_controller/motor_controller.ino`) is a demo that runs a fixed motion sequence with no control over speed, no encoders, no command interface. The next step in the general plan (step 5: motor control block) is a functional module that receives body-frame velocity commands `(v, ω)`, executes them precisely, and stops/branches motors safely. This firmware is the foundation the integration protocol (step 6) will be built on.

## What Changes

- Rewrite the Arduino sketch from scratch (nothing is ported from the prototype — it is used only as a reference for wiring and motor directions).
- Add a `set_speed(linear_speed, angular_speed)` command: body-frame linear velocity `v` (mm/s) and angular velocity `ω` (rad/s) at the midpoint of the wheel axis. Positive `ω` is counter-clockwise viewed from above, negative is clockwise.
- Add `stop()` (disable motors / free coast) and `power_stop()` (active braking per L298N: both IN lines high, fast motor stop).
- Add closed-loop per-wheel speed control using encoders (PID).
- Add smooth acceleration ramp towards the commanded speed.
- Add clipping: when a wheel speed target exceeds max, scale **both** wheels proportionally so the curvature of the commanded trajectory is preserved.
- Add configurable settings: wheel diameter (mm), wheelbase (mm), max speed (mm/s). Max angular velocity is derived from max speed and wheelbase.
- Keep the demo loop minimal by design: `set_speed(0, 0)`, then `stop()` after 10 seconds — a wiring verification, not a behavior demo.

## Capabilities

### New Capabilities

- `motion-control`: body-frame `set_speed(v, ω)` command, differential drive kinematics, per-wheel PID speed control on encoders, acceleration ramp, and curvature-preserving clipping.
- `motor-braking`: `stop()` (disable, coast) and `power_stop()` (L298N fast motor stop) with defined timing.

### Modified Capabilities

(none)

## Impact

- `src/motor_controller/motor_controller/motor_controller.ino` — rewritten from scratch.
- Wiring reference: Arduino Nano, L298N driver, DC motors with encoders. Right motor D2/D3/D5 + encoder D10/D11; left motor D4/D7/D6 + encoder D8/D9.
- No new external dependencies (Arduino standard libraries only).
- The integration protocol (step 6 of the general plan) will build on this firmware's command surface.
