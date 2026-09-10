## REMOVED Requirements

### Requirement: Arduino SHALL send periodic debug messages via HC-06

**Reason**: Периодическое сообщение «hello PC from ARDU» было тестовым. Канал теперь используется для приёма команд управления от ПК, а не только для телеметрии наружу.

**Migration**: Диагностика подключения выполняется по однократному приветствию при старте, которое выводит бот через тот же канал. Периодическая отправка не требуется.

## ADDED Requirements

### Requirement: Arduino SHALL send a startup greeting via HC-06

The system SHALL send the text message "hello PC from ARDU" once when the Arduino finishes setup, through the HC-06 Bluetooth module over Hardware Serial on pins D0 (RX) and D1 (TX) at 9600 baud. No periodic repetition of the greeting or other debug messages SHALL occur.

#### Scenario: Greeting sent on startup

- **WHEN** Arduino powers on and completes setup
- **THEN** the message "hello PC from ARDU" is sent once via Hardware Serial to HC-06

#### Scenario: No periodic repetition

- **WHEN** 1 second has elapsed since the startup greeting
- **THEN** no further greeting or debug messages are sent

#### Scenario: Hardware Serial configuration unchanged

- **WHEN** Arduino setup() is called
- **THEN** Serial is initialized at 9600 baud on D0/D1 for both greeting output and incoming command parsing