## 1. Sketch skeleton and configuration

- [ ] 1.1 Create a new sketch file for the motor controller firmware, keeping the prototype untouched
- [ ] 1.2 Define pin assignments (D2/D3/D5 right motor, D4/D7/D6 left motor, D8-D11 encoders) as named constants
- [ ] 1.3 Define configuration constants: wheel diameter (mm), wheelbase (mm), v_max (mm/s), ramp duration, brake duration
- [ ] 1.4 Derive `omega_max = 2 * v_max / wheelbase` as a constant
- [ ] 1.5 Implement `setup()` that initializes pins and leaves motors disabled (boot safety)

## 2. Encoder reading

- [ ] 2.1 Implement a single `PCINT0` interrupt service routine decoding quadrature ticks on D8-D11 with software debounce
- [ ] 2.2 Implement per-wheel tick counters with direction (forward/reverse)
- [ ] 2.3 Implement wheel linear-speed measurement from tick counts over the PID loop period

## 3. Kinematics and command surface

- [ ] 3.1 Implement `set_speed(linear_speed, angular_speed)` storing the commanded (v, ω) target
- [ ] 3.2 Implement differential drive mapping `v_l = v - ω·B/2`, `v_r = v + ω·B/2`
- [ ] 3.3 Apply the left-motor mounting mirror so a positive `v` drives the robot forward
- [ ] 3.4 Implement curvature-preserving clipping: scale both wheel targets by `k = v_max / max(|v_l|, |v_r|)` when exceeded
- [ ] 3.5 Convert wheel linear speeds to wheel rotation speeds for the PID loop using the wheel diameter

## 4. Closed-loop speed control

- [ ] 4.1 Implement a per-wheel PID controller with encoder feedback producing PWM output
- [ ] 4.2 Implement a periodic control loop (fixed sample period) reading encoders and updating PID output
- [ ] 4.3 Implement PWM and direction output to the L298N per wheel

## 5. Acceleration ramp

- [ ] 5.1 Implement a ramp that moves the ramped target from the current speed toward the commanded target over the configured duration
- [ ] 5.2 Ensure the ramp applies to both wheels and the PID tracks the ramped target, not the commanded one

## 6. Braking and stopping

- [ ] 6.1 Implement `stop()` that disables the driver (free coast)
- [ ] 6.2 Implement `power_stop()` using the L298N fast motor stop (both IN high, enable active) for the configured brake duration, then disable

## 7. Demo loop and verification

- [ ] 7.1 Implement the minimal demo loop: `set_speed(0, 0)`, then `stop()` after 10 seconds
- [ ] 7.2 Verify motors remain off on boot and wheels can be turned by hand
- [ ] 7.3 Bench-verify motor direction mapping against the prototype wiring reference (forward = both wheels drive robot forward)
- [ ] 7.4 Bench-verify curvature-preserving clipping with a wheel speed above v_max
- [ ] 7.5 Bench-verify `power_stop()` stops the wheels quickly and releases after the brake duration
