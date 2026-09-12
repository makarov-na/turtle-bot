## REMOVED Requirements

### Requirement: Arduino SHALL send periodic debug messages via HC-06

**Reason**: Периодическое сообщение «hello PC from ARDU» было тестовым. Канал теперь используется для приёма команд управления от ПК, а не только для телеметрии наружу.

**Migration**: Диагностика подключения выполняется по однократному приветствию при старте, которое выводит бот через тот же канал. Периодическая отправка не требуется.

## ADDED Requirements

### Requirement: Arduino SHALL send a startup greeting via HC-06

The system SHALL send the text message "hello PC from ARDU" once when the Arduino finishes setup, through the HC-06 Bluetooth module over Hardware Serial on pins D0 (RX) and D1 (TX) at 9600 baud. No periodic repetition of the greeting SHALL occur; the only further outgoing traffic is the per-command acknowledgement (separate requirement below).

#### Scenario: Greeting sent on startup

- **WHEN** Arduino powers on and completes setup
- **THEN** the message "hello PC from ARDU" is sent once via Hardware Serial to HC-06

#### Scenario: No periodic repetition

- **WHEN** 1 second has elapsed since the startup greeting
- **THEN** no further greetings or autonomous debug messages are sent; per-command acknowledgements are sent only in response to incoming commands

#### Scenario: Hardware Serial configuration unchanged

- **WHEN** Arduino setup() is called
- **THEN** Serial is initialized at 9600 baud on D0/D1 for both greeting output and incoming command parsing

### Requirement: Arduino SHALL acknowledge each command frame

The system SHALL send an acknowledgement line `ack <n> <linear> <angular>` over Hardware Serial (D0/D1, 9600 baud) after successfully parsing each valid `set_speed` command, where `<n>` is a running counter of processed commands and `<linear>`/`<angular>` are the parsed speeds (mm/s and rad/s). Together with the PC heartbeat (one command every 200 ms), this acknowledgement keeps the RFCOMM link continuously bidirectional. This keepalive is required because an HC-06 link with one-directional traffic drops at ~5.2 s, while continuous bidirectional traffic is verified to survive ≥60 s (bt-echo-test L3).

#### Scenario: Command is acknowledged

- **WHEN** the line `set_speed 30.0 0.0` arrives over Hardware Serial
- **THEN** the system replies `ack <n> 30.0 0.0` (n = incremented counter)

#### Scenario: Keepalive cadence

- **WHEN** valid commands arrive every 200 ms
- **THEN** an acknowledgement is sent for each command, producing continuous bidirectional RFCOMM traffic

#### Scenario: Invalid line is not acknowledged

- **WHEN** a line that does not match `set_speed` arrives
- **THEN** no acknowledgement is sent and the command counter does not increment