// --- Автоконфигурация HC-06: интерфейс -------------------------------------
// Класс вынесен в отдельный заголовок, чтобы тип был виден основному скетчу:
// bt_config.cpp — отдельная единица трансляции.
//
// Порт и пин индикации внедряются в конструкторе; init() выполняет
// AT-последовательность. Реализация — в bt_config.cpp.

#ifndef BT_CONFIG_H
#define BT_CONFIG_H

#include <Arduino.h>

class BtConfigurator {
 public:
  BtConfigurator(HardwareSerial &serial, uint8_t ledPin);

  // Переводит модуль на kBaud, мигает результатом и здоровается с ПК.
  void init();

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
