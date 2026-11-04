// --- Блок управления приводом: реализация ----------------------------------
// Порядок тика: привод, приём команд, кадр телеметрии, watchdog.
// MotorController не владеет модулями: ссылки на Chassis, CommandReceiver и
// Telemetry приходят из основного скетча.

#include "motor_controller.h"

MotorController::MotorController(Chassis &chassis, CommandReceiver &commands,
                                 Telemetry &telemetry)
    : chassis_(chassis), commands_(commands), telemetry_(telemetry) {}

void MotorController::init() {
  chassis_.init();
}

void MotorController::tick() {
  uint32_t now = millis();

  chassis_.tick(now);

  // Свежее время для команд: они могли прийти уже после снятия now.
  uint32_t cmdNow = millis();
  CommandReceiver::Command cmd;
  while (commands_.poll(cmdNow, cmd)) {
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
