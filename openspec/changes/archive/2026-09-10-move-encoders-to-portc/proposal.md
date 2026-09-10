## Why

При подключении HC-06 Bluetooth через SoftwareSerial на D12/D13 возникает конфликт прерываний: SoftwareSerial и ISR(PCINT0_vect) для энкодеров используют один вектор прерываний PORTB. Это не компилируется. Энкодеры временно отключены. Необходимо перенести энкодеры на PORTC (A0-A3), чтобы каждый порт имел свой вектор прерываний и конфликт исчез.

## What Changes

- Пины энкодеров переносятся с D8-D11 (PORTB) на A0-A3 (PORTC)
- ISR(PCINT0_vect) заменяется на ISR(PCINT1_vect)
- Регистры PCICR/PCMSK0 заменяются на PCIE1/PCMSK1
- Чтение пинов: PINB → PINC
- Восстанавливается работа энкодеров (была отключена)
- HC-06 на D12/D13 остаётся без изменений
- Проводная документация обновляется

## Capabilities

### New Capabilities
- (нет)

### Modified Capabilities
- `encoder-mapping`: Требования обновляются — пины энкодеров меняются с D8-D11 на A0-A3, ISR и регистры прерываний изменяются

## Impact

- **Firmware**: motor_controller.ino — изменение пинов, ISR, PCICR/PCMSK настройки
- **Hardware**: физическая перепайка 4 проводов энкодеров (D8→A0, D9→A1, D10→A2, D11→A3)
- **Документация**: specs/encoder-mapping/spec.md — обновление ссылок на пины
- **Ограничения**: A4/A5 (I2C) не задеваются; A0-A3 работают как digital input через PCINT
