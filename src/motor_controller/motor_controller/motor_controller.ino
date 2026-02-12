// Определение пинов для правого мотора
#define ENA_R 5   // ШИМ-контроль скорости
#define IN1_R 2   // Направление 1
#define IN2_R 3   // Направление 2

// Определение пинов для левого мотора
#define ENB_L 6   // ШИМ-контроль скорости
#define IN3_L 4   // Направление 1
#define IN4_L 7   // Направление 2

// Константы скорости
const int MIN_SPEED = 60;   // Минимальная рабочая скорость (0-255)
const int MAX_SPEED = 255;  // Максимальная скорость
const int ACCEL_STEPS = 100; // Шагов разгона
const int ACCEL_TIME = 10000; // Время разгона (мс)
const int STEP_DELAY = ACCEL_TIME / ACCEL_STEPS; // Задержка между шагами
const int BRAKE_POWER = 150; // Мощность торможения (0-255)
const int BRAKE_TIME = 300;  // Время торможения (мс)

void setup() {
  // Настройка пинов управления как выходы
  // Правый мотор
  pinMode(ENA_R, OUTPUT);
  pinMode(IN1_R, OUTPUT);
  pinMode(IN2_R, OUTPUT);
  
  // Левый мотор
  pinMode(ENB_L, OUTPUT);
  pinMode(IN3_L, OUTPUT);
  pinMode(IN4_L, OUTPUT);
  
  // Изначальная остановка моторов
  brakeMotors();
}

void loop() {
  // Движение вперед с разгоном
  setRightMotorForward();
  setLeftMotorBackward();
  accelerateMotors(MIN_SPEED, MAX_SPEED);
  
  // Остановка на 3 секунды с активным торможением
  activeBrake();
  delay(3000 - BRAKE_TIME); // Компенсируем время торможения
  
  // Движение назад с разгоном
  setRightMotorBackward();
  setLeftMotorForward();
  accelerateMotors(MIN_SPEED, MAX_SPEED);
  
  // Финальная остановка с активным торможением
  activeBrake();
  
  // Остановка программы
  while(true);
}

// Установить направление "Вперед" для правого мотора
void setRightMotorForward() {
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, HIGH);
}

// Установить направление "Назад" для правого мотора
void setRightMotorBackward() {
  digitalWrite(IN1_R, HIGH);
  digitalWrite(IN2_R, LOW);
}

// Установить направление "Вперед" для левого мотора
void setLeftMotorForward() {
  digitalWrite(IN3_L, LOW);
  digitalWrite(IN4_L, HIGH);
}

// Установить направление "Назад" для левого мотора
void setLeftMotorBackward() {
  digitalWrite(IN3_L, HIGH);
  digitalWrite(IN4_L, LOW);
}

// Функция плавного разгона для обоих моторов
void accelerateMotors(int startSpeed, int endSpeed) {
  float speedIncrement = (endSpeed - startSpeed) / (float)ACCEL_STEPS;
  float currentSpeed = startSpeed;
  
  for(int i = 0; i < ACCEL_STEPS; i++) {
    analogWrite(ENA_R, (int)currentSpeed);
    analogWrite(ENB_L, (int)currentSpeed);
    currentSpeed += speedIncrement;
    delay(STEP_DELAY);
  }
  analogWrite(ENA_R, endSpeed);
  analogWrite(ENB_L, endSpeed);
}

// Функция пассивной остановки моторов
void brakeMotors() {
  // Правый мотор
  analogWrite(ENA_R, 0);
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, LOW);
  
  // Левый мотор
  analogWrite(ENB_L, 0);
  digitalWrite(IN3_L, LOW);
  digitalWrite(IN4_L, LOW);
}

// Функция активного торможения с обратным усилием
void activeBrake() {
  // Определение текущего направления
  bool rightForward = (digitalRead(IN1_R) == LOW && digitalRead(IN2_R) == HIGH);
  bool leftForward = (digitalRead(IN3_L) == LOW && digitalRead(IN4_L) == HIGH);
  
  // Подача обратного напряжения
  if(rightForward) {
    setRightMotorBackward();
  } else {
    setRightMotorForward();
  }
  
  if(leftForward) {
    setLeftMotorBackward();
  } else {
    setLeftMotorForward();
  }
  
  // Подача тормозного усилия
  analogWrite(ENA_R, BRAKE_POWER);
  analogWrite(ENB_L, BRAKE_POWER);
  
  // Длительность торможения
  delay(BRAKE_TIME);
  
  // Полная остановка
  brakeMotors();
}