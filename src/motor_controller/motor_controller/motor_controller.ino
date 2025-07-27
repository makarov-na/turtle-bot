// Определение пинов для правого мотора
#define ENA_R 5   // ШИМ-контроль скорости
#define IN1_R 2   // Направление 1
#define IN2_R 3   // Направление 2

// Константы скорости
const int MIN_SPEED = 60;   // Минимальная рабочая скорость (0-255)
const int MAX_SPEED = 255;  // Максимальная скорость
const int ACCEL_STEPS = 100; // Шагов разгона
const int ACCEL_TIME = 10000; // Время разгона (мс)
const int STEP_DELAY = ACCEL_TIME / ACCEL_STEPS; // Задержка между шагами

void setup() {
  // Настройка пинов управления как выходы
  pinMode(ENA_R, OUTPUT);
  pinMode(IN1_R, OUTPUT);
  pinMode(IN2_R, OUTPUT);
  
  // Изначальная остановка мотора
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, LOW);
  analogWrite(ENA_R, 0);
}

void loop() {
  // ИСПРАВЛЕНИЕ: ПРАВИЛЬНОЕ НАПРАВЛЕНИЕ ВПЕРЕД
  // Разгон вперед (0 → 100% за 10 сек)
  digitalWrite(IN1_R, HIGH);  // ИСПРАВЛЕНО
  digitalWrite(IN2_R, LOW);   // ИСПРАВЛЕНО
  accelerateMotor(MIN_SPEED, MAX_SPEED);
  
  // Остановка на 3 секунды
  brakeMotor();
  delay(3000);
  
  // ИСПРАВЛЕНИЕ: ПРАВИЛЬНОЕ НАПРАВЛЕНИЕ НАЗАД
  // Разгон назад (0 → 100% за 10 сек)
  digitalWrite(IN1_R, LOW);   // ИСПРАВЛЕНО
  digitalWrite(IN2_R, HIGH);  // ИСПРАВЛЕНО
  accelerateMotor(MIN_SPEED, MAX_SPEED);
  
  // Финальная остановка
  brakeMotor();
  
  // Остановка программы
  while(true);
}

// Функция плавного разгона/торможения
void accelerateMotor(int startSpeed, int endSpeed) {
  float speedIncrement = (endSpeed - startSpeed) / (float)ACCEL_STEPS;
  float currentSpeed = startSpeed;
  
  for(int i = 0; i < ACCEL_STEPS; i++) {
    analogWrite(ENA_R, (int)currentSpeed);
    currentSpeed += speedIncrement;
    delay(STEP_DELAY);
  }
  analogWrite(ENA_R, endSpeed); // Гарантия достижения максимума
}

// Функция остановки мотора
void brakeMotor() {
  analogWrite(ENA_R, 0);
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, LOW);
}