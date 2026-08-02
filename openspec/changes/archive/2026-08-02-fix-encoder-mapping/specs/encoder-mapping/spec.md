## ADDED Requirements

### Requirement: Correct encoder-to-wheel mapping
The controller SHALL associate each wheel's PID feedback channel with the encoder physically attached to that same wheel. The left wheel SHALL read the encoder connected to pins D10/D11 and the right wheel SHALL read the encoder connected to pins D8/D9, matching the physical wiring.

#### Scenario: Left wheel feedback reads left encoder
- **WHEN** the left wheel's motor rotates and the right wheel's motor is stationary
- **THEN** the left wheel's measured speed reflects the left wheel's own encoder tick count and the right wheel's measured speed is zero

#### Scenario: Right wheel feedback reads right encoder
- **WHEN** the right wheel's motor rotates and the left wheel's motor is stationary
- **THEN** the right wheel's measured speed reflects the right wheel's own encoder tick count and the left wheel's measured speed is zero

#### Scenario: Straight forward motion
- **WHEN** `set_speed(250, 0)` is issued with the correct mapping in place
- **THEN** both wheels track their own commanded speed without the PID chasing the other wheel, and the robot does not turn sharply

### Requirement: Encoder direction sign per wheel
Each wheel's measured speed SHALL be positive when the wheel drives the robot forward, accounting for the mirror-mounted left motor. The firmware SHALL expose per-wheel sign compensation so a mismatched encoder channel polarity can be corrected without rewiring.

#### Scenario: Forward rotation reads positive on both wheels
- **WHEN** the robot is commanded forward and both wheels rotate in the direction that moves the robot forward
- **THEN** both wheels' measured speeds are positive

#### Scenario: Backward rotation reads negative on both wheels
- **WHEN** the robot is commanded backward and both wheels rotate in the direction that moves the robot backward
- **THEN** both wheels' measured speeds are negative

#### Scenario: Sign correction without rewiring
- **WHEN** an encoder channel polarity mismatch causes a wheel to measure its direction inverted
- **THEN** the wheel's sign compensation can be flipped in firmware and the wheel then reads the correct direction

### Requirement: Consistent wiring documentation
The wiring reference SHALL state the encoder connections that match the physical hardware: left motor encoder on D10/D11 and right motor encoder on D8/D9, consistent with the firmware mapping.

#### Scenario: Documentation matches hardware
- **WHEN** an operator rebuilds the wiring from the reference document
- **THEN** the encoder connections match the firmware mapping and the physical hardware
