// --- Автоконфигурация HC-06 ------------------------------------------------
// Всё, что знает про HC-06, собрано здесь. BtConfigurator хранит диалект
// модуля и работает с переданным Hardware Serial, поэтому не зависит от
// глобального Serial напрямую.
//
// Скорость задаётся константой BtConfigurator::kBaud. Основной скетч не должен
// обращаться к ней напрямую: configure() во всех ветках оставляет Hardware
// Serial на этой скорости.
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

// Коды результата автоконфигурации. Значения совпадают с прежними кодами
// мигания светодиода, чтобы диагностика на железе не изменилась.
enum class BtResult : uint8_t {
  Migrated = 1,
  AlreadyAtBaud = 2,
  NotConfirmed = 3,
  MigratedFromOther = 4,
  NoAnswer = 5,
};

class BtConfigurator {
 public:
  // Привязывает конфигуратор к порту и пину индикации. Вызывается из
  // Robot::begin() после инициализации Arduino, поэтому глобальный Serial
  // здесь ещё не используется.
  void begin(HardwareSerial& serial, uint8_t ledPin) {
    serial_ = &serial;
    ledPin_ = ledPin;
  }

  // Переводит модуль на kBaud и возвращает результат для диагностического
  // мигания светодиодом. Пока идёт AT-последовательность, встроенный
  // светодиод горит постоянно — так видно, что работает новая прошивка.
  // По выходу Hardware Serial всегда открыт на kBaud.
  BtResult configure() {
    // Индикатор: горит, пока идёт AT-последовательность.
    pinMode(ledPin_, OUTPUT);
    digitalWrite(ledPin_, HIGH);

    // 9600 — скорость по умолчанию и самый частый случай.
    serial_->begin(9600);
    delay(2500);  // HC-06 успевает загрузиться и войти в AT-режим

    if (detectDialect_()) {
      setBaud115200_();
      serial_->begin(kBaud);
      delay(100);
      // После смены скорости модуль перезагружается: диалект определяем заново.
      return detectDialect_() ? BtResult::Migrated : BtResult::NotConfirmed;
    }

    // На 9600 тишина: возможно, модуль уже на kBaud. Диалект здесь ещё не
    // известен (на повторных стартах 9600 молчит), поэтому определяем его.
    serial_->begin(kBaud);
    delay(100);
    if (detectDialect_()) {
      return BtResult::AlreadyAtBaud;
    }

    // Модуль не на 9600 и не на kBaud: перебираем промежуточные скорости.
    static const uint32_t kRates[] = {19200, 38400, 57600};
    for (uint8_t i = 0; i < sizeof(kRates) / sizeof(kRates[0]); i++) {
      serial_->begin(kRates[i]);
      delay(100);
      if (detectDialect_()) {
        setBaud115200_();
        serial_->begin(kBaud);
        delay(100);
        return detectDialect_() ? BtResult::MigratedFromOther
                                : BtResult::NotConfirmed;
      }
    }

    serial_->begin(kBaud);
    delay(100);
    return BtResult::NoAnswer;
  }

  // Мигает встроенным светодиодом (D13) кодом результата автоконфигурации.
  // Медленные вспышки (0.6 с) отличимы от быстрых вспышек загрузчика.
  void blinkResult(BtResult result) {
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
  }

 private:
  static constexpr uint32_t kBaud = 115200;

  bool atCommandRaw_(const char *cmd, bool crlf, uint32_t timeoutMs) {
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

  bool atCommand_(const char *cmd, uint32_t timeoutMs) {
    return atCommandRaw_(cmd, useCrlf_, timeoutMs);
  }

  // Определяет диалект модуля. Сначала официальный HC-06 (без \r\n), затем
  // клон с \r\n (с повторами: первая команда после старта может дать
  // ERROR:(0)). Диалект не зависит от скорости, но определяется на каждой
  // проверяемой скорости заново: после смены скорости модуль перезагружается
  // и снова может ответить ERROR:(0) на первую команду. Поэтому useCrlf_
  // сбрасывается.
  bool detectDialect_() {
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
  bool setBaud115200_() {
    if (atCommand_("AT+BAUD8", 500)) {
      return true;
    }
    return atCommand_("AT+UART=115200,0,0", 500);
  }

  HardwareSerial *serial_ = nullptr;
  uint8_t ledPin_ = LED_BUILTIN;
  bool useCrlf_ = false;
};

constexpr uint32_t BtConfigurator::kBaud;
