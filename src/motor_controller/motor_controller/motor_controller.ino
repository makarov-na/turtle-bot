// Блок управления приводом
// Arduino Nano + L298N + моторы с энкодерами
//
// Команды в теле робота:
//   set_speed(linear_speed, angular_speed)
//     linear_speed  — линейная скорость середины оси колёс, мм/с
//     angular_speed — угловая скорость, рад/с; + = против часовой (вид сверху)
//   stop()       — отключить драйвер, свободный выбег
//   power_stop() — силовое торможение (L298N fast motor stop)
//
// Подключение и направления моторов сверять с
// docs/4. Блок управления приводом/spec.md
//
// HC-06 Bluetooth: D0 (RX), D1 (TX), 115200 baud, Hardware Serial
//   HC-06 TX → D0 (напрямую, 3.3V читается как HIGH на 5V-логике)
//   HC-06 RX ← D1 (через делитель 5V → 3.3V, RX HC-06 не 5V-tolerant)
//   ВНИМАНИЕ: при перепрошивке через USB отключать HC-06 (загрузчик
//   использует D0/D1). SoftwareSerial не используется — его PCINT-алиасы
//   конфликтуют с ISR(PCINT1_vect) энкодеров на ATmega328P.
//
// Протокол управления по каналу (текст, \n — конец команды):
//   set_speed <linear_speed> <angular_speed>
//     linear_speed  — мм/с, любой знак
//     angular_speed — рад/с, любой знак
// Команды принимаются всегда. Watchdog: если команда не получена более
// 1000 мс, бот выполняет stop() до прихода новой команды.

#include "src/bluetooth/bt_config.h"
#include "src/protocol/command_receiver.h"
#include "motor_controller.h"
#include "src/chassis/chassis.h"
#include "src/chassis/encoders.h"
#include "src/telemetry/telemetry.h"

// Модули — отдельные C++-единицы трансляции:
//   src/chassis/encoders.h/.cpp — Encoder/Encoders и ISR квадратурных энкодеров;
//   src/chassis/chassis.h/.cpp — Chassis: привод целиком, режимы и телеметрия;
//   src/chassis/motor.h/.cpp — Motor: драйвер, энкодер и контур скорости;
//   src/chassis/drive_config.h — общие константы привода (V_MAX, база колёс);
//   motor_controller.h/.cpp — MotorController: композиция ядровых модулей;
//   src/bluetooth/bt_config.h/.cpp — автоконфигурация HC-06;
//   src/protocol/command_receiver.h/.cpp — приём, разбор команд и watchdog;
//   src/telemetry/telemetry.h/.cpp — TelemetryFrame и периодический кадр ack.
//
// Здесь остаются только точки входа Arduino и создание корневых экземпляров.
// Порядок объявления = порядок конструирования: Encoders первым (его адрес
// нужен ISR), MotorController последним (он получает ссылки на остальные
// модули).
Encoders encoders;
Chassis chassis(encoders);
CommandReceiver commands(Serial);
Telemetry telemetry(Serial);
MotorController controller(chassis, commands, telemetry);
BtConfigurator bt(Serial, LED_BUILTIN);

void setup() {
  // Инициализация железа — явная, после конструирования всех модулей.
  controller.init();

  // Автоконфигурация HC-06: поднимает UART и переводит модуль на рабочую
  // скорость. Состояние после этого живёт в самом модуле и Hardware Serial.
  bt.init();
}

void loop() {
  controller.tick();
}
