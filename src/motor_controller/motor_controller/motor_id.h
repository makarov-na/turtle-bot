// --- Общий идентификатор мотора --------------------------------------------
// Используется энкодерами, приводом и основным скетчем. Отдельный заголовок
// нужен, чтобы encoders.h и motors.h не зависели друг от друга по кругу.

#ifndef MOTOR_ID_H
#define MOTOR_ID_H

#include <stdint.h>

enum class MotorId : uint8_t { Left = 0, Right = 1, Count = 2 };

#endif
