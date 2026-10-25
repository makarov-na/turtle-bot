// --- Автоконфигурация HC-06 ------------------------------------------------
// Вынесено из motor_controller.ino: всё, что знает про HC-06, собрано здесь.
//
// BT_BAUD объявлен в этом файле, поэтому основной скетч не должен обращаться
// к нему напрямую: btConfigureModule() во всех ветках оставляет Hardware
// Serial на этой скорости.
//
// Модуль хранит скорость UART между включениями. После первой прошивки на
// 115200 он может остаться на 9600, поэтому на старте прошивка сама пытается
// его перевести: в AT-режиме (модуль ещё ни с кем не соединён по Bluetooth)
// команда смены скорости переключает UART на 115200. Если модуль уже на
// 115200 или сейчас в data-режиме (кто-то подключён), команда игнорируется,
// и дальше работа идёт на BT_BAUD.
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

#define BT_BAUD 115200

// true — команды завершаются \r\n (клоны), false — без перевода строки
// (официальный HC-06).
static bool btUseCrlf = false;

static bool btAtCommandRaw(const char *cmd, bool crlf, uint32_t timeoutMs) {
  char line[32];
  uint8_t len = 0;
  bool ok = false;

  while (Serial.available() > 0) {
    Serial.read();
  }
  Serial.print(cmd);
  if (crlf) {
    Serial.print("\r\n");
  }

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
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

static bool btAtCommand(const char *cmd, uint32_t timeoutMs) {
  return btAtCommandRaw(cmd, btUseCrlf, timeoutMs);
}

// Определяет диалект модуля. Сначала официальный HC-06 (без \r\n), затем
// клон с \r\n (с повторами: первая команда после старта может дать
// ERROR:(0)).
//
// Диалект не зависит от скорости, но определяется на каждой проверяемой
// скорости заново: после смены скорости модуль перезагружается и снова может
// ответить ERROR:(0) на первую команду. Поэтому btUseCrlf сбрасывается.
static bool btDetectDialect() {
  btUseCrlf = false;
  if (btAtCommandRaw("AT", false, 300)) {
    return true;
  }
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    if (btAtCommandRaw("AT", true, 300)) {
      btUseCrlf = true;
      return true;
    }
    delay(200);
  }
  return false;
}

// Переводит модуль на 115200. Официальные HC-06 понимают AT+BAUD8, некоторые
// клоны — только AT+UART=115200,0,0.
static bool btSetBaud115200() {
  if (btAtCommand("AT+BAUD8", 500)) {
    return true;
  }
  return btAtCommand("AT+UART=115200,0,0", 500);
}

// Переводит модуль на BT_BAUD и возвращает код результата для диагностического
// мигания светодиодом:
//   1 — AT на 9600, модуль переведён и 115200 подтверждён;
//   2 — модуль уже был на 115200;
//   3 — AT на 9600 работает, но 115200 не подтверждён (нужна перезагрузка
//       модуля или другая команда смены скорости);
//   4 — AT ответил на промежуточной скорости (переведён);
//   5 — AT не отвечает ни на одной скорости.
//
// Пока идёт AT-последовательность, встроенный светодиод горит постоянно —
// так видно, что работает новая прошивка. По выходу Hardware Serial всегда
// открыт на BT_BAUD.
uint8_t btConfigureModule() {
  const uint8_t kMigrated = 1;
  const uint8_t kAlready115200 = 2;
  const uint8_t kNotConfirmed = 3;
  const uint8_t kMigratedFromOther = 4;
  const uint8_t kNoAnswer = 5;

  // Индикатор: горит, пока идёт AT-последовательность.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // 9600 — скорость по умолчанию и самый частый случай.
  Serial.begin(9600);
  delay(2500);  // HC-06 успевает загрузиться и войти в AT-режим

  if (btDetectDialect()) {
    btSetBaud115200();
    Serial.begin(BT_BAUD);
    delay(100);
    // После смены скорости модуль перезагружается: диалект определяем заново.
    return btDetectDialect() ? kMigrated : kNotConfirmed;
  }

  // На 9600 тишина: возможно, модуль уже на 115200. Диалект здесь ещё не
  // известен (на повторных стартах 9600 молчит), поэтому определяем его.
  Serial.begin(BT_BAUD);
  delay(100);
  if (btDetectDialect()) {
    return kAlready115200;
  }

  // Модуль не на 9600 и не на 115200: перебираем промежуточные скорости.
  static const uint32_t kRates[] = {19200, 38400, 57600};
  for (uint8_t i = 0; i < sizeof(kRates) / sizeof(kRates[0]); i++) {
    Serial.begin(kRates[i]);
    delay(100);
    if (btDetectDialect()) {
      btSetBaud115200();
      Serial.begin(BT_BAUD);
      delay(100);
      return btDetectDialect() ? kMigratedFromOther : kNotConfirmed;
    }
  }

  Serial.begin(BT_BAUD);
  delay(100);
  return kNoAnswer;
}

// Мигает встроенным светодиодом (D13) кодом результата автоконфигурации.
// Позволяет проверить, что залита новая прошивка и как прошёл AT, не глядя
// в Bluetooth. Медленные вспышки (0.6 с) отличимы от быстрых вспышек
// загрузчика при включении.
void btBlinkResult(uint8_t code) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(400);
  for (uint8_t i = 0; i < code; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(600);
    digitalWrite(LED_BUILTIN, LOW);
    delay(400);
  }
  delay(800);
}
