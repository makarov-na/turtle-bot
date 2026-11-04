// --- Реализация автоконфигурации HC-06 -------------------------------------
// Интерфейс BtConfigurator — в bt_config.h, экземпляр bt — в основном скетче:
// автоконфигурация — самостоятельная опциональная функция, которую setup()
// запускает отдельно. Здесь — реализация AT-последовательности и её внутренние
// коды результата; наружу они не выходят.
//
// Модуль хранит скорость UART между включениями. После первой прошивки на
// 115200 он может остаться на 9600, поэтому на старте прошивка сама пытается
// его перевести: в AT-режиме (модуль ещё ни с кем не соединён по Bluetooth)
// команда смены скорости переключает UART на 115200. Если модуль уже на
// 115200 или сейчас в data-режиме (кто-то подключён), команда игнорируется,
// и дальше работа идёт на kBaud.
//
// Диалекты AT различаются:
//   * официальный HC-06 (linvor/hc01) — команды БЕЗ перевода строки;
//   * клоны (в том числе установленный) — команды ТОЛЬКО с \r\n, причём
//     первая команда после включения может ответить "ERROR:(0)", дальше "OK".
// Прошивка определяет диалект сама (на каждой проверяемой скорости заново) и
// дальше использует найденный.
//
// Смена скорости: AT+BAUD8 (HC-06), при отказе — AT+UART=115200,0,0
// (HC-05-совместимые клоны).
//
// Если модуль оказался на промежуточной скорости (19200/38400/57600), она
// находится перебором и всё равно переводится на 115200.
//
// Ограничение: AT-команды принимаются только когда модуль не соединён.
// Поэтому первый старт после прошивки должен произойти до подключения ПК;
// иначе остаётся ручная процедура по проводу (см. docs).

#include "bt_config.h"

constexpr uint32_t BtConfigurator::kBaud;

namespace {

// Коды результата автоконфигурации. Значения совпадают с прежними кодами
// мигания светодиода, чтобы диагностика на железе не изменилась.
enum class BtResult : uint8_t {
  Migrated = 1,
  AlreadyAtBaud = 2,
  NotConfirmed = 3,
  MigratedFromOther = 4,
  NoAnswer = 5,
};

}  // namespace

// Конструктор только связывает конфигуратор с портом и пином индикации.
BtConfigurator::BtConfigurator(HardwareSerial &serial, uint8_t ledPin)
    : serial_(&serial), ledPin_(ledPin) {}

// Единственная публичная точка входа: переводит модуль на kBaud, мигает
// результатом и здоровается с ПК.
// Пока идёт AT-последовательность, встроенный светодиод горит постоянно — так
// видно, что работает новая прошивка. По выходу Hardware Serial всегда открыт
// на kBaud.
void BtConfigurator::init() {
  // Индикатор: горит, пока идёт AT-последовательность.
  pinMode(ledPin_, OUTPUT);
  digitalWrite(ledPin_, HIGH);

  // 9600 — скорость по умолчанию и самый частый случай.
  serial_->begin(9600);
  delay(2500);  // HC-06 успевает загрузиться и войти в AT-режим

  BtResult result;
  if (detectDialect_()) {
    setBaud115200_();
    serial_->begin(kBaud);
    delay(100);
    // После смены скорости модуль перезагружается: диалект определяем заново.
    result = detectDialect_() ? BtResult::Migrated : BtResult::NotConfirmed;
  } else {
    // На 9600 тишина: возможно, модуль уже на kBaud. Диалект здесь ещё не
    // известен (на повторных стартах 9600 молчит), поэтому определяем его.
    serial_->begin(kBaud);
    delay(100);
    if (detectDialect_()) {
      result = BtResult::AlreadyAtBaud;
    } else {
      // Модуль не на 9600 и не на kBaud: перебираем промежуточные скорости.
      static const uint32_t kRates[] = {19200, 38400, 57600};
      bool found = false;
      for (uint8_t i = 0; i < sizeof(kRates) / sizeof(kRates[0]); i++) {
        serial_->begin(kRates[i]);
        delay(100);
        if (detectDialect_()) {
          setBaud115200_();
          serial_->begin(kBaud);
          delay(100);
          result = detectDialect_() ? BtResult::MigratedFromOther
                                    : BtResult::NotConfirmed;
          found = true;
          break;
        }
      }
      if (!found) {
        serial_->begin(kBaud);
        delay(100);
        result = BtResult::NoAnswer;
      }
    }
  }

  // Мигаем светодиодом (D13) кодом результата. Медленные вспышки (0.6 с)
  // отличимы от быстрых вспышек загрузчика.
  pinMode(ledPin_, OUTPUT);
  digitalWrite(ledPin_, LOW);
  delay(400);
  for (uint8_t i = 0; i < static_cast<uint8_t>(result); i++) {
    digitalWrite(ledPin_, HIGH);
    delay(600);
    digitalWrite(ledPin_, LOW);
    delay(400);
  }
  delay(800);

  // Приветствие подтверждает, что Hardware Serial открыт на рабочей скорости.
  serial_->println("hello PC from ARDU");
}

bool BtConfigurator::atCommandRaw_(const char *cmd, bool crlf,
                                   uint32_t timeoutMs) {
  char line[32];
  uint8_t len = 0;
  bool ok = false;

  while (serial_->available() > 0) {
    serial_->read();
  }
  serial_->print(cmd);
  if (crlf) {
    serial_->print("\r\n");
  }

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (serial_->available() > 0) {
      char c = (char)serial_->read();
      if (c == '\r' || c == '\n') {
        if (len > 0) {
          line[len] = '\0';
          if (strncmp(line, "OK", 2) == 0) {
            ok = true;
          }
          len = 0;
        }
      } else if (len < sizeof(line) - 1) {
        line[len++] = c;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

bool BtConfigurator::atCommand_(const char *cmd, uint32_t timeoutMs) {
  return atCommandRaw_(cmd, useCrlf_, timeoutMs);
}

// Определяет диалект модуля. Сначала официальный HC-06 (без \r\n), затем
// клон с \r\n (с повторами: первая команда после старта может дать
// ERROR:(0)). Диалект не зависит от скорости, но определяется на каждой
// проверяемой скорости заново: после смены скорости модуль перезагружается
// и снова может ответить ERROR:(0) на первую команду. Поэтому useCrlf_
// сбрасывается.
bool BtConfigurator::detectDialect_() {
  useCrlf_ = false;
  if (atCommandRaw_("AT", false, 300)) {
    return true;
  }
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    if (atCommandRaw_("AT", true, 300)) {
      useCrlf_ = true;
      return true;
    }
    delay(200);
  }
  return false;
}

// Переводит модуль на 115200. Официальные HC-06 понимают AT+BAUD8,
// некоторые клоны — только AT+UART=115200,0,0.
bool BtConfigurator::setBaud115200_() {
  if (atCommand_("AT+BAUD8", 500)) {
    return true;
  }
  return atCommand_("AT+UART=115200,0,0", 500);
}
