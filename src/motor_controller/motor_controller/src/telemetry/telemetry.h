// --- Периодическая телеметрия: интерфейс -----------------------------------
// TelemetryFrame — снимок одного кадра ack, Telemetry — его форматирование и
// период отправки. Модуль не знает, откуда берутся значения: кадр собирает
// MotorController. Поток внедряется в конструкторе.
//
// Реализация — в telemetry.cpp.

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>

// Поля соответствуют протоколу ack. Индексы массивов: 0 — левое колесо,
// 1 — правое.
struct TelemetryFrame {
  uint32_t counter;
  float linear;
  float angular;
  float measured[2];
  float pwm[2];
  float integral[2];
};

class Telemetry {
 public:
  // Привязывает кадры к порту. Экземпляр создаётся в основном скетче после
  // настройки UART.
  explicit Telemetry(Stream &serial);

  // Печатает кадр не чаще периода телеметрии. now — единый источник времени
  // основного цикла.
  void tick(uint32_t now, const TelemetryFrame &frame);

 private:
  Stream *serial_ = nullptr;
  uint32_t lastMs_ = 0;
};

#endif
