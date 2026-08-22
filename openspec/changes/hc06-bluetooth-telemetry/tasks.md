## 1. Setup — SoftwareSerial

- [x] 1.1 Добавить `#include <SoftwareSerial.h>` в motor_controller.ino
- [x] 1.2 Определить пины: `PIN_BT_RX 12`, `PIN_BT_TX 13`
- [x] 1.3 Определить скорость: `BT_BAUD 9600`
- [x] 1.4 Создать объект SoftwareSerial: `SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);`

## 2. Инициализация в setup()

- [x] 2.1 Добавить `btSerial.begin(BT_BAUD);` в setup() после существующего кода
- [x] 2.2 Добавить начальное сообщение `btSerial.println("hello PC from ARDU");` в setup() для проверки связи при старте

## 3. Периодическая отправка в loop()

- [x] 3.1 Добавить переменную `lastBtSendMs` для отслеживания времени последней отправки
- [x] 3.2 Добавить блок в loop(): каждую 1 секунду отправлять `"hello PC from ARDU"` через btSerial.println()
- [x] 3.3 Убедиться, что отправка не блокирует основной цикл (non-blocking, через millis())

## 4. Проверка — существующий код не сломан

- [x] 4.1 Убедиться, что D12/D13 не используются в другом коде firmware
- [x] 4.2 Убедиться, что Hardware Serial (D0/D1) не задействован новым кодом
- [x] 4.3 Проверить, что PCINT прерывания на D8-D11 не конфликтуют с SoftwareSerial
