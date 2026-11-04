// --- Мотор, PID: реализация -------------------------------------------------

#include "motor.h"

#include <math.h>

#include "drive_config.h"

namespace {

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
