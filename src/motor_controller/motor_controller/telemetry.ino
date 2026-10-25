// --- Периодическая телеметрия ----------------------------------------------
// Вынесено из motor_controller.ino. Кадр печатается по таймеру из loop(), а не
// в ответ на команду: иначе накопившиеся в RX-буфере команды печатаются
// пачкой и не дают controlLoop() выполниться (см. design.md).
//
// Непрерывный двунаправленный трафик удерживает RFCOMM-линк HC-06 (проверено:
// ≥60 с против обрыва ~5.2 с на одностороннем трафике).

const uint32_t TELEMETRY_PERIOD_MS = 200;

static uint32_t lastTelemetryMs = 0;

// v/w — последняя применённая уставка, n — счётчик обработанных команд.
void sendTelemetry() {
  Serial.print("ack ");
  Serial.print(commandsCounter());
  Serial.print(" ");
  Serial.print(commandsLastLinear());
  Serial.print(" ");
  Serial.print(commandsLastAngular());
  // Диагностика: измеренная скорость, ШИМ и интеграл каждого колеса.
  // mL/mR = изм. мм/с, oL/oR = ШИМ ±255, iL/iR = интеграл.
  Serial.print(" mL=");
  Serial.print(wheelMeasuredSpeed(WHEEL_LEFT));
  Serial.print(" mR=");
  Serial.print(wheelMeasuredSpeed(WHEEL_RIGHT));
  Serial.print(" oL=");
  Serial.print(wheelPwm(WHEEL_LEFT));
  Serial.print(" oR=");
  Serial.print(wheelPwm(WHEEL_RIGHT));
  Serial.print(" iL=");
  Serial.print(wheelIntegral(WHEEL_LEFT));
  Serial.print(" iR=");
  Serial.print(wheelIntegral(WHEEL_RIGHT));
  Serial.println();
}

void telemetryTick(uint32_t now) {
  if (now - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
    sendTelemetry();
    lastTelemetryMs = now;
  }
}
