// --- Квадратурные энкодеры -------------------------------------------------
// Вся работа с энкодерами собрана здесь. Класс Encoder инкапсулирует состояние
// одного колеса: счётчик тиков, подавление дребезга, переходы квадратурной
// таблицы и EMA-фильтр измеренной скорости. Класс Encoders владеет двумя
// экземплярами и общим обработчиком прерывания PCINT1.
//
// Пины A0-A3 — это PORTC, поэтому ISR читает состояние сразу всей группы PINC.
// Arduino склеивает табы в один файл: encoders.ino идёт после
// motor_controller.ino и commands.ino, но до motors.ino, поэтому тип Encoder
// уже доступен motors.ino.
//
// Методы определены прямо в классах. Вне класса остаётся только определение
// статической таблицы kQuadTable: в C++11 (AVR-ядро собирает с -std=gnu++11)
// статический массив-член нельзя инициализировать в теле класса.

#include <util/atomic.h>

#define PIN_ENC_R_A A0
#define PIN_ENC_R_B A1
#define PIN_ENC_L_A A2
#define PIN_ENC_L_B A3

const uint16_t ENCODER_CPR = 11;
const uint8_t GEAR_RATIO = 56;
const uint32_t TICKS_PER_WHEEL_REV = ENCODER_CPR * 4UL * GEAR_RATIO;

// Один тик не чаще MIN_TICK_US — защита от дребезга контактов.
const uint16_t MIN_TICK_US = 50;

// Постоянная времени сглаживания измеренной скорости (EMA). При периоде
// контура 10 мс один тик энкодера — это ~8.5 мм/с, а Kp=1.5 превращает
// квантование в ±10 ШИМ прямо на моторе (видимая неравномерность). EMA с
// τ=60 мс режет шум примерно в 3.6 раза, оставляя контур достаточно быстрым.
const float SPEED_FILTER_TAU_S = 0.06f;

const float WHEEL_DIAMETER_MM = 67.0f;

class Encoder {
 public:
  Encoder() = default;
  Encoder(const Encoder &) = delete;
  Encoder &operator=(const Encoder &) = delete;

  // Перебазирует опорную точку после паузы: следующая итерация считает тики
  // только с этого момента.
  void rebase() {
    prevTicks_ = takeTicks();
    filterInit_ = false;
  }

  // Сбрасывает фильтр, чтобы после остановки первый замер был не сглаженным.
  void resetFilter() {
    filterInit_ = false;
  }

  // Возвращает сглаженную скорость в мм/с. direction — знак колеса
  // (motorSign): +1 для правого, -1 для левого при движении вперёд.
  float measureSpeed(float dtSec, int8_t direction) {
    int32_t currentTicks = takeTicks();
    int32_t delta = currentTicks - prevTicks_;
    prevTicks_ = currentTicks;
    float mmPerTick = PI * WHEEL_DIAMETER_MM / (float)TICKS_PER_WHEEL_REV;
    float raw = (float)delta * mmPerTick * (float)direction / dtSec;
    // EMA: единичный отсчёт за такт квантуется шагом ~8.5 мм/с, и ПИД с
    // Kp=1.5 гоняется за этим шумом. Сглаживаем, чтобы ШИМ не дрожал.
    if (!filterInit_) {
      measuredMmS_ = raw;
      filterInit_ = true;
    } else {
      float alpha = dtSec / (SPEED_FILTER_TAU_S + dtSec);
      measuredMmS_ += alpha * (raw - measuredMmS_);
    }
    return measuredMmS_;
  }

 private:
  friend class Encoders;

  // Обрабатывает новое состояние каналов A/B (младшие 2 бита). Вызывается
  // только из Encoders::handleInterrupt(), то есть из ISR.
  void update(uint8_t state) {
    uint8_t idx = (prevState_ << 2) | state;
    int8_t step = kQuadTable[idx];
    if (step != 0) {
      prevState_ = state;
      uint32_t now = micros();
      if (now - lastTickMicros_ >= MIN_TICK_US) {
        lastTickMicros_ = now;
        ticks_ += step;
      }
    }
  }

  // Атомарный снимок 32-битного счётчика. На AVR volatile не делает чтение
  // четырёх байт атомарным: ISR может обновить ticks_ между байтами.
  int32_t takeTicks() const {
    int32_t snapshot;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      snapshot = ticks_;
    }
    return snapshot;
  }

  static const int8_t kQuadTable[16];

  volatile int32_t ticks_ = 0;
  int32_t prevTicks_ = 0;
  uint8_t prevState_ = 0;
  uint32_t lastTickMicros_ = 0;
  float measuredMmS_ = 0.0f;
  bool filterInit_ = false;
};

// Диагноз (контролируемый тест 2026-09-12): при вращении колеса "вперёд"
// руками оба энкодера показывали ОТРИЦАТЕЛЬНУЮ скорость (mL=mR=-4..-500
// мм/с) — контур работал в положительную обратную связь (моторы вылетали
// в oL=oR=255 в ту же сторону). motorSign(-1/+1) калиброваны под старую
// разводку D8-D11; после переезда на Port C (A0-A3) каналы A/B на обоих
// колёсах обменялись, что инвертирует подсчёт. Таблица ниже — фикс:
// эквивалент смены мест проводов A/B на обоих энкодерах. Она флипает знак
// СЧЁТА, не трогая motorSign и направление моторов.
const int8_t Encoder::kQuadTable[16] = {
    0, 1, -1, 0,
    -1, 0, 0, 1,
    1, 0, 0, -1,
    0, -1, 1, 0
};

class Encoders {
 public:
  Encoders() = default;
  Encoders(const Encoders &) = delete;
  Encoders &operator=(const Encoders &) = delete;

  void begin() {
    pinMode(PIN_ENC_L_A, INPUT_PULLUP);
    pinMode(PIN_ENC_L_B, INPUT_PULLUP);
    pinMode(PIN_ENC_R_A, INPUT_PULLUP);
    pinMode(PIN_ENC_R_B, INPUT_PULLUP);

    PCICR |= (1 << PCIE1);
    PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11);
  }

  void handleInterrupt() {
    uint8_t pins = PINC & 0x0F;
    encoders_[static_cast<uint8_t>(MotorId::Left)].update((pins >> 2) & 0x03);
    encoders_[static_cast<uint8_t>(MotorId::Right)].update(pins & 0x03);
  }

  Encoder &operator[](MotorId id) {
    return encoders_[static_cast<uint8_t>(id)];
  }

 private:
  Encoder encoders_[2];
};

Encoders encoders;

ISR(PCINT1_vect) {
  encoders.handleInterrupt();
}
