# Bluetooth Telemetry

## Purpose
Передача отладочной телеметрии с Arduino на PC через HC-06 Bluetooth модуль по каналу Hardware Serial (D0/D1) с защитой логических уровней для RX HC-06.

## Requirements

### Requirement: Arduino SHALL send periodic debug messages via HC-06

The system SHALL send the text message "hello PC from ARDU" through HC-06 Bluetooth module every 1 second using Hardware Serial on pins D0 (RX) and D1 (TX), replacing the previous SoftwareSerial on D12/D13.

#### Scenario: Message sent on startup

- **WHEN** Arduino powers on and completes setup
- **THEN** the message "hello PC from ARDU" is sent via Hardware Serial to HC-06

#### Scenario: Periodic repetition

- **WHEN** 1 second has elapsed since the last message was sent
- **THEN** the message "hello PC from ARDU" is sent again via Hardware Serial to HC-06

#### Scenario: Continuous operation

- **WHEN** Arduino is running and HC-06 is connected
- **THEN** messages continue to be sent every 1 second without stopping

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