## RENAMED Requirements

- FROM: `### Requirement: Arduino SHALL acknowledge each command frame`
- TO: `### Requirement: Arduino SHALL send a periodic telemetry frame`

## MODIFIED Requirements

### Requirement: Arduino SHALL send a startup greeting via HC-06

The system SHALL send the text message "hello PC from ARDU" once when the Arduino finishes setup, through the HC-06 Bluetooth module over Hardware Serial on pins D0 (RX) and D1 (TX) at 9600 baud. No periodic repetition of the greeting SHALL occur. Steady-state outgoing traffic consists of the periodic telemetry frame (separate requirement below), which is autonomous and not tied to command arrival.

#### Scenario: Greeting sent on startup

- **WHEN** Arduino powers on and completes setup
- **THEN** the message "hello PC from ARDU" is sent once via Hardware Serial to HC-06

#### Scenario: No periodic repetition of the greeting

- **WHEN** 1 second has elapsed since the startup greeting
- **THEN** no further greetings are sent; the only subsequent outgoing traffic is the periodic telemetry frame

#### Scenario: Hardware Serial configuration unchanged

- **WHEN** Arduino setup() is called
- **THEN** Serial is initialized at 9600 baud on D0/D1 for both greeting output and incoming command parsing

### Requirement: Arduino SHALL send a periodic telemetry frame

The system SHALL send a telemetry frame over Hardware Serial (D0/D1, 9600 baud) once every 200 ms, driven by a timer in the main loop independently of command arrival. The frame SHALL have the form `ack <n> <linear> <angular> mL=<ml> mR=<mr> oL=<ol> oR=<or> iL=<il> iR=<ir>`, where `<n>` is a running counter of processed valid `set_speed` commands, `<linear>`/`<angular>` are the most recently commanded speeds (mm/s and rad/s, both `0` before any command has arrived), and the `mL/mR/oL/oR/iL/iR` fields carry the current measured speed, PID output and PID integral of each wheel. The frame SHALL be sent regardless of drive state. Parsing or applying a command SHALL NOT itself emit a frame. The periodic frame keeps the RFCOMM link continuously bidirectional; this keepalive is required because an HC-06 link with one-directional traffic drops at ~5.2 s, while continuous bidirectional traffic is verified to survive ≥60 s (bt-echo-test L3).

#### Scenario: Telemetry is sent on the timer

- **WHEN** 200 ms have elapsed since the previous telemetry frame
- **THEN** a frame is sent regardless of whether a new command has arrived

#### Scenario: Frame echoes the last commanded speed

- **WHEN** the last valid command received was `set_speed 30.0 0.0`
- **THEN** subsequent telemetry frames report the fields `ack <n> 30.0 0.00` with `<n>` equal to the running command counter

#### Scenario: A command does not trigger an extra frame

- **WHEN** a valid `set_speed` line arrives between two timer ticks
- **THEN** the command is applied and the counter increments, but no frame is emitted from the command handler

#### Scenario: Invalid line does not change telemetry

- **WHEN** a line that does not match `set_speed` arrives
- **THEN** neither the command counter nor the echoed last commanded speed changes, and frames continue to be sent only by the periodic timer
