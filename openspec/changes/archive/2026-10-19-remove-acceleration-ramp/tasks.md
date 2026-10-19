## 1. Прошивка

- [x] 1.1 Удалить из `Wheel` поля `rampedLinear`, `rampFrom`, `rampStartMs`, `ramping` и метод `updateRamp()`.
- [x] 1.2 Упростить `setTarget()` до мгновенного применения уставки: `targetLinear = target;` (без `rampFrom`/`rampStartMs`/`ramping`).
- [x] 1.3 Удалить неиспользуемую константу `RAMP_DURATION_MS`.
- [x] 1.4 Убрать вызов `updateRamp()` из `controlLoop()`; в `pidUpdate()` передавать `targetLinear` вместо `rampedLinear`.
- [x] 1.5 Убедиться, что `set_speed()`, `stop()`, `power_stop()` и перебазирование `prevTicks`/`lastControlMs`/`speedFilterInit` не изменились по смыслу.
- [x] 1.6 Скомпилировать прошивку, зафиксировать размер флеша/ОЗУ (ожидается уменьшение) и отсутствие новых предупреждений.

## 2. Проверка

- [x] 2.1 Прошить и прогнать на подвесе при `v = 120` и `v = 150` мм/с тем же harness, что и в `balance-wheel-speeds`.
- [x] 2.2 Сравнить с прежним логом: 0 сбросов ПИД, сходимость и ровность не хуже; ступенчатая команда отрабатывает без задержки.
- [x] 2.3 Проверить, что формат кадра телеметрии не изменился и `controller.py` его по-прежнему парсит.

## 3. Документация и архив

- [x] 3.1 `openspec validate remove-acceleration-ramp --strict`.
- [x] 3.2 Архивировать change (`openspec archive remove-acceleration-ramp --yes`).
- [x] 3.3 Вручную убрать упоминание «acceleration ramping» из строки `Purpose` в `openspec/specs/motion-control/spec.md` (при архивации `Purpose` не обновляется автоматически).
