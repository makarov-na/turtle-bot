## MODIFIED Requirements

### Requirement: Parse set_speed commands over Hardware Serial

The firmware SHALL read incoming lines from the Bluetooth channel via Hardware Serial (D0/D1, 115200 baud) and parse commands in the text form `set_speed <linear_speed> <angular_speed>`, where `linear_speed` is in mm/s and `angular_speed` in rad/s, both possibly negative floats. A valid command SHALL be applied through the existing `set_speed()` motion controller. Lines that do not match the syntax or that contain non-numeric values SHALL be ignored without affecting the current command.

#### Scenario: Valid command is applied

- **WHEN** the line `set_speed 90 0.65` arrives over Hardware Serial
- **THEN** the motion controller receives `set_speed(90, 0.65)` and the wheels respond accordingly

#### Scenario: Reverse command is applied

- **WHEN** the line `set_speed -60 0.0` arrives
- **THEN** the robot drives in reverse at 60 mm/s

#### Scenario: Command with turn is applied

- **WHEN** the line `set_speed 120 1.2` arrives
- **THEN** the robot drives forward while turning, per the differential drive model

#### Scenario: Zero-speed command actively brakes

- **WHEN** the line `set_speed 0.0 0.0` arrives while the robot is moving
- **THEN** the robot executes an active braking stop (power_stop) and resets the PID integral, so it does not keep drifting forward from accumulated integrator windup

#### Scenario: Malformed line is ignored

- **WHEN** a line such as `set_speed abc def` or `stop_now` arrives
- **THEN** the line is ignored and the current command remains active

#### Scenario: Line terminator handling

- **WHEN** the parser receives a complete command terminated by a newline
- **THEN** the command is processed exactly once, and partial fragments are buffered until the newline
