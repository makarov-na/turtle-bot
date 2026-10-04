## MODIFIED Requirements

### Requirement: Live telemetry dashboard

While connected, the application SHALL run in a full-screen mode using the terminal alternate screen buffer and SHALL continuously render a telemetry dashboard: a status line on top, below it a table of the **50 most recent** acknowledgement lines (newest row on top), and a footer with control hints. The live dashboard SHALL be updated on each new acknowledgement (approximately every 200 ms). The most recent row SHALL be visually highlighted. All internal messages (connection status, link-loss notices, hints) SHALL be rendered inside the dashboard, not written over it. When the application exits, it SHALL leave the alternate screen buffer and restore the normal terminal screen.

Directly above the column header the dashboard SHALL render a two-line legend explaining what every table column means, separated from the column header by one blank line. The legend SHALL state the unit of measure for each column and SHALL distinguish the commanded speed columns from the measured ones. The legend SHALL be static text, SHALL NOT be a table row and SHALL NOT consume table space: the table SHALL still show 50 acknowledgement rows, so the whole panel occupies 58 lines. The column identifiers in the header SHALL remain the same short names as the `ack` protocol fields.

#### Scenario: Dashboard shows last 50 acknowledgements with newest on top

- **WHEN** more than 50 acknowledgements have arrived
- **THEN** the table shows exactly the 50 most recent ones, with the newest row at the top of the table

#### Scenario: Column legend is rendered above the header

- **WHEN** the dashboard is drawn
- **THEN** exactly two legend lines appear immediately above the column header line and below the status line, with one blank line between the legend and the column header, and neither of the legend lines is treated as a table row

#### Scenario: Legend tells commanded apart from measured

- **WHEN** the operator reads the legend
- **THEN** the legend names the unit of measure for each column and marks which columns hold the commanded speed and which hold the measured speed

#### Scenario: Legend does not reduce the number of table rows

- **WHEN** the panel is drawn with the legend present
- **THEN** the table area still occupies exactly 50 rows, padded with empty rows when fewer than 50 acknowledgements have arrived, and the panel is 58 lines tall

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