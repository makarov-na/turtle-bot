// --- Приём и разбор команд, watchdog: интерфейс ----------------------------
// CommandReceiver — чистый разборщик протокола: он не знает ни про привод, ни
// про Chassis. Наружу отдаёт разобранную команду, а решение, как её применить,
// принимает Robot.
//
// Реализация — в command_receiver.cpp.

#ifndef COMMAND_RECEIVER_H
#define COMMAND_RECEIVER_H

#include <Arduino.h>

class CommandReceiver {
 public:
  enum class Action : uint8_t { SetSpeed, PowerStop };

  struct Command {
    Action action;
    float linear;
    float angular;
  };

  // Читает порт до первой полной валидной команды. Возвращает true, если
  // команда разобрана и записана в out. Невалидные строки игнорируются.
  bool poll(Stream &serial, uint32_t now, Command &out);

  // Возвращает true один раз, когда с последней валидной команды прошло
  // больше таймаута watchdog. Вызывающий сам решает, как остановить привод.
  bool watchdogExpired(uint32_t now);

  uint32_t counter() const;

  float lastLinear() const;

  float lastAngular() const;

 private:
  static const uint8_t kBufSize = 40;

  bool parseCommand_(char *buf, uint32_t now, Command &out);

  char cmdBuf_[kBufSize];
  uint8_t cmdLen_ = 0;
  bool cmdOverflow_ = false;
  uint32_t lastCommandMs_ = 0;
  bool watchdogTripped_ = true;
  uint32_t cmdCounter_ = 0;
  float lastLinear_ = 0.0f;
  float lastAngular_ = 0.0f;
};

#endif
