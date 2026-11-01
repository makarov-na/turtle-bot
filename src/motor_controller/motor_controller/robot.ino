// --- Композиция модулей ----------------------------------------------------
// Robot — корень композиции ядровой логики: владеет приводом, приёмником
// команд и телеметрией, связывает их зависимости и повторяет прежний порядок
// работы loop(). TelemetryFrame и Telemetry — в telemetry.h/.cpp, а
// автоконфигурация HC-06 запускается отдельно из setup().
//
// Порядок тика: привод, приём команд, кадр телеметрии, watchdog.
//
// Собственной логики здесь нет: только сборка модулей и две точки входа для
// основного скетча.

#include "telemetry.h"

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

// Точки входа Robot для основного скетча.
void robotInit() {
  robot.begin();
}

void robotTick() {
  robot.tick();
}
