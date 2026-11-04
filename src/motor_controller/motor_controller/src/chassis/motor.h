// --- Мотор: интерфейс -------------------------------------------------------
// Motor скрывает энкодер, драйвер L298N, редуктор и состояние контура
// скорости. Состояние контура закрыто; менять его может только Chassis.
//
// Реализация — в motor.cpp.

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

#include "encoders.h"

// Снимок состояния одного мотора для телеметрии. PID-внутренности остаются
// закрытыми, наружу отдаётся только то, что требует протокол ack.
struct MotorTelemetry {
  float measuredSpeed;
  float pwm;
  float integral;
};

class Motor {
 public:
  Motor(uint8_t pwm, uint8_t in1, uint8_t in2, int8_t sign, Encoder &enc);
  Motor(const Motor &) = delete;
  Motor &operator=(const Motor &) = delete;

  MotorTelemetry telemetry() const;

 private:
  friend class Chassis;

  void setTarget(float target);
  void tick(float dtSec);
  void rebaseEncoder();
  void resetPid();
  void brake();
  void coast();
  float pidUpdate(float setpoint, float feedback, float dt);
  void applyPwm(float pwmCmd);

  const uint8_t pinPwm_;
  const uint8_t pinIn1_;
  const uint8_t pinIn2_;
  const int8_t motorSign_;
  Encoder &encoder_;

  float targetLinear_ = 0.0f;
  float measuredLinear_ = 0.0f;
  float integral_ = 0.0f;
  float prevError_ = 0.0f;
  float lastOut_ = 0.0f;
};

#endif
