## Why

Сейчас нет возможности получать обратную связь от контроллера моторов для отладки. Firmware работает "вслепую" — нет вывода данных на PC, нет телеметрии, нет подтверждения состояния. HC-06 Bluetooth модуль позволит установить беспроводной канал связи Arduino → PC для передачи отладочной информации.

## What Changes

- Добавлен SoftwareSerial на пинах D12/D13 для работы с HC-06
- Добавлен преобразователь уровней напряжения (3.3V ↔ 5V) между Arduino и HC-06
- HC-06 питается от отдельного 5V источника (Cedar buck converter)
- Firmware отправляет сообщение "hello PC from ARDU" каждую секунду через HC-06
- Hardware Serial (D0/D1) остаётся свободным для USB/CH340 (прошивка, отладка)

## Capabilities

### New Capabilities
- `bluetooth-telemetry`: Передача отладочной телеметрии с Arduino на PC через HC-06 Bluetooth модуль по каналу SoftwareSerial (D12/D13) с преобразованием уровней напряжения

### Modified Capabilities
- (нет изменений требований в существующих спеках)

## Impact

- **Firmware**: добавление `#include <SoftwareSerial.h>`, инициализация SoftwareSerial на D12/D13, вывод сообщений в setup/loop
- **Hardware**: подключение HC-06 + level shifter к Arduino Nano (D12, D13, 5V, 3.3V, GND)
- **Зависимости**: библиотека SoftwareSerial (встроена в Arduino IDE, не требует установки)
- **Ограничения**: SoftwareSerial на 9600 baud — достаточно для HC-06, но не подходит для высокоскоростной телеметрии
