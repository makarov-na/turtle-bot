## 1. Тестовый скетч

- [x] 1.1 Создать `src/motor_controller/encoder_test/encoder_test.ino` (отдельная папка — Arduino компилирует .ino в одной папке в единую программу) с шапкой: назначение скетча, ссылка на копирование энкодерной логики из `motor_controller.ino`
- [x] 1.2 Скопировать энкодерную логику из `motor_controller.ino`: определения пинов A0-A3, константы (QUAD_TABLE, MIN_TICK_US, WHEEL_DIAMETER_MM, TICKS_PER_WHEEL_REV), struct/переменные счётчиков
- [x] 1.3 Перенести `updateEncoder`, дебаунс и `ISR(PCINT1_vect)` (PORTC, PCINT8-PCINT11) как в `motor_controller.ino`
- [x] 1.4 Добавить инициализацию в `setup()`: pins INPUT_PULLUP, PCICR/PCMSK1, Hardware Serial (9600) вместо btSerial/SoftwareSerial
- [x] 1.5 Реализовать цикл: раз в секунду читать тики с учётом `motorSign`, считать скорость в мм/с, отправлять строку через Hardware Serial (D0/D1, HC-06 подключён на D0/D1; без ANSI, построчно)

## 2. Проверка

- [x] 2.1 Скомпилировать `encoder_test.ino` (arduino-cli / Arduino IDE) без ошибок
- [ ] 2.2 Проверить вручную: HC-06 на D0/D1; при вращении колеса «вперёд» вывод через HC-06 показывает плюс для тиков, «назад» — минус; скорость mm/с отображается
- [ ] 2.3 Убедиться, что вывод при застое идёт раз в секунду через HC-06 (бинариально проверить, что строка приходит даже без движения)
