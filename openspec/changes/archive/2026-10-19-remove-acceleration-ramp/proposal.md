## Why

Рампа разгона фактически отключена: `RAMP_DURATION_MS = 1`, а `controlLoop()` запускается не раньше чем через
`PID_PERIOD_MS = 10` мс после `setTarget()`, поэтому `t >= 1` уже на первой итерации. Механизм не работает, но
оставляет мёртвый код (`rampedLinear`, `rampFrom`, `ramping`, `updateRamp()`), лишний вызов в контуре и расхождение
со спекой: `motion-control` требует «Smooth acceleration», которого нет.

Рывки, ради которых рампу отключили (`01480d9`), давало квантование энкодера (закрыто EMA в `balance-wheel-speeds`)
и блокирующая телеметрия, а не ступенька уставки. Решение — убрать рампу и привести спеку в соответствие.
Ограничение пускового тока при необходимости вернут отдельным change'ем.

## What Changes

- Из `Wheel` удаляются поля и метод рампы: `rampedLinear`, `rampFrom`, `rampStartMs`, `ramping`, `updateRamp()`;
  из `controlLoop()` убирается вызов `updateRamp()`.
- `setTarget()` упрощается до мгновенного применения уставки (`targetLinear = target`); ПИД и прямая связь (FF)
  продолжают работать от `targetLinear`. Перебазирование `prevTicks`/`lastControlMs` при входе в `STATE_RUN`
  сохраняется без изменений.
- Удаляется неиспользуемая константа `RAMP_DURATION_MS`.
- Из спеки `motion-control` удаляется требование **Smooth acceleration**.
- **Поведение не меняется**: `set_speed` уже сейчас ступенчатый при `RAMP_DURATION_MS = 1`.
- Формат кадра телеметрии, протокол команд, `controller.py` и логика клиппинга/кинематики не затрагиваются.

## Capabilities

### New Capabilities

<!-- нет -->

### Modified Capabilities

- `motion-control`: удаляется требование «Smooth acceleration» (рампа уставки). Требование «Closed-loop wheel speed
  control» без изменений продолжает описывать слежение, FF, перебазирование и сходимость.

## Impact

- `src/motor_controller/motor_controller/motor_controller.ino` — структура `Wheel`, `controlLoop()`, константа
  `RAMP_DURATION_MS`. Прошивка должна стать меньше по флешу/ОЗУ за счёт удаления полей и кода.
- `openspec/specs/motion-control/spec.md` — удаление требования «Smooth acceleration» (через дельту этого change'а
  при архивации).
- Проверка: живой прогон на подвесе при `v = 120` и `v = 150` должен воспроизвести прежнее поведение (рампа и так
  не работала), регрессий по сходимости и ровности нет.
