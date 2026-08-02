# Motion Control

## Purpose
Define the body-frame velocity command surface, differential drive kinematics, speed limiting, acceleration ramping, and closed-loop per-wheel speed control for the robot's motor controller firmware.

## Requirements

### Requirement: Set body-frame velocity
The motion controller SHALL provide a `set_speed(linear_speed, angular_speed)` command where `linear_speed` is the linear velocity of the midpoint of the wheel axis in mm/s and `angular_speed` is the angular velocity in rad/s. Positive angular velocity SHALL mean counter-clockwise rotation viewed from above; negative SHALL mean clockwise. The controller SHALL translate a positive linear command into forward motion of the robot, accounting for the physical mounting orientation of each motor.

#### Scenario: Forward motion
- **WHEN** `set_speed(100, 0)` is issued
- **THEN** both wheels rotate such that the robot moves forward at 100 mm/s

#### Scenario: In-place counter-clockwise turn
- **WHEN** `set_speed(0, 1.0)` is issued
- **THEN** the wheels rotate in opposite directions producing a counter-clockwise spin about the midpoint of the wheel axis

#### Scenario: In-place clockwise turn
- **WHEN** `set_speed(0, -1.0)` is issued
- **THEN** the wheels rotate in opposite directions producing a clockwise spin

### Requirement: Differential drive kinematics
The controller SHALL compute per-wheel target linear speeds from the body-frame command using the differential drive model `v_l = v - ω·B/2` and `v_r = v + ω·B/2`, where `B` is the configured wheelbase.

#### Scenario: Forward with slight turn
- **WHEN** a command `(v, ω)` produces wheel targets where one wheel is faster than the other
- **THEN** the computed targets reflect the turn: one wheel faster, the other slower, with the difference proportional to `ω·B`

### Requirement: Curvature-preserving clipping
When a command would drive either wheel beyond the configured maximum speed, the controller SHALL scale **both** wheel targets by the same factor `k = v_max / max(|v_l|, |v_r|)` so the curvature of the commanded trajectory is preserved. When neither wheel target exceeds the maximum, the targets SHALL be used unchanged.

#### Scenario: Command within limits
- **WHEN** a command yields both wheel targets at or below `v_max`
- **THEN** the targets are applied unchanged

#### Scenario: One wheel exceeds the maximum
- **WHEN** a command yields `|v_r| = 300` mm/s with `v_max = 200` mm/s and `|v_l| = 100` mm/s
- **THEN** both targets are scaled by `k = 2/3` to `200` mm/s and `66.7` mm/s, preserving their ratio

#### Scenario: Pure rotation at the limit
- **WHEN** `set_speed(0, ω)` is issued such that `|ω|·B/2` exceeds `v_max`
- **THEN** the wheel targets are scaled down so the robot spins about the same center at a lower rate

### Requirement: Smooth acceleration
The controller SHALL ramp wheel speed from the current value to the commanded target over a configured ramp duration instead of stepping instantly. The ramp SHALL apply to both wheels.

#### Scenario: Ramp from rest
- **WHEN** `set_speed(200, 0)` is issued while the wheels are at rest
- **THEN** wheel speed increases gradually toward 200 mm/s without an instantaneous step

#### Scenario: Ramp on target change
- **WHEN** a new command changes the target while a ramp is in progress
- **THEN** the wheels transition smoothly from the current speed to the new target

### Requirement: Closed-loop wheel speed control
The controller SHALL measure real wheel rotation speed using the motor encoders and SHALL adjust the PWM output via a PID controller so each wheel tracks its commanded target speed.

#### Scenario: Tracks the commanded speed
- **WHEN** a target wheel speed is set
- **THEN** the measured wheel speed converges to the target within the configured tolerance

#### Scenario: Recovers from load disturbance
- **WHEN** a momentary load slows a wheel below its target
- **THEN** the PID controller increases output to restore the wheel to the target speed

### Requirement: Configurable parameters
The controller SHALL support configuration of wheel diameter (mm), wheelbase (mm), and maximum linear speed (mm/s). Maximum angular velocity SHALL be derived as `ω_max = 2·v_max / B` and SHALL NOT require separate configuration. Commands producing wheel targets beyond the limits SHALL be handled by the curvature-preserving clipping rule.

#### Scenario: Wheel diameter affects speed mapping
- **WHEN** the configured wheel diameter is changed
- **THEN** the wheel rotation speed required for the same body linear speed changes inversely with the diameter

#### Scenario: Derived maximum angular velocity
- **WHEN** wheelbase `B` and maximum speed `v_max` are configured
- **THEN** the maximum angular velocity is `2·v_max/B` without separate configuration
