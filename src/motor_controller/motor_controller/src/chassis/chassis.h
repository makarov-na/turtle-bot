// --- Привод целиком: интерфейс ----------------------------------------------
// Chassis владеет двумя моторами, режимом привода, таймерами, насыщением
// уставок и остановками. Снаружи предоставляет set_speed/stop/power_stop и
// снимок для телеметрии.
//
// Реализация — в chassis.cpp. Экземпляр Chassis создаётся в основном скетче.

#ifndef CHASSIS_H
#define CHASSIS_H

#include <Arduino.h>

#include "encoders.h"
#include "motor.h"
#include "../telemetry/telemetry.h"

class Chassis {
 public:
  explicit Chassis(Encoders &encoders);

  void init();

  // Один тик привода из MotorController::tick(): завершает торможение по
  // таймеру и, если привод в режиме Run, выполняет итерацию PID.
  void tick(uint32_t now);

  void set_speed(float linearSpeed, float angularSpeed);
  void stop();
  void power_stop();

  void fillTelemetry(TelemetryFrame &frame) const;

 private:
  enum class DriveState : uint8_t { Stopped, Run, Braking };

  static constexpr uint8_t kMotorCount = 2;

  void controlLoop_();

  Encoders &encoders_;
  Motor motors_[2];
  DriveState state_ = DriveState::Stopped;
  uint32_t brakeStartMs_ = 0;
  uint32_t lastControlMs_ = 0;
};

#endif
