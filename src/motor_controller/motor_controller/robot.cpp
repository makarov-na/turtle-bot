// --- Композиция модулей: реализация ----------------------------------------
// Порядок тика: привод, приём команд, кадр телеметрии, watchdog.
// TelemetryFrame и Telemetry — в telemetry.h/.cpp, CommandReceiver — в
// command_receiver.h/.cpp, автоконфигурация HC-06 запускается отдельно из
// setup().

#include "robot.h"

Robot::Robot(Encoders &encoders) : chassis_(encoders) {}

void Robot::begin() {
  chassis_.begin();
  telemetry_.begin(Serial);
}

void Robot::tick() {
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
