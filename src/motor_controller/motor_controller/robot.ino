// --- Композиция модулей ----------------------------------------------------
// Robot — корень композиции ядровой логики: владеет приводом, приёмником
// команд и телеметрией, связывает их зависимости и повторяет прежний порядок
// работы loop(). CommandReceiver только разбирает протокол и сообщает о
// срабатывании watchdog; применение команд к Chassis — здесь.
// TelemetryFrame и Telemetry — в telemetry.h/.cpp, а автоконфигурация HC-06
// запускается отдельно из setup().
//
// Порядок тика: привод, приём команд, кадр телеметрии, watchdog.

#include "command_receiver.h"
#include "telemetry.h"

class Robot {
 public:
  Robot() : chassis_(encoders) {}

  void begin() {
    chassis_.begin();
    telemetry_.begin(Serial);
  }

  void tick() {
    uint32_t now = millis();

    chassis_.tick(now);

    // Свежее время для команд: они могли прийти уже после снятия now.
    uint32_t cmdNow = millis();
    CommandReceiver::Command cmd;
    while (commands_.poll(Serial, cmdNow, cmd)) {
      if (cmd.action == CommandReceiver::Action::PowerStop) {
        chassis_.power_stop();
      } else {
        chassis_.set_speed(cmd.linear, cmd.angular);
      }
    }

    TelemetryFrame frame;
    frame.counter = commands_.counter();
    frame.linear = commands_.lastLinear();
    frame.angular = commands_.lastAngular();
    chassis_.fillTelemetry(frame);
    telemetry_.tick(now, frame);

    // Watchdog — после телеметрии, как раньше: кадр уходит до остановки.
    if (commands_.watchdogExpired(millis())) {
      chassis_.stop();
    }
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
