# Camera IP Search

A cross-platform desktop utility for discovering and provisioning supported IP cameras on a local network.

**Status:** Early scaffold — the Qt desktop shell and project documentation are in place. Device discovery and configuration are not implemented yet.

## Technology

- C++20
- Qt 6 Widgets and Qt Network
- CMake
- SQLite (planned for local settings, device history and operation logs)
- Windows, macOS and Linux

## Current application

The initial application opens a desktop window with a camera inventory table and status area. The discovery action remains disabled until the firmware protocol adapter is implemented and tested.

## Build

Install CMake 3.21+, a C++20 compiler, and Qt 6.5+ with the Widgets, Network and Sql modules.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the executable:

```bash
./build/camera-ip-search
```

See [docs/development.md](docs/development.md) for setup details.

## Architecture and protocol

- [Architecture](docs/architecture.md)
- [Development setup](docs/development.md)
- [Firmware communication contract](docs/protocol-contract.md)
- [Contributing](CONTRIBUTING.md)

The proprietary camera discovery protocol uses UDP port **9988**. ONVIF WS-Discovery on UDP port **3702** is a separate mechanism.

We will implement packet handling only against the agreed firmware contract. Unknown response, activation and network-configuration schemas must be confirmed before the application sends those commands.

## Planned milestones

1. Desktop shell and build system
2. Network interface selection and UDP camera discovery
3. Device inventory and detail view
4. Activation and network configuration
5. Batch configuration with per-device results
6. Firmware upgrade workflow
7. Windows, macOS and Linux packaging and hardware validation

## Safety

The application will not embed default camera credentials or log passwords. High-impact operations (activation, network changes, batch changes and firmware upgrades) will require explicit confirmation.
