## Why

`motor_controller.ino` не компилируется: на ATmega328P библиотека `SoftwareSerial` объявляет `ISR(PCINT0_vect)` и алиасит на него `PCINT1_vect` и `PCINT2_vect`. Наш `ISR(PCINT1_vect)` для энкодеров (A0-A3, PORTC) конфликтует с этими алиасами — `multiple definition of '__vector_4'` на этапе линковки. Проверочный скетч `encoder_test.ino` подтвердил, что Hardware Serial (D0/D1) работает и с энкодерами, и с HC-06. Нужно перенести HC-06 в основном скетче на Hardware Serial.

## What Changes

- В `motor_controller.ino`: удаляется `#include <SoftwareSerial.h>` и объект `btSerial`
- HC-06 подключается на Hardware Serial D0 (RX) / D1 (TX)
- Все вызовы `btSerial.*` заменяются на `Serial.*`
- Удаляются определения пинов `PIN_BT_RX` (D12) и `PIN_BT_TX` (D13) и константы SoftwareSerial
- В `setup()` вместо `btSerial.begin` вызывается `Serial.begin(9600)`
- Периодическая отправка телеметрии раз в секунду сохраняется (теперь через `Serial`)
- Комментарий про HC-06 в шапке обновляется на D0/D1

## Capabilities

### New Capabilities
- (нет)

### Modified Capabilities
- `encoder-mapping`: требования обновляются — HC-06 переводится с SoftwareSerial (D12/D13) на Hardware Serial (D0/D1), конфликт прерываний PCINT1 устраняется

## Impact

- **Firmware**: `src/motor_controller/motor_controller/motor_controller.ino` — замена SoftwareSerial на Hardware Serial
- **Hardware**: физическое переподключение HC-06 с D12/D13 на D0/D1 (HC-06 TX→D0, HC-06 RX←D1 через делитель 5V→3.3V)
- **Совместимость**: при перепрошивке через USB загрузчик использует D0/D1 — HC-06 нужно отключать на время заливки
- **Проверка**: энкодеры на A0-A3 (PCINT1) продолжают работать без конфликтов