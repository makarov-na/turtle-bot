# Bluetooth Telemetry

## Purpose
Передача отладочной телеметрии с Arduino на PC через HC-06 Bluetooth модуль по каналу Hardware Serial (D0/D1) с защитой логических уровней для RX HC-06.
## Requirements
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

### Requirement: Hardware Serial SHALL be initialized on D0/D1

The system SHALL use the built-in Hardware Serial (UART) on pins D0 (RX, receiving from HC-06 TXD) and D1 (TX, transmitting to HC-06 RXD) at 9600 baud. SoftwareSerial MUST NOT be used, because its PCINT aliasing conflicts with the encoder `ISR(PCINT1_vect)` on the ATmega328P.

#### Scenario: Hardware Serial configuration

- **WHEN** Arduino setup() is called
- **THEN** Serial is initialized at 9600 baud

#### Scenario: HC-06 communication channel is independent from USB

- **WHEN** Hardware Serial is active on D0/D1
- **THEN** the same data is available both on HC-06 and on the USB/CH340 monitor

#### Scenario: No interrupt conflict with encoders

- **WHEN** the firmware is compiled with encoder ISR on PCINT1 and Hardware Serial
- **THEN** linking succeeds without a multiple definition error for the shared interrupt vector

### Requirement: Logic level conversion SHALL protect HC-06 RX

The system SHALL protect HC-06 RXD from Arduino 5V logic by using a voltage divider (5V → 3.3V) on the D1 (TX) to HC-06 RXD line. HC-06 TXD (3.3V) SHALL connect directly to D0 (RX), which reads 3.3V as a valid HIGH.

#### Scenario: Arduino TX to HC-06 RX

- **WHEN** Arduino transmits via D1 (TX, 5V)
- **THEN** the voltage divider drops the signal to 3.3V before reaching HC-06 RXD

#### Scenario: HC-06 TX to Arduino RX

- **WHEN** HC-06 transmits via TXD (3.3V)
- **THEN** the signal connects directly to Arduino D0 (RX) and is read as valid HIGH

### Requirement: HC-06 SHALL be powered from separate 5V source

The system SHALL power HC-06 VCC from the Cedar DC-DC buck converter 5V output, not from Arduino's 5V pin.

#### Scenario: HC-06 power supply

- **WHEN** Arduino is powered on
- **THEN** HC-06 receives 5V from Cedar buck converter via separate power wire

#### Scenario: Common ground

- **WHEN** HC-06 is powered from Cedar buck converter
- **THEN** HC-06 GND is connected to Arduino GND (common ground reference)

### Requirement: Existing motor control SHALL remain unchanged

The system SHALL preserve all existing motor control functionality (L298N, encoders, PID) without modification.

#### Scenario: Motor operation after HC-06 addition

- **WHEN** HC-06 is connected and transmitting
- **THEN** both motors respond to set_speed(), stop(), and power_stop() commands identically to before

#### Scenario: Encoder readings unaffected

- **WHEN** HC-06 is active on D0/D1 via Hardware Serial
- **THEN** encoder tick counts on A0-A3 continue to be read accurately, with no interrupt conflict

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

