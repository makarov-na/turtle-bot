// --- Приём и разбор команд, watchdog ---------------------------------------
// Вынесено из motor_controller.ino. Модуль владеет буфером строки, счётчиком
// команд, последней уставкой и состоянием watchdog; наружу отдаёт
// commandsProcess(), commandsWatchdogTick() и аксессоры для телеметрии.

const uint32_t WATCHDOG_TIMEOUT_MS = 1000;

const uint8_t CMD_BUF_SIZE = 40;
static char cmdBuf[CMD_BUF_SIZE];
static uint8_t cmdLen = 0;
static bool cmdOverflow = false;

static uint32_t lastCommandMs = 0;
static bool watchdogTripped = true;

static uint32_t cmdCounter = 0;

// Последняя применённая уставка — эхо колонок v/w периодической телеметрии.
// До первой команды — нули.
static float lastLinear = 0.0f;
static float lastAngular = 0.0f;

static void handleCommand(char *buf) {
  // Парсим вручную: scanf-вариант из avr-libc по умолчанию не умеет %f
  // (нужна принудительная линковка libscanf_flt), а strtof доступен всегда.
  char *saveptr = nullptr;
  char *tok = strtok_r(buf, " ", &saveptr);
  if (!tok || strcmp(tok, "set_speed") != 0) {
    return;
  }
  char *vTok = strtok_r(nullptr, " ", &saveptr);
  char *wTok = strtok_r(nullptr, " ", &saveptr);
  char *extra = strtok_r(nullptr, " ", &saveptr);
  if (!vTok || !wTok || extra != nullptr) {
    return;
  }
  char *endV = nullptr;
  char *endW = nullptr;
  float v = (float)strtod(vTok, &endV);
  float w = (float)strtod(wTok, &endW);
  if (endV == vTok || endW == wTok || *endV != '\0' || *endW != '\0') {
    return;
  }
  lastCommandMs = millis();
  watchdogTripped = false;
  if (fabsf(v) < 0.05f && fabsf(w) < 0.05f) {
    // 0-команда: резкая остановка + сброс ПИД. Без торможения integral
    // windup после разгона оставляет бота едущим ~ V_MAX*Ki/Kp·I_limit ≈
    // 27 мм/с: ошибка(0−measured)+Ki·I = 0 достигается при measured>0.
    power_stop();
  } else {
    set_speed(v, w);
  }

  // Команда не печатает телеметрию: кадр отправляется по таймеру из loop()
  // (sendTelemetry), чтобы печать не зависела от накопления команд в RX.
  cmdCounter++;
  lastLinear = v;
  lastAngular = w;
}

void commandsProcess() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0 && !cmdOverflow) {
        cmdBuf[cmdLen] = '\0';
        handleCommand(cmdBuf);
        cmdLen = 0;
        cmdOverflow = false;
      }
    } else if (cmdLen < CMD_BUF_SIZE - 1) {
      cmdBuf[cmdLen++] = c;
    } else {
      cmdOverflow = true;
    }
  }
}

void commandsWatchdogTick() {
  // Считаем таймаут по свежему millis(): lastCommandMs обновляется в
  // commandsProcess() позже, чем снят `now` в loop(), поэтому беззнаковая
  // разность `now - lastCommandMs` могла переполниться (0xFFFFFFFF) и дать
  // ложный stop().
  if (!watchdogTripped &&
      (uint32_t)(millis() - lastCommandMs) > WATCHDOG_TIMEOUT_MS) {
    watchdogTripped = true;
    stop();
  }
}

uint32_t commandsCounter() {
  return cmdCounter;
}

float commandsLastLinear() {
  return lastLinear;
}

float commandsLastAngular() {
  return lastAngular;
}
