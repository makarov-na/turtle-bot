## MODIFIED Requirements

### Requirement: Incoming telemetry is drained while running

While connected, the application SHALL continuously read and discard incoming bytes from the serial port (the bot's keepalive acknowledgements). This keeps the terminal input buffer from filling up, which would otherwise stall the RFCOMM flow control and break the channel over time. Incoming acknowledgement lines SHALL additionally be parsed and rendered live in the telemetry dashboard (separate requirement below), so draining and displaying the data happen together.

#### Scenario: Acknowledgements are drained

- **WHEN** the bot sends `ack ...` lines every ~200 ms while the application is connected
- **THEN** the application reads them promptly so the input buffer does not grow without bound

#### Scenario: Acknowledgements are parsed for display

- **WHEN** a complete `ack` line with `mL/mR/oL/oR/iL/iR` fields arrives
- **THEN** the field values are parsed and pushed into the live telemetry dashboard, and the dashboard is redrawn

#### Scenario: Link closure mid-run is detected via read errors

- **WHEN** the channel closes while driving and a read or write on the port raises an I/O error (for example `EIO` on a dropped RFCOMM link)
- **THEN** the application treats it as a link loss and enters the auto-reconnect flow; an empty read on an idle tty (a no-op return with no data, which is normal on a `VMIN=0` port) is ignored and does not count as a closure

## ADDED Requirements

### Requirement: Status line shows absolute command values

The application SHALL display the current command with both the percentage of the maximum and the absolute value: linear speed in mm/s (one decimal place) and angular speed in rad/s (two decimal places), matching the format of the outgoing `set_speed` command.

#### Scenario: Linear value shown

- **WHEN** the linear component is +30%
- **THEN** the status line shows the percentage (`+30%`) together with the absolute value (`90.0 мм/с`)

#### Scenario: Angular value shown

- **WHEN** the angular component is -10%
- **THEN** the status line shows the percentage (`-10%`) together with the absolute value (`-0.32 рад/с`)

### Requirement: Live telemetry dashboard

While connected, the application SHALL run in a full-screen mode using the terminal alternate screen buffer and SHALL continuously render a telemetry dashboard: a status line on top, below it a table of the **20 most recent** acknowledgement lines (newest row on top), and a footer with control hints. The live dashboard SHALL be updated on each new acknowledgement (approximately every 200 ms). The most recent row SHALL be visually highlighted. All internal messages (connection status, link-loss notices, hints) SHALL be rendered inside the dashboard, not written over it. When the application exits, it SHALL leave the alternate screen buffer and restore the normal terminal screen.

#### Scenario: Dashboard shows last 20 acknowledgements with newest on top

- **WHEN** more than 20 acknowledgements have arrived
- **THEN** the table shows exactly the 20 most recent ones, with the newest row at the top of the table

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