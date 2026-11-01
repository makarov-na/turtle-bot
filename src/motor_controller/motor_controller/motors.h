// --- Моторы, кинематика и PID: интерфейс -----------------------------------
// Motor скрывает энкодер, драйвер и состояние контура скорости; Chassis
// владеет двумя моторами, режимом привода, таймерами, насыщением уставок и
// остановками. Снаружи Chassis предоставляет set_speed/stop/power_stop и
// снимок для телеметрии.
//
// Реализация — в motors.cpp. Экземпляр Chassis создаёт Robot.

#ifndef MOTORS_H
#define MOTORS_H

#include <Arduino.h>

#include "encoders.h"
#include "telemetry.h"

// Снимок состояния одного мотора для телеметрии. PID-внутренности остаются
// закрытыми, наружу отдаётся только то, что требует протокол ack.
struct MotorTelemetry {
  float measuredSpeed;
  float pwm;
  float integral;
};

class Chassis;

// Один мотор: энкодер, драйвер L298N, редуктор и контур скорости. Состояние
// контура закрыто; менять его может только Chassis.
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

// Привод целиком: два мотора, режим (stopped/run/braking) и таймеры.
// Инварианты режима и насыщение уставок живут только здесь.
class Chassis {
 public:
  explicit Chassis(Encoders &encoders);

  void begin();

  // Один тик привода из Robot::tick(): завершает торможение по таймеру и,
  // если привод в режиме Run, выполняет итерацию PID.
  void tick(uint32_t now);

  void set_speed(float linearSpeed, float angularSpeed);
  void stop();
  void power_stop();

  MotorTelemetry telemetry(MotorId id) const;
  void fillTelemetry(TelemetryFrame &frame) const;

 private:
  enum class DriveState : uint8_t { Stopped, Run, Braking };

  static constexpr uint8_t kMotorCount = 2;

  static uint8_t index(MotorId id);

  void controlLoop_();

  Encoders &encoders_;
  Motor motors_[2];
  DriveState state_ = DriveState::Stopped;
  uint32_t brakeStartMs_ = 0;
  uint32_t lastControlMs_ = 0;
};

#endif
