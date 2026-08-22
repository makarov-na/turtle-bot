## ADDED Requirements

### Requirement: Arduino SHALL send periodic debug messages via HC-06
The system SHALL send the text message "hello PC from ARDU" through HC-06 Bluetooth module every 1 second using SoftwareSerial on pins D12 (RX) and D13 (TX).

#### Scenario: Message sent on startup
- **WHEN** Arduino powers on and completes setup
- **THEN** the message "hello PC from ARDU" is sent via SoftwareSerial to HC-06

#### Scenario: Periodic repetition
- **WHEN** 1 second has elapsed since the last message was sent
- **THEN** the message "hello PC from ARDU" is sent again via SoftwareSerial to HC-06

#### Scenario: Continuous operation
- **WHEN** Arduino is running and HC-06 is connected
- **THEN** messages continue to be sent every 1 second without stopping

### Requirement: SoftwareSerial SHALL be initialized on D12/D13
The system SHALL use SoftwareSerial library with pin D12 as RX (receiving from HC-06 TXD) and pin D13 as TX (transmitting to HC-06 RXD) at 9600 baud.

#### Scenario: SoftwareSerial configuration
- **WHEN** Arduino setup() is called
- **THEN** SoftwareSerial is initialized with RX=D12, TX=D13 at 9600 baud

#### Scenario: HC-06 communication channel is independent from USB
- **WHEN** SoftwareSerial is active on D12/D13
- **THEN** Hardware Serial (D0/D1) remains available for USB/CH340 communication

### Requirement: Level shifter SHALL convert logic levels between Arduino and HC-06
The system SHALL use a BSS138-based level shifter module to convert 5V logic from Arduino to 3.3V logic for HC-06 RXD, and 3.3V logic from HC-06 TXD to 5V for Arduino D12.

#### Scenario: Arduino TX to HC-06 RX
- **WHEN** Arduino transmits via SoftwareSerial TX (D13, 5V)
- **THEN** the level shifter converts the signal to 3.3V before reaching HC-06 RXD

#### Scenario: HC-06 TX to Arduino RX
- **WHEN** HC-06 transmits via TXD (3.3V)
- **THEN** the level shifter converts the signal to 5V before reaching Arduino D12

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
- **WHEN** HC-06 is active on D12/D13
- **THEN** encoder tick counts on D8-D11 are unaffected
