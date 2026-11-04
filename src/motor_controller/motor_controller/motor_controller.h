// --- Блок управления приводом: интерфейс -----------------------------------
// MotorController — корень композиции ядровой логики: связывает привод,
// приёмник команд и телеметрию, повторяя прежний порядок работы loop().
// CommandReceiver только разбирает протокол и сообщает о срабатывании
// watchdog; применение команд к Chassis — в motor_controller.cpp.
//
// Экземпляр MotorController создаётся в основном скетче последним и получает
// ссылки на уже созданные Chassis, CommandReceiver и Telemetry.

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

#include "src/protocol/command_receiver.h"
#include "src/chassis/chassis.h"
#include "src/telemetry/telemetry.h"

class MotorController {
 public:
  MotorController(Chassis &chassis, CommandReceiver &commands,
                  Telemetry &telemetry);

  void init();
  void tick();

 private:
  Chassis &chassis_;
  CommandReceiver &commands_;
  Telemetry &telemetry_;
};

#endif
