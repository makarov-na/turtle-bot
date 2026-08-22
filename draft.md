
## Краткое описание задачи

* Сейчас нет возможности получать обратную связь от контроллера для отладки
* Необходимо реализовать передачу отладочной информации на PC через H06 Bluetooth
* Для начала нужно реализовать передачу данных в виде текстового сообщения hello PC from ADRU
* При старте контроллера нужно отправить сообщение hello PC from ADRU  в бесконечном цикле с периодом 1 сек.

# Подключение H06

| Провод       | Направление | Куда идёт           |
|--------------|-------------|---------------------|
| Arduino D13  | →           | Level shifter HV1   |
| Arduino D12  | ←           | Level shifter HV2   |
| Arduino 5V   | →           | Level shifter VCC_HV|
| Arduino 3.3V | →           | Level shifter VCC_LV|
| Arduino GND  | →           | Level shifter GND   |
| Arduino GND  | →           | HC-06 GND           |
| Level LV1    | →           | HC-06 RXD           |
| Level LV2    | ←           | HC-06 TXD           |
| 5V (Cedar)   | →           | HC-06 VCC           |
| GND (Cedar)  | →           | HC-06 GND           |



## Список тестов

*

Проверка HC-06 на openSUSE Linux
1. Подключи HC-06 к Arduino и подай питание
   LED на HC-06 должен мигать быстро — значит в pairing mode.
2. Включи Bluetooth на PC
# Проверь что bluetoothctl доступен
bluetoothctl power on
bluetoothctl scan on
Дождись появления HC-06 в списке (обычно имя HC-06 или linvor).
3. Сопряжение
   bluetoothctl
> power on
> scan on
> pair <MAC_ADDRESS_HC06>     # например AA:BB:CC:DD:EE:FF
> trust <MAC_ADDRESS_HC06>
> connect <MAC_ADDRESS_HC06>
PIN по умолчанию: 1234 (если спросит).
После сопряжения LED на HC-06 должен гореть постоянно (не мигать).
4. Найди COM-порт
# После сопряжения появится /dev/rfcomm0
ls /dev/rfcomm*

# Или через dmesg
dmesg | tail -20
Также можно проверить через bluetoothctl:
bluetoothctl info <MAC_ADDRESS>
5. Открой терминал
# Минимальный вариант — screen
screen /dev/rfcomm0 9600

# Или minicom
minicom -D /dev/rfcomm0 -b 9600

# Или Python
python3 -c "
import serial, time
s = serial.Serial('/dev/rfcomm0', 9600, timeout=1)
while True:
line = s.readline()
if line:
print(line.decode().strip())
"
6. Ожидаемый результат
   hello PC from ARDU
   hello PC from ARDU
   hello PC from ARDU
   ...
   Каждую секунду.
   Если rfcomm0 не появился
# Привязка вручную
sudo rfcomm bind /dev/rfcomm0 <MAC_ADDRESS_HC06> 1

# Проверка
ls -la /dev/rfcomm0
Диагностика проблем
Проблема
HC-06 не виден в scan
rfcomm0 не появляется
Нет данных
Мусор в терминале
Требует PIN