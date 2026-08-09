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

#define PIN_IN1_R  2
#define PIN_IN2_R  3
#define PIN_ENA_R  5
#define PIN_ENC_R_A 8
#define PIN_ENC_R_B 9

#define PIN_IN3_L  4
#define PIN_IN4_L  7
#define PIN_ENB_L  6
#define PIN_ENC_L_A 10
#define PIN_ENC_L_B 11

const float WHEEL_DIAMETER_MM = 67.0f;
const float WHEELBASE_MM = 185.0f;
const float V_MAX_MM_S = 300.0f;
const float OMEGA_MAX_RAD_S = 2.0f * V_MAX_MM_S / WHEELBASE_MM;

const uint32_t RAMP_DURATION_MS = 1000;
const uint32_t BRAKE_DURATION_MS = 300;
const uint32_t PID_PERIOD_MS = 10;
const uint16_t MIN_TICK_US = 50;

const float TEST_SPEED_MM_S = 250.0f;
const uint32_t TEST_RUN_TIME_MS = 9000;

const uint16_t ENCODER_CPR  = 11;
const uint8_t  GEAR_RATIO   = 56;
const uint32_t TICKS_PER_WHEEL_REV = ENCODER_CPR * 4UL * GEAR_RATIO;

const float PID_KP = 1.5f;
const float PID_KI = 0.2f;
const float PID_KD = 0.0f;
const float PID_INTEGRAL_LIMIT = 200.0f;

static const int8_t QUAD_TABLE[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
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
    float out = PID_KP * error + PID_KI * integral + PID_KD * derivative;
    return constrain(out, -255.0f, 255.0f);
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

ISR(PCINT0_vect) {
  uint8_t pins = PINB & 0x0F;
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
}

void power_stop() {
  wheels[WHEEL_LEFT].setTarget(0.0f);
  wheels[WHEEL_RIGHT].setTarget(0.0f);
  driveState = STATE_BRAKING;
  brakeStartMs = millis();
  wheels[WHEEL_LEFT].brake();
  wheels[WHEEL_RIGHT].brake();
}

void controlLoop() {
  uint32_t now = millis();
  if (now - lastControlMs < PID_PERIOD_MS) {
    return;
  }
  float dt = (float)(now - lastControlMs) / 1000.0f;
  lastControlMs = now;

  for (int w = 0; w < 2; w++) {
    Wheel &wh = wheels[w];
    wh.updateRamp(now);
    wh.measureSpeed(dt);
    float out = wh.pidUpdate(wh.rampedLinear, wh.measuredLinear, dt);
    wh.applyPwm(out);
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

  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT0) | (1 << PCINT1) | (1 << PCINT2) | (1 << PCINT3);
}

void loop() {
  static uint32_t demoStartMs = 0;
  static bool demoStarted = false;
  static bool demoStopped = false;
  uint32_t now = millis();

  if (driveState == STATE_BRAKING && now - brakeStartMs >= BRAKE_DURATION_MS) {
    driveState = STATE_STOPPED;
    wheels[WHEEL_LEFT].coast();
    wheels[WHEEL_RIGHT].coast();
  }

  if (driveState == STATE_RUN) {
    controlLoop();
  }

  if (!demoStarted) {
    demoStarted = true;
    demoStartMs = now;
    set_speed(TEST_SPEED_MM_S, 0.0f);
  }
  if (!demoStopped && now - demoStartMs >= TEST_RUN_TIME_MS) {
    demoStopped = true;
    power_stop();
  }
}
