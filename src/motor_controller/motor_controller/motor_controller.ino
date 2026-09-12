// Блок управления приводом
// Arduino Nano + L298N + моторы с энкодерами
//
// Команды в теле робота:
//   set_speed(linear_speed, angular_speed)
//     linear_speed  — линейная скорость середины оси колёс, мм/с
//     angular_speed — угловая скорость, рад/с; + = против часовой (вид сверху)
//   stop()       — отключить драйвер, свободный выбег
//   power_stop() — силовое торможение (L298N fast motor stop)
//
// Подключение и направления моторов сверять с
// docs/4. Блок управления приводом/spec.md
//
// HC-06 Bluetooth: D0 (RX), D1 (TX), 9600 baud, Hardware Serial
//   HC-06 TX → D0 (напрямую, 3.3V читается как HIGH на 5V-логике)
//   HC-06 RX ← D1 (через делитель 5V → 3.3V, RX HC-06 не 5V-tolerant)
//   ВНИМАНИЕ: при перепрошивке через USB отключать HC-06 (загрузчик
//   использует D0/D1). SoftwareSerial не используется — его PCINT-алиасы
//   конфликтуют с ISR(PCINT1_vect) энкодеров на ATmega328P.
//
// Протокол управления по каналу (текст, \n — конец команды):
//   set_speed <linear_speed> <angular_speed>
//     linear_speed  — мм/с, любой знак
//     angular_speed — рад/с, любой знак
// Команды принимаются всегда. Watchdog: если команда не получена более
// WATCHDOG_TIMEOUT_MS, бот выполняет stop() до прихода новой команды.

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

#define BT_BAUD 9600

const float WHEEL_DIAMETER_MM = 67.0f;
const float WHEELBASE_MM = 185.0f;
const float V_MAX_MM_S = 300.0f;
const float OMEGA_MAX_RAD_S = 2.0f * V_MAX_MM_S / WHEELBASE_MM;

const uint32_t RAMP_DURATION_MS = 1000;
const uint32_t BRAKE_DURATION_MS = 300;
const uint32_t PID_PERIOD_MS = 10;
const uint16_t MIN_TICK_US = 50;
const uint32_t WATCHDOG_TIMEOUT_MS = 1000;

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

enum WheelId { WHEEL_LEFT = 0, WHEEL_RIGHT = 1 };

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
  float rampedLinear = 0.0f;
  float measuredLinear = 0.0f;
  float rampFrom = 0.0f;
  uint32_t rampStartMs = 0;
  bool ramping = false;

  float integral = 0.0f;
  float prevError = 0.0f;
  float lastOut = 0.0f;

  void setTarget(float target) {
    rampFrom = rampedLinear;
    rampStartMs = millis();
    ramping = true;
    targetLinear = target;
  }

  void updateRamp(uint32_t now) {
    if (!ramping) {
      return;
    }
    float t = (float)(now - rampStartMs) / (float)RAMP_DURATION_MS;
    if (t >= 1.0f) {
      rampedLinear = targetLinear;
      ramping = false;
    } else {
      rampedLinear = rampFrom + (targetLinear - rampFrom) * t;
    }
  }

  void measureSpeed(float dtSec) {
    int32_t delta = ticks - prevTicks;
    prevTicks = ticks;
    float mmPerTick = PI * WHEEL_DIAMETER_MM / (float)TICKS_PER_WHEEL_REV;
    measuredLinear = (float)delta * mmPerTick * (float)motorSign / dtSec;
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

const uint8_t CMD_BUF_SIZE = 40;
char cmdBuf[CMD_BUF_SIZE];
uint8_t cmdLen = 0;
bool cmdOverflow = false;

uint32_t lastCommandMs = 0;
bool watchdogTripped = true;

uint32_t cmdCounter = 0;

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
  // Если контур долго не работал (пауза, торможение, watchdog), первая
  // итерация после возобновления не должна "видеть" накопленное с тех пор:
  // иначе dt и дельта тиков дадут гигантский фейковый интеграл/скорость
  // (наблюдалось iL=±296 и "колёса сами крутятся после паузы"). Сбрасываем
  // точку отсчёта и берём штатные 10 мс.
  if (gap > 200UL) {
    for (int w = 0; w < 2; w++) {
      wheels[w].prevTicks = wheels[w].ticks;
    }
    gap = PID_PERIOD_MS;
  }
  float dt = (float)gap / 1000.0f;
  lastControlMs = now + (gap - PID_PERIOD_MS);

  for (int w = 0; w < 2; w++) {
    Wheel &wh = wheels[w];
    wh.updateRamp(now);
    wh.measureSpeed(dt);
    float out = wh.pidUpdate(wh.rampedLinear, wh.measuredLinear, dt);
    wh.lastOut = out;
    wh.applyPwm(out);
  }
}

void handleCommand(char *buf) {
  // Парсим вручную: scanf-вариант из avr-libc по умолчанию не умеет %f
  // (нужна принудительная линковка libscanf_flt), а strtof доступен всегда.
  char *saveptr = nullptr;
  char *tok = strtok_r(buf, " ", &saveptr);
  if (!tok || strcmp(tok, "set_speed") != 0) {
    return;
  }
  char *vTok = strtok_r(nullptr, " ", &saveptr);
  char *wTok = strtok_r(nullptr, " ", &saveptr);
  char *extra = strtok_r(nullptr, " ", &saveptr);
  if (!vTok || !wTok || extra != nullptr) {
    return;
  }
  char *endV = nullptr;
  char *endW = nullptr;
  float v = (float)strtod(vTok, &endV);
  float w = (float)strtod(wTok, &endW);
  if (endV == vTok || endW == wTok || *endV != '\0' || *endW != '\0') {
    return;
  }
  lastCommandMs = millis();
  watchdogTripped = false;
  if (fabsf(v) < 0.05f && fabsf(w) < 0.05f) {
    // 0-команда: резкая остановка + сброс ПИД. Без торможения integral
    // windup после разгона оставляет бота едущим ~ V_MAX*Ki/Kp·I_limit ≈
    // 27 мм/с: ошибка(0−measured)+Ki·I = 0 достигается при measured>0.
    power_stop();
  } else {
    set_speed(v, w);
  }

  // Keepalive-телеметрия: непрерывный двунаправленный трафик удерживает
  // RFCOMM-линк HC-06 (проверено: ≥60 с против обрыва ~5.2 с на
  // одностороннем трафике). Ответ на каждую команду создаёт его.
  cmdCounter++;
  Serial.print("ack ");
  Serial.print(cmdCounter);
  Serial.print(" ");
  Serial.print(v);
  Serial.print(" ");
  Serial.print(w);
  // Диагностика: измеренная скорость, выход ШИМ и интеграл каждого колеса.
  // mL/mR = measured, oL/oR = pid out, iL/iR = integral.
  Serial.print(" mL=");
  Serial.print(wheels[WHEEL_LEFT].measuredLinear);
  Serial.print(" mR=");
  Serial.print(wheels[WHEEL_RIGHT].measuredLinear);
  Serial.print(" oL=");
  Serial.print(wheels[WHEEL_LEFT].lastOut);
  Serial.print(" oR=");
  Serial.print(wheels[WHEEL_RIGHT].lastOut);
  Serial.print(" iL=");
  Serial.print(wheels[WHEEL_LEFT].integral);
  Serial.print(" iR=");
  Serial.print(wheels[WHEEL_RIGHT].integral);
  Serial.println();
}

void processSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0 && !cmdOverflow) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
        cmdOverflow = false;
      }
    } else if (cmdLen < CMD_BUF_SIZE - 1) {
      cmdBuf[cmdLen++] = c;
    } else {
      cmdOverflow = true;
    }
  }
}

void setup() {
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

  Serial.begin(BT_BAUD);
  Serial.println("hello PC from ARDU");
}

void loop() {
  uint32_t now = millis();

  if (driveState == STATE_BRAKING && now - brakeStartMs >= BRAKE_DURATION_MS) {
    driveState = STATE_STOPPED;
    wheels[WHEEL_LEFT].coast();
    wheels[WHEEL_RIGHT].coast();
  }

  if (driveState == STATE_RUN) {
    controlLoop();
  }

  processSerial();

  if (!watchdogTripped && now - lastCommandMs > WATCHDOG_TIMEOUT_MS) {
    watchdogTripped = true;
    stop();
  }
}
