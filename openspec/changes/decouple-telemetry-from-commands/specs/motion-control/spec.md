## MODIFIED Requirements

### Requirement: Closed-loop wheel speed control

The controller SHALL measure real wheel rotation speed using the motor encoders and SHALL adjust the PWM output via a PID controller so each wheel tracks its commanded target speed. The feedback SHALL be sign-correct: a wheel spun forward by an external force SHALL be opposed (braked) by the loop, not driven further. The controller SHALL include a static feedforward term from the commanded speed to PWM so the loop can reach low-speed targets even when the mechanical free-play threshold (stiction/cogging) is above what the error-driven terms alone can produce. The controller SHALL re-baseline its measurement reference whenever the control loop enters the running state after a pause during which it did not run (braking, coasting, watchdog stop), so the first iteration after resuming does not attribute the accumulated encoder delta or elapsed wall time to the current control step. A long iteration during active control — for example while the blocking telemetry frame is being transmitted — SHALL NOT be treated as such a pause: the controller SHALL measure the wheel speed over the true elapsed time and SHALL NOT substitute a zero measurement. The next control iteration's deadline SHALL NOT be scheduled ahead of the current time, so that an overrunning iteration cannot make the elapsed-time counter wrap to a spurious large value.

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

#### Scenario: Blocking telemetry does not fabricate a stop

- **WHEN** the main loop is blocked during active control for longer than one control period (for example, by transmitting the telemetry frame), and then the control loop runs again
- **THEN** the controller measures the wheel speed over the true elapsed time since the previous control step and the PID output does not saturate because of a substituted zero measurement

#### Scenario: A late control iteration is not scheduled into the future

- **WHEN** a control iteration starts later than the nominal control period after the previous one
- **THEN** the next iteration's deadline is derived from the current time and never lies ahead of it, so the elapsed-time counter cannot wrap to a spurious large value
