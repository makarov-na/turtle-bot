# PC Keyboard Control

## Purpose
Консольное Python-приложение на ПК для управления роботом по Bluetooth: чтение стрелок в raw-режиме, дискретные инкременты ±10%, heartbeat 200 мс, graceful shutdown и автореконнект при обрыве линка.

## Requirements

### Requirement: Console app reads arrow keys without Enter

The application SHALL run a console (terminal) session in raw mode and SHALL recognize the arrow keys Up, Down, Left, Right, and the exit key `q`, reacting to a key press immediately without requiring the Enter key. Terminal contents and behavior SHALL be restored when the application exits.

#### Scenario: Arrow key press is detected immediately

- **WHEN** the user presses an arrow key while the application runs
- **THEN** the application reacts to that key press without waiting for Enter

#### Scenario: Exit key recognized

- **WHEN** the user presses `q` or sends Ctrl+C
- **THEN** the application shuts down and restores the terminal

### Requirement: Discrete speed increments

Starting from `set_speed(0, 0)`, each arrow key press SHALL change the held command by exactly 10% of the configured maximum: Up increments linear speed, Down decrements linear speed, Left increments angular speed, Right decrements angular speed. Linear and angular components SHALL be maintained independently (a forward-and-turn command keeps both components). Each component SHALL be capped so it never exceeds ±100% of its maximum, and Down beyond 0% SHALL continue into negative linear speed (reverse driving).

#### Scenario: Initial command is zero

- **WHEN** the application starts and connects
- **THEN** it sends `set_speed 0.0 0.0` as the initial command

#### Scenario: Linear increment

- **WHEN** the user presses Up once
- **THEN** the linear component becomes +10% of maximum and is sent as the new command

#### Scenario: Linear decrement into reverse

- **WHEN** the user presses Down while linear speed is at +10%
- **THEN** the linear component returns to 0%, and a further Down press drives into negative (reverse) territory

#### Scenario: Angular increment and decrement

- **WHEN** the user presses Left and then Right
- **THEN** the angular component first rises to +10% and then returns to 0% of the angular maximum

#### Scenario: Combined forward and turn

- **WHEN** the user presses Up followed by Left
- **THEN** the command keeps the linear increment and adds the angular increment, resulting in a turn while driving forward

#### Scenario: Cap at maximum

- **WHEN** the user presses Up ten times from zero
- **THEN** the linear component reaches +100% and additional Up presses do not raise it further

### Requirement: Command mapping matches firmware limits

The application SHALL treat +100% linear speed as `V_MAX_MM_S = 300` mm/s and +100% angular speed as `ω_max = 2·300/WHEELBASE_MM ≈ 3.24` rad/s, matching the constants defined in the firmware, and SHALL send numeric values to one decimal place for mm/s and two decimal places for rad/s.

#### Scenario: Ten percent step maps to firmware units

- **WHEN** the user presses Up once
- **THEN** the application sends `set_speed 30.0 0.0` (10% of 300 mm/s)

#### Scenario: Angular step maps to firmware units

- **WHEN** the user presses Left once
- **THEN** the application sends an angular value of approximately 0.32 rad/s (10% of ω_max)

### Requirement: Command repetition as heartbeat

While connected and running, the application SHALL resend the current command at a fixed interval of 200 ms, so the firmware watchdog (which stops the robot after a timeout) is reset, and so a host link drop is automatically detected on the firmware side.

#### Scenario: Steady heartbeat while idle

- **WHEN** the command is unchanged and the application is running
- **THEN** the current command is resent every 200 ms

#### Scenario: Heartbeat tone on each change

- **WHEN** the user changes the command with an arrow key
- **THEN** the new command is sent immediately and continues to be repeated every 200 ms

### Requirement: Graceful shutdown stops the robot

When the application exits, whether via `q`, Ctrl+C, or the reconnection attempts after a link loss being exhausted, it SHALL send a final `set_speed 0.0 0.0` before closing the connection so the robot does not continue moving with the last non-zero command.

#### Scenario: Quit via key sends stop

- **WHEN** the user presses `q`
- **THEN** the application sends `set_speed 0.0 0.0` and then closes the serial port

#### Scenario: Exit via Ctrl+C sends stop

- **WHEN** the user interrupts the application with Ctrl+C
- **THEN** the application sends `set_speed 0.0 0.0` and restores the terminal

#### Scenario: Exit after failed reconnect sends stop

- **WHEN** a link loss cannot be repaired within the reconnection attempts
- **THEN** the application sends `set_speed 0.0 0.0` and exits with an error message

### Requirement: Auto-reconnect on lost link

While running, if the connection to the robot is lost (for example a Bluetooth RFCOMM link that drops), the application SHALL automatically reconnect to the same port and resume sending the current command, keeping the current linear and angular components. It SHALL repeat the reconnection at most 5 times with a 0.5 s pause between attempts; if all attempts fail, the application SHALL exit per the graceful-shutdown requirement. A link loss SHALL not reset the user's held command.

#### Scenario: Link drop while driving resumes

- **WHEN** the connection drops while a non-zero command is held
- **THEN** the application reconnects and continues sending the same command without the user having to rebuild it from zero

#### Scenario: Unrecoverable link loss exits cleanly

- **WHEN** reconnection attempts all fail
- **THEN** the application prints a readable error message and exits with a final stop command

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

### Requirement: Serial port is supplied as a launch argument

The application SHALL accept the serial port device as its single command-line argument (for example `/dev/rfcomm0`) and SHALL connect at 9600 baud. If the port is missing or cannot be opened, the application SHALL print a clear error and exit.

#### Scenario: Port provided and opened

- **WHEN** the application is launched with a valid device path
- **THEN** it connects to the device at 9600 baud and starts sending commands

#### Scenario: Port missing

- **WHEN** the application is launched with no argument
- **THEN** it prints usage information and exits without an error trace

#### Scenario: Port cannot be opened

- **WHEN** the device path is invalid or the device is in use
- **THEN** the application prints a readable error message and exits