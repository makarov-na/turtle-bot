## Context

Текущий `motor_controller.ino` не компилируется. Причина: на ATmega328P библиотека `SoftwareSerial` объявляет `ISR(PCINT0_vect)` и алиасит `PCINT1_vect` и `PCINT2_vect` на него (`ISR(PCINT1_vect, ISR_ALIASOF(PCINT0_vect))`). Энкодеры используют `ISR(PCINT1_vect)` (A0-A3, PORTC), что даёт `multiple definition of '__vector_4'` на этапе линковки.

Проверочный скетч `encoder_test.ino` (change `add-encoder-test-sketch`, заархивирован) подтвердил: Hardware Serial (D0/D1) с подключённым HC-06 работает, конфликтов с энкодерами нет.

## Goals / Non-Goals

**Goals:**
- Устранить конфликт прерываний: заменить SoftwareSerial (D12/D13) на Hardware Serial (D0/D1) в `motor_controller.ino`
- Сохранить периодическую телеметрию «hello PC from ARDU» раз в секунду
- Оставить энкодеры и управление моторами без изменений

**Non-Goals:**
- Изменение логики энкодеров (QUAD_TABLE, ISR PCINT1, PID)
- Изменение формата телеметрии или частоты
- Переход на библиотеки типа AltSoftSerial — не требуется

## Decisions

**1. Переход на Hardware Serial (D0/D1) для HC-06.**
Убираем `SoftwareSerial` и объект `btSerial`, вместо них используем встроенный `Serial`. На ATmega328P Hardware UART не использует PCINT-вектора, поэтому конфликт с `ISR(PCINT1_vect)` исчезает. Решение проверено на реальном железе в `encoder_test.ino`.
Альтернативы:
- Оставить SoftwareSerial и убрать свой ISR PCINT1 → отклонено: теряем прерывания энкодеров (главная причина переноса их на PORTC).
- AltSoftSerial → отклонено: лишняя зависимость, для наших скоростей Hardware Serial избыточно и достаточно.

**2. HC-06 подключается к D0 (RX) / D1 (TX).**
Вместо level shifter достаточно делителя на TX→RX HC-06 (HC-06 RX не 5V-tolerant). HC-06 TX (3.3V) читается как HIGH на 5V-логике D0. Как в проверенном `encoder_test.ino`.

**3. Удаляется лишний код SoftwareSerial.**
`#include <SoftwareSerial.h>`, определения `PIN_BT_RX`/`PIN_BT_TX`, константа `BT_BAUD` заменяется на использование `Serial`. Все `btSerial.*` → `Serial.*`.

## Risks / Trade-offs

- **[Перепрошивка через USB]** → загрузчик использует D0/D1; HC-06 нужно отключать на время заливки. Документируется в шапке скетча.
- **[USB-монитор и HC-06 видят одни данные]** → это следствие аппаратной схемы Nano (D0/D1 идут на CH340). Не мешает: телеметрия идёт и туда, и туда.
- **[Расхождение с заархивированной спецой bluetooth-telemetry]** → спека из архива 2026-08-22 не была синхронизирована в `openspec/specs/`. Этот change обновляет её требования (D12/D13 → D0/D1, level shifter → делитель).