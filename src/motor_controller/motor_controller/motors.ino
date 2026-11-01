// --- Моторы, кинематика и PID ----------------------------------------------
// Вынесено из motor_controller.ino. Снаружи нужны только motorsInit(),
// motorsTick() и аксессоры для телеметрии. Класс Motor скрывает энкодер,
// драйвер и состояние контура скорости; в файле остаётся состояние привода
// (driveState, lastControlMs). Работу с энкодерами мотор делегирует классам
// Encoder/Encoders из encoders.ino.
//
// Arduino склеивает табы так: сначала motor_controller.ino, затем остальные по
// алфавиту. Поэтому объявления этого файла недоступны основному скетчу, а
// глобальные объявления основного скетча (например, enum MotorId) здесь видны.

#define PIN_IN1_R  2
#define PIN_IN2_R  3
#define PIN_ENA_R  5

#define PIN_IN3_L  4
#define PIN_IN4_L  7
#define PIN_ENB_L  6

const float WHEELBASE_MM = 185.0f;
const float V_MAX_MM_S = 300.0f;
const float OMEGA_MAX_RAD_S = 2.0f * V_MAX_MM_S / WHEELBASE_MM;

const uint32_t BRAKE_DURATION_MS = 300;
const uint32_t PID_PERIOD_MS = 10;

const float PID_KP = 1.5f;
const float PID_KI = 0.2f;
const float PID_KD = 0.0f;
const float PID_INTEGRAL_LIMIT = 500.0f;

// Прямая связь: компенсирует усилие, нужное мосту для поддержания скорости.
// Полная шкала ШИМ 255 ~= V_MAX мм/с, поэтому коэффициент 255/V_MAX≈0.85.
// Без неё на низких целях (напр. 30 мм/с) контур может выдать максимум
// Kp·30 + Ki·I_limit ≈ 45+40 = 85 ШИМ, а редуктор 56:1 трогается только
// c ~60 ШИМ — бот "жужжит" и не едет. FF добавляет ~0.85·setpoint сразу.
const float PID_FF = 255.0f / V_MAX_MM_S;

enum DriveState { STATE_STOPPED, STATE_RUN, STATE_BRAKING };

// Один мотор: энкодер, драйвер L298N, редуктор и контур скорости. Состояние
// контура (уставка, измеренная скорость, интегратор, последний ШИМ) закрыто;
// наружу торчат только операции и read-only геттеры для телеметрии.
class Motor {
 public:
  Motor(uint8_t pwm, uint8_t in1, uint8_t in2, int8_t sign, Encoder &enc)
      : pinPwm_(pwm), pinIn1_(in1), pinIn2_(in2), motorSign_(sign), encoder_(enc) {}

  void setTarget(float target) {
    targetLinear_ = target;
  }

  // Одна итерация контура: замер, PID и выдача ШИМ. Порядок шагов прежний —
  // сначала измерение, затем закон управления, затем мост.
  void tick(float dtSec) {
    measuredLinear_ = encoder_.measureSpeed(dtSec, motorSign_);
    lastOut_ = pidUpdate(targetLinear_, measuredLinear_, dtSec);
    applyPwm(lastOut_);
  }

  void rebaseEncoder() {
    encoder_.rebase();
  }

  void resetPid() {
    integral_ = 0.0f;
    prevError_ = 0.0f;
    encoder_.resetFilter();
  }

  void brake() {
    digitalWrite(pinIn1_, HIGH);
    digitalWrite(pinIn2_, HIGH);
    analogWrite(pinPwm_, 255);
  }

  void coast() {
    digitalWrite(pinIn1_, LOW);
    digitalWrite(pinIn2_, LOW);
    analogWrite(pinPwm_, 0);
  }

  float measuredSpeed() const {
    return measuredLinear_;
  }

  float pwm() const {
    return lastOut_;
  }

  float integralValue() const {
    return integral_;
  }

 private:
  float pidUpdate(float setpoint, float feedback, float dt) {
    float error = setpoint - feedback;
    integral_ += error * dt;
    integral_ = constrain(integral_, -PID_INTEGRAL_LIMIT, PID_INTEGRAL_LIMIT);
    float derivative = dt > 0.0f ? (error - prevError_) / dt : 0.0f;
    prevError_ = error;
    float out = PID_FF * setpoint
              + PID_KP * error
              + PID_KI * integral_
              + PID_KD * derivative;
    return constrain(out, -255.0f, 255.0f);
  }

  void applyPwm(float pwmCmd) {
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

Motor motors[2] = {
    {PIN_ENB_L, PIN_IN3_L, PIN_IN4_L, -1, encoders[MOTOR_LEFT]},
    {PIN_ENA_R, PIN_IN1_R, PIN_IN2_R, +1, encoders[MOTOR_RIGHT]}
};

DriveState driveState = STATE_STOPPED;
uint32_t brakeStartMs = 0;
uint32_t lastControlMs = 0;

void set_speed(float linearSpeed, float angularSpeed) {
  float vRight = linearSpeed + angularSpeed * WHEELBASE_MM / 2.0f;
  float vLeft = linearSpeed - angularSpeed * WHEELBASE_MM / 2.0f;

  float maxAbs = fmaxf(fabsf(vLeft), fabsf(vRight));
  if (maxAbs > V_MAX_MM_S) {
    float k = V_MAX_MM_S / maxAbs;
    vLeft *= k;
    vRight *= k;
  }

  // Вход в режим управления после паузы: перебазировать опорную точку контура,
  // чтобы первая итерация не увидела накопленные за паузу тики и время.
  // Команда поверх уже идущего STATE_RUN (heartbeat) НЕ перебазирует: иначе
  // таймер контура сбрасывался бы каждые 200 мс и controlLoop() не успевал бы
  // отработать.
  if (driveState != STATE_RUN) {
    lastControlMs = millis();
    for (int w = 0; w < 2; w++) {
      motors[w].rebaseEncoder();
    }
  }

  driveState = STATE_RUN;
  motors[MOTOR_LEFT].setTarget(vLeft);
  motors[MOTOR_RIGHT].setTarget(vRight);
}

void stop() {
  driveState = STATE_STOPPED;
  motors[MOTOR_LEFT].setTarget(0.0f);
  motors[MOTOR_RIGHT].setTarget(0.0f);
  motors[MOTOR_LEFT].coast();
  motors[MOTOR_RIGHT].coast();
  motors[MOTOR_LEFT].resetPid();
  motors[MOTOR_RIGHT].resetPid();
}

void power_stop() {
  motors[MOTOR_LEFT].setTarget(0.0f);
  motors[MOTOR_RIGHT].setTarget(0.0f);
  motors[MOTOR_LEFT].resetPid();
  motors[MOTOR_RIGHT].resetPid();
  driveState = STATE_BRAKING;
  brakeStartMs = millis();
  motors[MOTOR_LEFT].brake();
  motors[MOTOR_RIGHT].brake();
}

void controlLoop() {
  uint32_t now = millis();
  uint32_t gap = now - lastControlMs;
  if (gap < PID_PERIOD_MS) {
    return;
  }
  // Длинная итерация во время активного управления (например, блокирующая
  // печать кадра телеметрии ~78 мс) — это НЕ пауза: колесо всё это время
  // вращалось, дельта тиков настоящая. Меряем по реальному dt и ничего не
  // подменяем нулём. Настоящая пауза (торможение/останов/watchdog) видна по
  // смене driveState и обрабатывается перебазированием в set_speed().
  float dt = (float)gap / 1000.0f;
  // Дедлайн следующей итерации — от текущего момента, без захода в будущее:
  // иначе uint32_t-разность на следующей итерации переполняется и даёт
  // ложный огромный gap (и подмен нулём через старую ветку gap > 200UL).
  lastControlMs = now;

  for (int w = 0; w < 2; w++) {
    motors[w].tick(dt);
  }
}

// --- Инициализация и тик модуля --------------------------------------------

void motorsInit() {
  pinMode(PIN_ENA_R, OUTPUT);
  pinMode(PIN_IN1_R, OUTPUT);
  pinMode(PIN_IN2_R, OUTPUT);
  pinMode(PIN_ENB_L, OUTPUT);
  pinMode(PIN_IN3_L, OUTPUT);
  pinMode(PIN_IN4_L, OUTPUT);

  motors[MOTOR_LEFT].coast();
  motors[MOTOR_RIGHT].coast();
  driveState = STATE_STOPPED;

  encoders.begin();
}

// Один тик модуля из loop(): завершает торможение по таймеру и, если привод
// в режиме RUN, выполняет итерацию PID.
void motorsTick(uint32_t now) {
  if (driveState == STATE_BRAKING && now - brakeStartMs >= BRAKE_DURATION_MS) {
    driveState = STATE_STOPPED;
    motors[MOTOR_LEFT].coast();
    motors[MOTOR_RIGHT].coast();
  }

  if (driveState == STATE_RUN) {
    controlLoop();
  }
}

// --- Аксессоры для телеметрии ----------------------------------------------

float motorMeasuredSpeed(uint8_t motor) {
  return motors[motor].measuredSpeed();
}

float motorPwm(uint8_t motor) {
  return motors[motor].pwm();
}

float motorIntegral(uint8_t motor) {
  return motors[motor].integralValue();
}
