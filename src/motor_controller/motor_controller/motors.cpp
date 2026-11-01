// --- Моторы, кинематика и PID: реализация ----------------------------------

#include "motors.h"

#include <math.h>

namespace {

const uint8_t kPinIn1R = 2;
const uint8_t kPinIn2R = 3;
const uint8_t kPinEnaR = 5;

const uint8_t kPinIn3L = 4;
const uint8_t kPinIn4L = 7;
const uint8_t kPinEnbL = 6;

const float kWheelbaseMm = 185.0f;
const float kVMaxMmS = 300.0f;
const float kOmegaMaxRadS = 2.0f * kVMaxMmS / kWheelbaseMm;

const uint32_t kBrakeDurationMs = 300;
const uint32_t kPidPeriodMs = 10;

const float kPidKp = 1.5f;
const float kPidKi = 0.2f;
const float kPidKd = 0.0f;
const float kPidIntegralLimit = 500.0f;

// Прямая связь: компенсирует усилие, нужное мосту для поддержания скорости.
// Полная шкала ШИМ 255 ~= V_MAX мм/с, поэтому коэффициент 255/V_MAX≈0.85.
// Без неё на низких целях (напр. 30 мм/с) контур может выдать максимум
// Kp·30 + Ki·I_limit ≈ 45+40 = 85 ШИМ, а редуктор 56:1 трогается только
// c ~60 ШИМ — бот "жужжит" и не едет. FF добавляет ~0.85·setpoint сразу.
const float kPidFf = 255.0f / kVMaxMmS;

}  // namespace

Motor::Motor(uint8_t pwm, uint8_t in1, uint8_t in2, int8_t sign, Encoder &enc)
    : pinPwm_(pwm), pinIn1_(in1), pinIn2_(in2), motorSign_(sign), encoder_(enc) {}

MotorTelemetry Motor::telemetry() const {
  return MotorTelemetry{measuredLinear_, lastOut_, integral_};
}

void Motor::setTarget(float target) {
  targetLinear_ = target;
}

// Одна итерация контура: замер, PID и выдача ШИМ. Порядок шагов прежний —
// сначала измерение, затем закон управления, затем мост.
void Motor::tick(float dtSec) {
  measuredLinear_ = encoder_.measureSpeed(dtSec, motorSign_);
  lastOut_ = pidUpdate(targetLinear_, measuredLinear_, dtSec);
  applyPwm(lastOut_);
}

void Motor::rebaseEncoder() {
  encoder_.rebase();
}

void Motor::resetPid() {
  integral_ = 0.0f;
  prevError_ = 0.0f;
  encoder_.resetFilter();
}

void Motor::brake() {
  digitalWrite(pinIn1_, HIGH);
  digitalWrite(pinIn2_, HIGH);
  analogWrite(pinPwm_, 255);
}

void Motor::coast() {
  digitalWrite(pinIn1_, LOW);
  digitalWrite(pinIn2_, LOW);
  analogWrite(pinPwm_, 0);
}

float Motor::pidUpdate(float setpoint, float feedback, float dt) {
  float error = setpoint - feedback;
  integral_ += error * dt;
  integral_ = constrain(integral_, -kPidIntegralLimit, kPidIntegralLimit);
  float derivative = dt > 0.0f ? (error - prevError_) / dt : 0.0f;
  prevError_ = error;
  float out = kPidFf * setpoint
            + kPidKp * error
            + kPidKi * integral_
            + kPidKd * derivative;
  return constrain(out, -255.0f, 255.0f);
}

void Motor::applyPwm(float pwmCmd) {
  bool robotForward = pwmCmd >= 0.0f;
  bool motorForward = (motorSign_ >= 0) ? robotForward : !robotForward;
  if (motorForward) {
    digitalWrite(pinIn1_, LOW);
    digitalWrite(pinIn2_, HIGH);
  } else {
    digitalWrite(pinIn1_, HIGH);
    digitalWrite(pinIn2_, LOW);
  }
  analogWrite(pinPwm_, (int)constrain(fabsf(pwmCmd), 0.0f, 255.0f));
}

Chassis::Chassis(Encoders &encoders)
    : encoders_(encoders),
      motors_{
          {kPinEnbL, kPinIn3L, kPinIn4L, -1, encoders[MotorId::Left]},
          {kPinEnaR, kPinIn1R, kPinIn2R, +1, encoders[MotorId::Right]}} {}

void Chassis::begin() {
  pinMode(kPinEnaR, OUTPUT);
  pinMode(kPinIn1R, OUTPUT);
  pinMode(kPinIn2R, OUTPUT);
  pinMode(kPinEnbL, OUTPUT);
  pinMode(kPinIn3L, OUTPUT);
  pinMode(kPinIn4L, OUTPUT);

  motors_[index(MotorId::Left)].coast();
  motors_[index(MotorId::Right)].coast();
  state_ = DriveState::Stopped;

  encoders_.begin();
}

void Chassis::tick(uint32_t now) {
  if (state_ == DriveState::Braking &&
      now - brakeStartMs_ >= kBrakeDurationMs) {
    state_ = DriveState::Stopped;
    motors_[index(MotorId::Left)].coast();
    motors_[index(MotorId::Right)].coast();
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
  motors_[index(MotorId::Left)].setTarget(vLeft);
  motors_[index(MotorId::Right)].setTarget(vRight);
}

void Chassis::stop() {
  state_ = DriveState::Stopped;
  motors_[index(MotorId::Left)].setTarget(0.0f);
  motors_[index(MotorId::Right)].setTarget(0.0f);
  motors_[index(MotorId::Left)].coast();
  motors_[index(MotorId::Right)].coast();
  motors_[index(MotorId::Left)].resetPid();
  motors_[index(MotorId::Right)].resetPid();
}

void Chassis::power_stop() {
  motors_[index(MotorId::Left)].setTarget(0.0f);
  motors_[index(MotorId::Right)].setTarget(0.0f);
  motors_[index(MotorId::Left)].resetPid();
  motors_[index(MotorId::Right)].resetPid();
  state_ = DriveState::Braking;
  brakeStartMs_ = millis();
  motors_[index(MotorId::Left)].brake();
  motors_[index(MotorId::Right)].brake();
}

MotorTelemetry Chassis::telemetry(MotorId id) const {
  return motors_[static_cast<uint8_t>(id)].telemetry();
}

void Chassis::fillTelemetry(TelemetryFrame &frame) const {
  for (uint8_t i = 0; i < kMotorCount; i++) {
    MotorTelemetry t = motors_[i].telemetry();
    frame.measured[i] = t.measuredSpeed;
    frame.pwm[i] = t.pwm;
    frame.integral[i] = t.integral;
  }
}

uint8_t Chassis::index(MotorId id) {
  return static_cast<uint8_t>(id);
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
