// --- Автоконфигурация HC-06: интерфейс -------------------------------------
// Класс вынесен в отдельный заголовок, чтобы экземпляр можно было создать в
// основном скетче: bt_config.cpp — отдельная единица трансляции, и без
// заголовка тип в setup() не виден.
//
// Реализация AT-последовательности и внутренние коды результата — в
// bt_config.cpp.

#ifndef BT_CONFIG_H
#define BT_CONFIG_H

#include <Arduino.h>

class BtConfigurator {
 public:
  // Привязывает конфигуратор к порту и пину индикации, переводит модуль на
  // kBaud, мигает результатом и здоровается с ПК.
  void btInit(HardwareSerial &serial, uint8_t ledPin);

 private:
  static constexpr uint32_t kBaud = 115200;

  bool atCommandRaw_(const char *cmd, bool crlf, uint32_t timeoutMs);
  bool atCommand_(const char *cmd, uint32_t timeoutMs);
  bool detectDialect_();
  bool setBaud115200_();

  HardwareSerial *serial_ = nullptr;
  uint8_t ledPin_ = LED_BUILTIN;
  bool useCrlf_ = false;
};

#endif
