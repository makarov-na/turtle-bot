// Тестовый скетч проверки Bluetooth-канала через эхо
// Arduino Nano
//
// Принимает текстовые строки с ПК через Hardware Serial (D0/D1, на которые
// подключён HC-06) на скорости 9600 бод и возвращает их обратно с нарастающим
// счётчиком в формате:
//     echo <n>: <line>
// где <line> — принятая строка без перевода строки, <n> — номер ответа.
//
// Канал соответствует ограничениям bluetooth-telemetry:
// HC-06 TX → D0 (RX), HC-06 RX ← D1 (TX) через делитель 5V→3.3V (RX HC-06
// не 5V-tolerant). Питание HC-06 — от отдельного 5V (Cedar buck).
//
// Sketch — автономный «измерительный прибор» канала:
//   *  не использует драйвер L298N (пины ШИМ и IN1-IN4 не активируются);
//   *  не имеет watchdog/остановок — отвечает на любую строку безусловно,
//      даже если ПК молчал несколько секунд;
//   *  периодические сообщения отсутствуют: единственный вывод в простое —
//      приветствие при старте.
//
// ВНИМАНИЕ: при перепрошивке отключать HC-06 (загрузчик Arduino использует
// D0/D1).

#define HARDWARE_SERIAL_BAUD 9600

#define LINE_BUF_SIZE 64

char lineBuf[LINE_BUF_SIZE];
uint8_t lineLen = 0;
bool lineOverflow = false;
uint32_t echoCounter = 0;

void setup() {
  Serial.begin(HARDWARE_SERIAL_BAUD);
  Serial.println("echo test ready");
}

void processLine() {
  echoCounter++;

  Serial.print("echo ");
  Serial.print(echoCounter);
  if (lineOverflow) {
    // Частичную строку не эхоируем — пропускаем, чтобы не путать метрику.
    Serial.println(": <line too long>");
  } else {
    Serial.print(": ");
    Serial.println(lineBuf);
  }

  lineLen = 0;
  lineOverflow = false;
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n') {
      processLine();
    } else if (c == '\r') {
      // Игнорируем возврат каретки, строки завершаются переводом строки.
    } else if (lineLen + 1 < LINE_BUF_SIZE) {
      lineBuf[lineLen++] = c;
      lineBuf[lineLen] = '\0';
    } else {
      lineOverflow = true;
    }
  }
}