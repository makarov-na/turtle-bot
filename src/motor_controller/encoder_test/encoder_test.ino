// Тестовый скетч проверки энкодеров
// Arduino Nano
//
// Читает квадратурные энкодеры обоих колёс через прерывания PORTC (A0-A3)
// и выводит значения тиков (со знаком) и скорость (мм/с) через Hardware Serial
// (D0/D1). HC-06 подключён на D0 (RX) и D1 (TX) — данные доступны и через
// Bluetooth, и через USB-монитор.
// Плюс в выводе = бот едет вперёд (правый мотор по часовой, левый против).
//
// Энкодерная логика скопирована из motor_controller.ino — при правках
// боевого скетча синхронизировать вручную.
//
// ВАЖНО: не используем SoftwareSerial (HC-06) — на ATmega328P она алиасит
// PCINT1_vect (и PCINT2_vect) на PCINT0_vect, что конфликтует с ISR(PCINT1_vect)
// энкодеров на этапе линковки (multiple definition of '__vector_4').
// Вывод идёт через встроенный Serial (D0/D1): HC-06 TX→D0, HC-06 RX←D1
// через делитель 5V→3.3V (HC-06 RX не 5V-tolerant).
//
// ВНИМАНИЕ: при перепрошивке отключать HC-06 (загрузчик Arduino использует D0/D1).
// Драйвер моторов (L298N) в этом скетче не задействуется.

#define PIN_ENC_R_A A0
#define PIN_ENC_R_B A1

#define PIN_ENC_L_A A2
#define PIN_ENC_L_B A3

#define HARDWARE_SERIAL_BAUD 9600

const float WHEEL_DIAMETER_MM = 67.0f;
const uint32_t SEND_PERIOD_MS = 1000;
const uint16_t MIN_TICK_US = 50;

const uint16_t ENCODER_CPR  = 11;
const uint8_t  GEAR_RATIO   = 56;
const uint32_t TICKS_PER_WHEEL_REV = ENCODER_CPR * 4UL * GEAR_RATIO;

static const int8_t QUAD_TABLE[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
};

enum WheelId { WHEEL_LEFT = 0, WHEEL_RIGHT = 1 };

struct Wheel {
  const int8_t motorSign;

  volatile int32_t ticks = 0;
  int32_t prevTicks = 0;
  int32_t dispTicks = 0;
  float measuredLinear = 0.0f;

  Wheel(int8_t sign)
      : motorSign(sign) {}

  void measureSpeed(float dtSec) {
    int32_t delta = ticks - prevTicks;
    prevTicks = ticks;
    dispTicks = delta * motorSign;
    float mmPerTick = PI * WHEEL_DIAMETER_MM / (float)TICKS_PER_WHEEL_REV;
    measuredLinear = (float)delta * mmPerTick * (float)motorSign / dtSec;
  }
};

Wheel wheels[2] = {
    {-1},
    {+1}
};

uint32_t lastSendMs = 0;

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

void setup() {
  pinMode(PIN_ENC_L_A, INPUT_PULLUP);
  pinMode(PIN_ENC_L_B, INPUT_PULLUP);
  pinMode(PIN_ENC_R_A, INPUT_PULLUP);
  pinMode(PIN_ENC_R_B, INPUT_PULLUP);

  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11);

  Serial.begin(HARDWARE_SERIAL_BAUD);
  Serial.println("encoder test ready");
}

void loop() {
  uint32_t now = millis();
  if (now - lastSendMs >= SEND_PERIOD_MS) {
    lastSendMs = now;
    float dt = (float)SEND_PERIOD_MS / 1000.0f;
    for (int w = 0; w < 2; w++) {
      wheels[w].measureSpeed(dt);
    }
    Serial.print("L:");
    Serial.print(wheels[WHEEL_LEFT].dispTicks);
    Serial.print(" R:");
    Serial.print(wheels[WHEEL_RIGHT].dispTicks);
    Serial.print(" Lmm/s:");
    Serial.print(wheels[WHEEL_LEFT].measuredLinear);
    Serial.print(" Rmm/s:");
    Serial.println(wheels[WHEEL_RIGHT].measuredLinear);
  }
}