// --- Квадратурные энкодеры: интерфейс --------------------------------------
// Encoder инкапсулирует состояние одного колеса: счётчик тиков, подавление
// дребезга, переходы квадратурной таблицы и EMA-фильтр измеренной скорости.
// Encoders владеет двумя экземплярами и общим обработчиком прерывания PCINT1.
//
// Экземпляр encoders создаётся в основном скетче: его адрес нужен ISR, а ссылку
// на него получает Chassis. Реализация и ISR — в encoders.cpp.

#ifndef ENCODERS_H
#define ENCODERS_H

#include <Arduino.h>

class Encoder {
 public:
  Encoder() = default;
  Encoder(const Encoder &) = delete;
  Encoder &operator=(const Encoder &) = delete;

  // Перебазирует опорную точку после паузы: следующая итерация считает тики
  // только с этого момента.
  void rebase();

  // Сбрасывает фильтр, чтобы после остановки первый замер был не сглаженным.
  void resetFilter();

  // Возвращает сглаженную скорость в мм/с. direction — знак колеса
  // (motorSign): +1 для правого, -1 для левого при движении вперёд.
  float measureSpeed(float dtSec, int8_t direction);

 private:
  friend class Encoders;

  // Обрабатывает новое состояние каналов A/B (младшие 2 бита). Вызывается
  // только из Encoders::handleInterrupt(), то есть из ISR.
  void update(uint8_t state);

  // Атомарный снимок 32-битного счётчика. На AVR volatile не делает чтение
  // четырёх байт атомарным: ISR может обновить ticks_ между байтами.
  int32_t takeTicks() const;

  static const int8_t kQuadTable[16];

  volatile int32_t ticks_ = 0;
  int32_t prevTicks_ = 0;
  uint8_t prevState_ = 0;
  uint32_t lastTickMicros_ = 0;
  float measuredMmS_ = 0.0f;
  bool filterInit_ = false;
};

class Encoders {
 public:
  Encoders() = default;
  Encoders(const Encoders &) = delete;
  Encoders &operator=(const Encoders &) = delete;

  void init();

  // Обрабатывает фронт на любой из линий A0-A3. Вызывается из ISR.
  void handleInterrupt();

  Encoder &left();
  Encoder &right();

 private:
  Encoder encoders_[2];
};

// Экземпляр создаётся в основном скетче; ISR из encoders.cpp обращается к нему
// через это объявление.
extern Encoders encoders;

#endif
