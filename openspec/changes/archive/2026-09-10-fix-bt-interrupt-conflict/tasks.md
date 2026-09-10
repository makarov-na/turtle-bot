## 1. Доработка motor_controller.ino

- [x] 1.1 Удалить `#include <SoftwareSerial.h>` и объект `SoftwareSerial btSerial`
- [x] 1.2 Удалить определения `PIN_BT_RX` (D12), `PIN_BT_TX` (D13) и константу `BT_BAUD`
- [x] 1.3 Заменить в `setup()` `btSerial.begin(BT_BAUD)` на `Serial.begin(9600)`
- [x] 1.4 Заменить все вызовы `btSerial.*` на `Serial.*` (setup + loop, телеметрия раз в секунду)
- [x] 1.5 Обновить шапку комментария: HC-06 на D0/D1 через Hardware Serial, делитель на TX→RX, отключать HC-06 при перепрошивке

## 2. Проверка

- [x] 2.1 Скомпилировать `motor_controller.ino` (arduino-cli --fqbn arduino:avr:nano) без ошибок
- [x] 2.2 Проверить на железе: телеметрия «hello PC from ARDU» приходит раз в секунду через HC-06/D0/D1