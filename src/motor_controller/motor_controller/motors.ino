// --- Моторы, энкодеры, кинематика и PID ------------------------------------
// Вынесено из motor_controller.ino. Снаружи нужны только motorsInit(),
// motorsTick() и аксессоры для телеметрии; состояние колёс (Wheel, driveState,
// lastControlMs) остаётся в этом файле.
//
// Arduino склеивает табы так: сначала motor_controller.ino, затем остальные по
// алфавиту. Поэтому объявления этого файла недоступны основному скетчу, а
// глобальные объявления основного скетча (например, enum WheelId) здесь видны.

#define PIN_IN1_R  2
#define PIN_IN2_R  3
#define PIN_ENA_R  5
#define PIN_ENC_R_A A0
#define PIN_ENC_R_B A1

#define PIN_IN3_L  4
#define PIN_IN4_L  7
#define PIN_ENB_L  6
#define PIN_ENC_L_A A2
#define PIN_ENC_L_B A3

const float WHEEL_DIAMETER_MM = 67.0f;
const float WHEELBASE_MM = 185.0f;
const float V_MAX_MM_S = 300.0f;
const float OMEGA_MAX_RAD_S = 2.0f * V_MAX_MM_S / WHEELBASE_MM;

const uint32_t BRAKE_DURATION_MS = 300;
const uint32_t PID_PERIOD_MS = 10;
const uint16_t MIN_TICK_US = 50;

const uint16_t ENCODER_CPR  = 11;
const uint8_t  GEAR_RATIO   = 56;
const uint32_t TICKS_PER_WHEEL_REV = ENCODER_CPR * 4UL * GEAR_RATIO;

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

// Постоянная времени сглаживания измеренной скорости (EMA). При периоде
// контура 10 мс один тик энкодера — это ~8.5 мм/с, а Kp=1.5 превращает
// квантование в ±10 ШИМ прямо на моторе (видимая неравномерность). EMA с
// τ=60 мс режет шум примерно в 3.6 раза, оставляя контур достаточно быстрым.
const float SPEED_FILTER_TAU_S = 0.06f;

// Диагноз (контролируемый тест 2026-09-12): при вращении колеса "вперёд"
// руками оба энкодера показывали ОТРИЦАТЕЛЬНУЮ скорость (mL=mR=-4..-500
// мм/с) — контур работал в положительную обратную связь (моторы вылетали
// в oL=oR=255 в ту же сторону). motorSign(-1/+1) калиброваны под старую
// разводку D8-D11; после переезда на Port C (A0-A3) каналы A/B на обоих
// колёсах обменялись, что инвертирует подсчёт. Фикс: инвертировать таблицу
// (эквивалентно смене мест проводов A/B на обоих энкодерах) — это флипнет
// знак СЧЁТА, не тронув motorSign и направление моторов.
static const int8_t QUAD_TABLE[16] = {
    0, 1, -1, 0,
    -1, 0, 0, 1,
    1, 0, 0, -1,
    0, -1, 1, 0
};

enum DriveState { STATE_STOPPED, STATE_RUN, STATE_BRAKING };

struct Wheel {
  const uint8_t pinPwm;
  const uint8_t pinIn1;
  const uint8_t pinIn2;
  const int8_t motorSign;

  Wheel(uint8_t pwm, uint8_t in1, uint8_t in2, int8_t sign)
      : pinPwm(pwm), pinIn1(in1), pinIn2(in2), motorSign(sign) {}

  volatile int32_t ticks = 0;
  int32_t prevTicks = 0;

  float targetLinear = 0.0f;
  float measuredLinear = 0.0f;

  float integral = 0.0f;
  float prevError = 0.0f;
  float lastOut = 0.0f;
  bool speedFilterInit = false;

  void setTarget(float target) {
    targetLinear = target;
  }

  void measureSpeed(float dtSec) {
    int32_t delta = ticks - prevTicks;
    prevTicks = ticks;
    float mmPerTick = PI * WHEEL_DIAMETER_MM / (float)TICKS_PER_WHEEL_REV;
    float raw = (float)delta * mmPerTick * (float)motorSign / dtSec;
    // EMA: единичный отсчёт за такт квантуется шагом ~8.5 мм/с, и ПИД с
    // Kp=1.5 гоняется за этим шумом. Сглаживаем, чтобы ШИМ не дрожал.
    if (!speedFilterInit) {
      measuredLinear = raw;
      speedFilterInit = true;
    } else {
      float alpha = dtSec / (SPEED_FILTER_TAU_S + dtSec);
      measuredLinear += alpha * (raw - measuredLinear);
    }
  }

  float pidUpdate(float setpoint, float feedback, float dt) {
    float error = setpoint - feedback;
    integral += error * dt;
    integral = constrain(integral, -PID_INTEGRAL_LIMIT, PID_INTEGRAL_LIMIT);
    float derivative = dt > 0.0f ? (error - prevError) / dt : 0.0f;
    prevError = error;
    float out = PID_FF * setpoint
              + PID_KP * error
              + PID_KI * integral
              + PID_KD * derivative;
    return constrain(out, -255.0f, 255.0f);
  }

  void resetPid() {
    integral = 0.0f;
    prevError = 0.0f;
    speedFilterInit = false;
  }

  void applyPwm(float pwmCmd) {
    bool robotForward = pwmCmd >= 0.0f;
    bool motorForward = (motorSign >= 0) ? robotForward : !robotForward;
    if (motorForward) {
      digitalWrite(pinIn1, LOW);
      digitalWrite(pinIn2, HIGH);
    } else {
      digitalWrite(pinIn1, HIGH);
      digitalWrite(pinIn2, LOW);
    }
    analogWrite(pinPwm, (int)constrain(fabsf(pwmCmd), 0.0f, 255.0f));
  }

  void brake() {
    digitalWrite(pinIn1, HIGH);
    digitalWrite(pinIn2, HIGH);
    analogWrite(pinPwm, 255);
  }

  void coast() {
    digitalWrite(pinIn1, LOW);
    digitalWrite(pinIn2, LOW);
    analogWrite(pinPwm, 0);
  }
};

Wheel wheels[2] = {
    {PIN_ENB_L, PIN_IN3_L, PIN_IN4_L, -1},
    {PIN_ENA_R, PIN_IN1_R, PIN_IN2_R, +1}
};

DriveState driveState = STATE_STOPPED;
uint32_t brakeStartMs = 0;
uint32_t lastControlMs = 0;

static uint8_t prevEncState[2] = {0, 0};
static uint32_t lastTickMicros[2] = {0, 0};

void updateEncoder(int wheel, uint8_t state) {
  uint8_t idx = (prevEncState[wheel] << 2) | state;
  int8_t step = QUAD_TABLE[idx];
  if (step != 0) {
    prevEncState[wheel] = state;
    uint32_t now = micros();
    if (now - lastTickMicros[wheel] >= MIN_TICK_US) {
      lastTickMicros[wheel] = now;
      wheels[wheel].ticks += step;
    }
  }
}

ISR(PCINT1_vect) {
  uint8_t pins = PINC & 0x0F;
  updateEncoder(WHEEL_LEFT, (pins >> 2) & 0x03);
  updateEncoder(WHEEL_RIGHT, pins & 0x03);
}

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
      wheels[w].prevTicks = wheels[w].ticks;
      wheels[w].speedFilterInit = false;
    }
  }

  driveState = STATE_RUN;
  wheels[WHEEL_LEFT].setTarget(vLeft);
  wheels[WHEEL_RIGHT].setTarget(vRight);
}

void stop() {
  driveState = STATE_STOPPED;
  wheels[WHEEL_LEFT].setTarget(0.0f);
  wheels[WHEEL_RIGHT].setTarget(0.0f);
  wheels[WHEEL_LEFT].coast();
  wheels[WHEEL_RIGHT].coast();
  wheels[WHEEL_LEFT].resetPid();
  wheels[WHEEL_RIGHT].resetPid();
}

void power_stop() {
  wheels[WHEEL_LEFT].setTarget(0.0f);
  wheels[WHEEL_RIGHT].setTarget(0.0f);
  wheels[WHEEL_LEFT].resetPid();
  wheels[WHEEL_RIGHT].resetPid();
  driveState = STATE_BRAKING;
  brakeStartMs = millis();
  wheels[WHEEL_LEFT].brake();
  wheels[WHEEL_RIGHT].brake();
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
    Wheel &wh = wheels[w];
    wh.measureSpeed(dt);
    float out = wh.pidUpdate(wh.targetLinear, wh.measuredLinear, dt);
    wh.lastOut = out;
    wh.applyPwm(out);
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

  pinMode(PIN_ENC_L_A, INPUT_PULLUP);
  pinMode(PIN_ENC_L_B, INPUT_PULLUP);
  pinMode(PIN_ENC_R_A, INPUT_PULLUP);
  pinMode(PIN_ENC_R_B, INPUT_PULLUP);

  wheels[WHEEL_LEFT].coast();
  wheels[WHEEL_RIGHT].coast();
  driveState = STATE_STOPPED;

  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11);
}

// Один тик модуля из loop(): завершает торможение по таймеру и, если привод
// в режиме RUN, выполняет итерацию PID.
void motorsTick(uint32_t now) {
  if (driveState == STATE_BRAKING && now - brakeStartMs >= BRAKE_DURATION_MS) {
    driveState = STATE_STOPPED;
    wheels[WHEEL_LEFT].coast();
    wheels[WHEEL_RIGHT].coast();
  }

  if (driveState == STATE_RUN) {
    controlLoop();
  }
}

// --- Аксессоры для телеметрии ----------------------------------------------

float wheelMeasuredSpeed(uint8_t wheel) {
  return wheels[wheel].measuredLinear;
}

float wheelPwm(uint8_t wheel) {
  return wheels[wheel].lastOut;
}

float wheelIntegral(uint8_t wheel) {
  return wheels[wheel].integral;
}
