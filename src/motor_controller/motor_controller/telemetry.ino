// --- Периодическая телеметрия и композиция модулей -------------------------
// Telemetry только форматирует готовый TelemetryFrame: таймер кадра и порт
// принадлежат классу, а откуда взялись значения, ему неизвестно.
//
// Robot — корень композиции ядровой логики: владеет приводом, приёмником
// команд и телеметрией, связывает их зависимости и повторяет прежний порядок
// работы loop(). Автоконфигурация HC-06 — самостоятельная опциональная
// функция: её экземпляр создаётся в основном скетче и запускается из setup()
// отдельно.
//
// Кадр печатается по таймеру, а не в ответ на команду: иначе накопившиеся в
// RX-буфере команды печатаются пачкой и не дают контуру выполниться
// (см. design.md).
//
// Непрерывный двунаправленный трафик удерживает RFCOMM-линк HC-06 (проверено:
// ≥60 с против обрыва ~5.2 с на одностороннем трафике).

const uint32_t TELEMETRY_PERIOD_MS = 200;

class Telemetry {
 public:
  // Привязывает кадры к порту. Вызывается из Robot::begin(), когда порт уже
  // настроен BT-конфигуратором.
  void begin(Stream &serial) {
    serial_ = &serial;
  }

  // Печатает кадр не чаще TELEMETRY_PERIOD_MS. now — единый источник времени
  // основного цикла.
  void tick(uint32_t now, const TelemetryFrame &frame) {
    if (now - lastMs_ >= TELEMETRY_PERIOD_MS) {
      send_(frame);
      lastMs_ = now;
    }
  }

 private:
  // v/w — последняя применённая уставка, n — счётчик обработанных команд.
  void send_(const TelemetryFrame &frame) {
    serial_->print("ack ");
    serial_->print(frame.counter);
    serial_->print(" ");
    serial_->print(frame.linear);
    serial_->print(" ");
    serial_->print(frame.angular);
    // Диагностика: измеренная скорость, ШИМ и интеграл каждого колеса.
    // mL/mR = изм. мм/с, oL/oR = ШИМ ±255, iL/iR = интеграл.
    serial_->print(" mL=");
    serial_->print(frame.measured[0]);
    serial_->print(" mR=");
    serial_->print(frame.measured[1]);
    serial_->print(" oL=");
    serial_->print(frame.pwm[0]);
    serial_->print(" oR=");
    serial_->print(frame.pwm[1]);
    serial_->print(" iL=");
    serial_->print(frame.integral[0]);
    serial_->print(" iR=");
    serial_->print(frame.integral[1]);
    serial_->println();
  }

  Stream *serial_ = nullptr;
  uint32_t lastMs_ = 0;
};

// Корень композиции ядровой логики. Порядок тика повторяет прежний loop():
// привод, приём команд, кадр телеметрии, watchdog.
class Robot {
 public:
  Robot() : chassis_(encoders), commands_(chassis_) {}

  void begin() {
    chassis_.begin();
    telemetry_.begin(Serial);
  }

  void tick() {
    uint32_t now = millis();

    chassis_.tick(now);

    // Свежее время для команд и watchdog: команда могла прийти уже после
    // снятия now, а таймаут считается от текущего millis() — как раньше.
    commands_.process(Serial, millis());

    TelemetryFrame frame;
    commands_.fillTelemetry(frame);
    chassis_.fillTelemetry(frame);
    telemetry_.tick(now, frame);

    commands_.watchdogTick(millis());
  }

 private:
  Chassis chassis_;
  CommandReceiver commands_;
  Telemetry telemetry_;
};

Robot robot;

// Точки входа Robot для основного скетча: типы модулей объявлены в более
// поздних табах и в setup()/loop() недоступны.
void robotInit() {
  robot.begin();
}

void robotTick() {
  robot.tick();
}
