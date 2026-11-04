// --- Привод целиком: реализация ---------------------------------------------

#include "chassis.h"

#include <math.h>

#include "drive_config.h"

namespace {

const uint8_t kPinIn1R = 2;
const uint8_t kPinIn2R = 3;
const uint8_t kPinEnaR = 5;

const uint8_t kPinIn3L = 4;
const uint8_t kPinIn4L = 7;
const uint8_t kPinEnbL = 6;

const float kOmegaMaxRadS = 2.0f * kVMaxMmS / kWheelbaseMm;

const uint8_t kLeftMotor = 0;
const uint8_t kRightMotor = 1;

const uint32_t kBrakeDurationMs = 300;
const uint32_t kPidPeriodMs = 10;

}  // namespace

Chassis::Chassis(Encoders &encoders)
    : encoders_(encoders),
      motors_{
          {kPinEnbL, kPinIn3L, kPinIn4L, -1, encoders.left()},
          {kPinEnaR, kPinIn1R, kPinIn2R, +1, encoders.right()}} {}

void Chassis::init() {
  pinMode(kPinEnaR, OUTPUT);
  pinMode(kPinIn1R, OUTPUT);
  pinMode(kPinIn2R, OUTPUT);
  pinMode(kPinEnbL, OUTPUT);
  pinMode(kPinIn3L, OUTPUT);
  pinMode(kPinIn4L, OUTPUT);

  motors_[kLeftMotor].coast();
  motors_[kRightMotor].coast();
  state_ = DriveState::Stopped;

  encoders_.init();
}

void Chassis::tick(uint32_t now) {
  if (state_ == DriveState::Braking &&
      now - brakeStartMs_ >= kBrakeDurationMs) {
    state_ = DriveState::Stopped;
    motors_[kLeftMotor].coast();
    motors_[kRightMotor].coast();
  }

  if (state_ == DriveState::Run) {
    controlLoop_();
  }
}

void Chassis::set_speed(float linearSpeed, float angularSpeed) {
  float vRight = linearSpeed + angularSpeed * kWheelbaseMm / 2.0f;
  float vLeft = linearSpeed - angularSpeed * kWheelbaseMm / 2.0f;

  float maxAbs = fmaxf(fabsf(vLeft), fabsf(vRight));
  if (maxAbs > kVMaxMmS) {
    float k = kVMaxMmS / maxAbs;
    vLeft *= k;
    vRight *= k;
  }

  // Вход в режим управления после паузы: перебазировать опорную точку
  // контура, чтобы первая итерация не увидела накопленные за паузу тики и
  // время. Команда поверх уже идущего Run (heartbeat) НЕ перебазирует:
  // иначе таймер контура сбрасывался бы каждые 200 мс и controlLoop_() не
  // успевал бы отработать.
  if (state_ != DriveState::Run) {
    lastControlMs_ = millis();
    for (uint8_t i = 0; i < kMotorCount; i++) {
      motors_[i].rebaseEncoder();
    }
  }

  state_ = DriveState::Run;
  motors_[kLeftMotor].setTarget(vLeft);
  motors_[kRightMotor].setTarget(vRight);
}

void Chassis::stop() {
  state_ = DriveState::Stopped;
  motors_[kLeftMotor].setTarget(0.0f);
  motors_[kRightMotor].setTarget(0.0f);
  motors_[kLeftMotor].coast();
  motors_[kRightMotor].coast();
  motors_[kLeftMotor].resetPid();
  motors_[kRightMotor].resetPid();
}

void Chassis::power_stop() {
  motors_[kLeftMotor].setTarget(0.0f);
  motors_[kRightMotor].setTarget(0.0f);
  motors_[kLeftMotor].resetPid();
  motors_[kRightMotor].resetPid();
  state_ = DriveState::Braking;
  brakeStartMs_ = millis();
  motors_[kLeftMotor].brake();
  motors_[kRightMotor].brake();
}

void Chassis::fillTelemetry(TelemetryFrame &frame) const {
  for (uint8_t i = 0; i < kMotorCount; i++) {
    MotorTelemetry t = motors_[i].telemetry();
    frame.measured[i] = t.measuredSpeed;
    frame.pwm[i] = t.pwm;
    frame.integral[i] = t.integral;
  }
}

void Chassis::controlLoop_() {
  uint32_t now = millis();
  uint32_t gap = now - lastControlMs_;
  if (gap < kPidPeriodMs) {
    return;
  }
  // Длинная итерация во время активного управления (например, блокирующая
  // печать кадра телеметрии ~78 мс) — это НЕ пауза: колесо всё это время
  // вращалось, дельта тиков настоящая. Меряем по реальному dt и ничего не
  // подменяем нулём. Настоящая пауза (торможение/останов/watchdog) видна по
  // смене состояния и обрабатывается перебазированием в set_speed().
  float dt = (float)gap / 1000.0f;
  // Дедлайн следующей итерации — от текущего момента, без захода в будущее:
  // иначе uint32_t-разность на следующей итерации переполняется и даёт
  // ложный огромный gap.
  lastControlMs_ = now;

  for (uint8_t i = 0; i < kMotorCount; i++) {
    motors_[i].tick(dt);
  }
}
