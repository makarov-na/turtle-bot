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
// WATCHDOG_TIMEOUT_MS, бот выполняет stop() до прихода новой команды.

#include "bt_config.h"

// Табы скетча (Arduino склеивает этот файл первым, остальные .ino — по
// алфавиту):
//   commands.ino  — CommandReceiver: приём, разбор, watchdog;
//   encoders.ino  — Encoder/Encoders и ISR квадратурных энкодеров;
//   motors.ino    — Motor/Chassis: драйверы, кинематика и PID;
//   telemetry.ino — Telemetry и Robot: кадр ack и композиция модулей.
//
// Отдельный C++-модуль: bt_config.h/.cpp — автоконфигурация HC-06.

// Общий идентификатор мотора: используется motors.ino, encoders.ino и
// telemetry.ino.
enum class MotorId : uint8_t { Left = 0, Right = 1, Count = 2 };

// Снимок одного кадра телеметрии. Заполняется Chassis и CommandReceiver,
// форматируется Telemetry; поля соответствуют протоколу ack.
struct TelemetryFrame {
  uint32_t counter;
  float linear;
  float angular;
  float measured[2];
  float pwm[2];
  float integral[2];
};

void setup() {
  // Ядровая логика — композиция Robot в telemetry.ino.
  robotInit();

  // Автоконфигурация HC-06 — самостоятельный опциональный шаг. Экземпляр
  // нужен только на время инициализации: после setup() состояние живёт в
  // самом модуле HC-06 и Hardware Serial.
  BtConfigurator bt;
  bt.btInit(Serial, LED_BUILTIN);
}

void loop() {
  robotTick();
}
