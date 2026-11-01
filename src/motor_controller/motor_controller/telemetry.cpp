// --- Периодическая телеметрия: реализация ----------------------------------
// Кадр печатается по таймеру, а не в ответ на команду: иначе накопившиеся в
// RX-буфере команды печатаются пачкой и не дают контуру выполниться
// (см. design.md).
//
// Непрерывный двунаправленный трафик удерживает RFCOMM-линк HC-06 (проверено:
// ≥60 с против обрыва ~5.2 с на одностороннем трафике).

#include "telemetry.h"

namespace {

const uint32_t kPeriodMs = 200;

void sendFrame(Stream &serial, const TelemetryFrame &frame) {
  serial.print("ack ");
  serial.print(frame.counter);
  serial.print(" ");
  serial.print(frame.linear);
  serial.print(" ");
  serial.print(frame.angular);
  // Диагностика: измеренная скорость, ШИМ и интеграл каждого колеса.
  // mL/mR = изм. мм/с, oL/oR = ШИМ ±255, iL/iR = интеграл.
  serial.print(" mL=");
  serial.print(frame.measured[0]);
  serial.print(" mR=");
  serial.print(frame.measured[1]);
  serial.print(" oL=");
  serial.print(frame.pwm[0]);
  serial.print(" oR=");
  serial.print(frame.pwm[1]);
  serial.print(" iL=");
  serial.print(frame.integral[0]);
  serial.print(" iR=");
  serial.print(frame.integral[1]);
  serial.println();
}

}  // namespace

void Telemetry::begin(Stream &serial) {
  serial_ = &serial;
}

void Telemetry::tick(uint32_t now, const TelemetryFrame &frame) {
  if (now - lastMs_ >= kPeriodMs) {
    sendFrame(*serial_, frame);
    lastMs_ = now;
  }
}
