## MODIFIED Requirements

### Requirement: Live telemetry dashboard

While connected, the application SHALL run in a full-screen mode using the terminal alternate screen buffer and SHALL continuously render a telemetry dashboard: a status line on top, below it a table of the **50 most recent** acknowledgement lines (newest row on top), and a footer with control hints. The live dashboard SHALL be updated on each new acknowledgement (approximately every 200 ms). The most recent row SHALL be visually highlighted. All internal messages (connection status, link-loss notices, hints) SHALL be rendered inside the dashboard, not written over it. When the application exits, it SHALL leave the alternate screen buffer and restore the normal terminal screen.

#### Scenario: Dashboard shows last 50 acknowledgements with newest on top

- **WHEN** more than 50 acknowledgements have arrived
- **THEN** the table shows exactly the 50 most recent ones, with the newest row at the top of the table

#### Scenario: Latest row is highlighted

- **WHEN** the dashboard is drawn
- **THEN** the newest acknowledgement row is visually distinguished (inverted colors)

#### Scenario: Messages render inside the dashboard

- **WHEN** the application needs to notify the user (for example "link lost, reconnecting")
- **THEN** the message is displayed in a slot of the dashboard and the panel is re-rendered cleanly, without mixing with the live table

#### Scenario: Dashboard clears after reconnect

- **WHEN** the connection is re-established after a link loss
- **THEN** the telemetry table is cleared so stale values from the dead link are not shown alongside fresh ones

#### Scenario: Screen restored on exit

- **WHEN** the application exits via `q`, Ctrl+C, or exhausted reconnection attempts
- **THEN** the alternate screen buffer is left, the cursor visibility is restored, and the terminal shows the normal previous content

### Requirement: Панель замораживается во время паузы

Во время паузы приложение SHALL NOT перерисовывать панель в ответ на входящие подтверждения или клавиши-стрелки: статусная строка и таблица из 50 строк телеметрии SHALL оставаться ровно такими, какими были в момент паузы. Входящие строки подтверждения SHALL по-прежнему читаться, парситься и помещаться в кольцо телеметрии, чтобы таблица сразу после resume показывала свежее состояние.

#### Scenario: Приход ack на паузе не перерисовывает панель

- **WHEN** во время паузы приходит строка `ack`
- **THEN** телеметрия парсится и сохраняется, но панель не перерисовывается

#### Scenario: Стрелки на паузе не перерисовывают панель

- **WHEN** пользователь нажимает стрелку во время паузы
- **THEN** команда не меняется, ничего не отправляется, и панель не перерисовывается

#### Scenario: Панель показывает свежую телеметрию на resume

- **WHEN** пользователь возобновляет движение, а в кольцо телеметрии за время паузы накопились подтверждения
- **THEN** панель перерисовывается на resume и показывает текущие строки подтверждений
