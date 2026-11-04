// --- Квадратурные энкодеры: реализация -------------------------------------
// Пины A0-A3 — это PORTC, поэтому ISR читает состояние сразу всей группы PINC.
// Вне класса остаётся только определение статической таблицы kQuadTable:
// в C++11 (AVR-ядро собирает с -std=gnu++11) статический массив-член нельзя
// инициализировать в теле класса.

#include "encoders.h"

#include <util/atomic.h>

namespace {

const uint8_t kPinEncRA = A0;
const uint8_t kPinEncRB = A1;
const uint8_t kPinEncLA = A2;
const uint8_t kPinEncLB = A3;

const uint16_t kEncoderCpr = 11;
const uint8_t kGearRatio = 56;
const uint32_t kTicksPerWheelRev = kEncoderCpr * 4UL * kGearRatio;

const uint8_t kLeftEncoder = 0;
const uint8_t kRightEncoder = 1;

// Один тик не чаще kMinTickUs — защита от дребезга контактов.
const uint16_t kMinTickUs = 50;

// Постоянная времени сглаживания измеренной скорости (EMA). При периоде
// контура 10 мс один тик энкодера — это ~8.5 мм/с, а Kp=1.5 превращает
// квантование в ±10 ШИМ прямо на моторе (видимая неравномерность). EMA с
// τ=60 мс режет шум примерно в 3.6 раза, оставляя контур достаточно быстрым.
const float kSpeedFilterTauS = 0.06f;

const float kWheelDiameterMm = 67.0f;

}  // namespace

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

void Encoder::rebase() {
  prevTicks_ = takeTicks();
  filterInit_ = false;
}

void Encoder::resetFilter() {
  filterInit_ = false;
}

float Encoder::measureSpeed(float dtSec, int8_t direction) {
  int32_t currentTicks = takeTicks();
  int32_t delta = currentTicks - prevTicks_;
  prevTicks_ = currentTicks;
  float mmPerTick = PI * kWheelDiameterMm / (float)kTicksPerWheelRev;
  float raw = (float)delta * mmPerTick * (float)direction / dtSec;
  // EMA: единичный отсчёт за такт квантуется шагом ~8.5 мм/с, и ПИД с
  // Kp=1.5 гоняется за этим шумом. Сглаживаем, чтобы ШИМ не дрожал.
  if (!filterInit_) {
    measuredMmS_ = raw;
    filterInit_ = true;
  } else {
    float alpha = dtSec / (kSpeedFilterTauS + dtSec);
    measuredMmS_ += alpha * (raw - measuredMmS_);
  }
  return measuredMmS_;
}

void Encoder::update(uint8_t state) {
  uint8_t idx = (prevState_ << 2) | state;
  int8_t step = kQuadTable[idx];
  if (step != 0) {
    prevState_ = state;
    uint32_t now = micros();
    if (now - lastTickMicros_ >= kMinTickUs) {
      lastTickMicros_ = now;
      ticks_ += step;
    }
  }
}

int32_t Encoder::takeTicks() const {
  int32_t snapshot;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    snapshot = ticks_;
  }
  return snapshot;
}

void Encoders::init() {
  pinMode(kPinEncLA, INPUT_PULLUP);
  pinMode(kPinEncLB, INPUT_PULLUP);
  pinMode(kPinEncRA, INPUT_PULLUP);
  pinMode(kPinEncRB, INPUT_PULLUP);

  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11);
}

void Encoders::handleInterrupt() {
  uint8_t pins = PINC & 0x0F;
  encoders_[kLeftEncoder].update((pins >> 2) & 0x03);
  encoders_[kRightEncoder].update(pins & 0x03);
}

Encoder &Encoders::left() {
  return encoders_[kLeftEncoder];
}

Encoder &Encoders::right() {
  return encoders_[kRightEncoder];
}

ISR(PCINT1_vect) {
  encoders.handleInterrupt();
}
