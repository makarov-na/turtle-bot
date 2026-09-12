## MODIFIED Requirements

### Requirement: Closed-loop wheel speed control (MODIFIED)

The controller SHALL measure real wheel rotation speed using the motor encoders and SHALL adjust the PWM output via a PID controller so each wheel tracks its commanded target speed. The feedback SHALL be sign-correct: a wheel spun forward by an external force SHALL be opposed (braked) by the loop, not driven further. The controller SHALL include a static feedforward term from the commanded speed to PWM so the loop can reach low-speed targets even when the mechanical free-play threshold (stiction/cogging) is above what the error-driven terms alone can produce. After any pause during which the control loop did not run (braking, coasting, watchdog stop), the first iteration SHALL NOT attribute the accumulated encoder delta or elapsed wall time to the current control step, so resuming motion causes no false speed reading or integrator spike.

#### Scenario: Tracks the commanded speed

- **WHEN** a target wheel speed is set
- **THEN** the measured wheel speed converges to the target within the configured tolerance

#### Scenario: Recovers from load disturbance

- **WHEN** a momentary load slows a wheel below its target
- **THEN** the PID controller increases output to restore the wheel to the target speed

#### Scenario: Hand-spun wheel is braked (sign check)

- **WHEN** a wheel is spun forward by hand while the loop is active at a low or zero target
- **THEN** the measured speed of that wheel is positive and the PWM output goes opposite to the spin (the motor resists the hand rotation)

#### Scenario: Low-speed target is reachable

- **WHEN** the commanded target is below the mechanical free-play threshold of the drivetrain
- **THEN** the feedforward plus accumulated integrator raise the PWM output enough to start the wheel within a bounded time

#### Scenario: Resume after a stop is clean

- **WHEN** a non-zero command resumes the loop after a pause longer than one control period with no loop activity
- **THEN** the first control steps use a nominal step time (not the whole pause duration) and treat the encoder delta since the pause as zero, so no integrator whiplash or false speed spike occurs