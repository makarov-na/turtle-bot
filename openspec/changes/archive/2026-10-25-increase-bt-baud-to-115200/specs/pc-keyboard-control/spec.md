## MODIFIED Requirements

### Requirement: Serial port is supplied as a launch argument

The application SHALL accept the serial port device as its single command-line argument (for example `/dev/rfcomm0`) and SHALL connect at 115200 baud. If the port is missing or cannot be opened, the application SHALL print a clear error and exit.

#### Scenario: Port provided and opened

- **WHEN** the application is launched with a valid device path
- **THEN** it connects to the device at 115200 baud and starts sending commands

#### Scenario: Port missing

- **WHEN** the application is launched with no argument
- **THEN** it prints usage information and exits without an error trace

#### Scenario: Port cannot be opened

- **WHEN** the device path is invalid or the device is in use
- **THEN** the application prints a readable error message and exits
