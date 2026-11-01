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

// Табы скетча (Arduino склеивает этот файл первым, остальные — по алфавиту):
//   bt_config.ino — автоконфигурация HC-06;
//   commands.ino  — приём и разбор команд, watchdog;
//   encoders.ino  — классы Encoder/Encoders, ISR квадратурных энкодеров;
//   motors.ino    — драйверы, кинематика и PID;
//   telemetry.ino — периодический кадр телеметрии.

// Общий идентификатор мотора: используется motors.ino, encoders.ino и
// telemetry.ino.
enum MotorId { MOTOR_LEFT = 0, MOTOR_RIGHT = 1 };

void setup() {
  motorsInit();

  // Конфигурация HC-06 вынесена в bt_config.ino; по выходу Hardware Serial
  // уже открыт на нужной скорости (BT_BAUD объявлен в том файле).
  uint8_t btResult = btConfigureModule();
  btBlinkResult(btResult);
  Serial.println("hello PC from ARDU");
}

void loop() {
  uint32_t now = millis();

  motorsTick(now);
  commandsProcess();
  telemetryTick(now);
  commandsWatchdogTick();
}
