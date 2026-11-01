// --- Приём и разбор команд, watchdog ---------------------------------------
// CommandReceiver владеет буфером строки, счётчиком команд, последней уставкой
// и состоянием watchdog. Действия передаются через CommandSink, поэтому парсер
// не зависит от Chassis напрямую и его можно проверить без моторов.
//
// Watchdog остановлен, пока не пришла первая валидная команда; каждая валидная
// команда сбрасывает таймер. По истечении WATCHDOG_TIMEOUT_MS вызывается
// CommandSink::stop(), и привод остаётся остановленным до новой команды.

const uint32_t WATCHDOG_TIMEOUT_MS = 1000;

// Минимальный интерфейс, который нужен парсеру от привода.
class CommandSink {
 public:
  virtual ~CommandSink() {}
  virtual void set_speed(float linearSpeed, float angularSpeed) = 0;
  virtual void power_stop() = 0;
  virtual void stop() = 0;
};

class CommandReceiver {
 public:
  explicit CommandReceiver(CommandSink &sink) : sink_(sink) {}

  // Читает всё, что накопилось в порту. now — момент начала обработки; он
  // запоминается как время последней валидной команды.
  void process(Stream &serial, uint32_t now) {
    while (serial.available() > 0) {
      char c = (char)serial.read();
      if (c == '\n' || c == '\r') {
        if (cmdLen_ > 0 && !cmdOverflow_) {
          cmdBuf_[cmdLen_] = '\0';
          handleCommand_(cmdBuf_, now);
        }
        // Сбрасываем буфер и признак переполнения в любом случае: длинная
        // битая строка не должна навсегда залипать в парсере.
        cmdLen_ = 0;
        cmdOverflow_ = false;
      } else if (cmdLen_ < kBufSize - 1) {
        cmdBuf_[cmdLen_++] = c;
      } else {
        cmdOverflow_ = true;
      }
    }
  }

  // Считаем таймаут по переданному now — тот же источник времени, что и в
  // основном цикле. Беззнаковая разность защищает от переполнения millis().
  void watchdogTick(uint32_t now) {
    if (!watchdogTripped_ &&
        (uint32_t)(now - lastCommandMs_) > WATCHDOG_TIMEOUT_MS) {
      watchdogTripped_ = true;
      sink_.stop();
    }
  }

  uint32_t counter() const {
    return cmdCounter_;
  }

  float lastLinear() const {
    return lastLinear_;
  }

  float lastAngular() const {
    return lastAngular_;
  }

  // Заполняет общие поля кадра телеметрии.
  void fillTelemetry(TelemetryFrame &frame) const {
    frame.counter = cmdCounter_;
    frame.linear = lastLinear_;
    frame.angular = lastAngular_;
  }

 private:
  static const uint8_t kBufSize = 40;

  void handleCommand_(char *buf, uint32_t now) {
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
    lastCommandMs_ = now;
    watchdogTripped_ = false;
    if (fabsf(v) < 0.05f && fabsf(w) < 0.05f) {
      // 0-команда: резкая остановка + сброс ПИД. Без торможения integral
      // windup после разгона оставляет бота едущим ~ V_MAX*Ki/Kp·I_limit ≈
      // 27 мм/с: ошибка(0−measured)+Ki·I = 0 достигается при measured>0.
      sink_.power_stop();
    } else {
      sink_.set_speed(v, w);
    }

    // Команда не печатает телеметрию: кадр отправляется по таймеру из
    // Robot::tick(), чтобы печать не зависела от накопления команд в RX.
    cmdCounter_++;
    lastLinear_ = v;
    lastAngular_ = w;
  }

  CommandSink &sink_;
  char cmdBuf_[kBufSize];
  uint8_t cmdLen_ = 0;
  bool cmdOverflow_ = false;
  uint32_t lastCommandMs_ = 0;
  bool watchdogTripped_ = true;
  uint32_t cmdCounter_ = 0;
  float lastLinear_ = 0.0f;
  float lastAngular_ = 0.0f;
};
