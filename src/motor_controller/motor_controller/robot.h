// --- Композиция модулей: интерфейс -----------------------------------------
// Robot — корень композиции ядровой логики: владеет приводом, приёмником
// команд и телеметрией, связывает их зависимости и повторяет прежний порядок
// работы loop(). CommandReceiver только разбирает протокол и сообщает о
// срабатывании watchdog; применение команд к Chassis — в robot.cpp.
//
// Экземпляр Robot создаётся в основном скетче и получает ссылку на encoders.

#ifndef ROBOT_H
#define ROBOT_H

#include <Arduino.h>

#include "command_receiver.h"
#include "motors.h"
#include "telemetry.h"

class Robot {
 public:
  explicit Robot(Encoders &encoders);

  void begin();
  void tick();

 private:
  Chassis chassis_;
  CommandReceiver commands_;
  Telemetry telemetry_;
};

#endif
