# Architecture

## Goals

Camera IP Search is a native desktop utility for Windows, macOS and Linux. It will discover supported cameras on a local network, display device identity and status, activate devices, configure network settings in batches, and initiate firmware upgrades.

## Technology decisions

- **C++20** for the application and networking core.
- **Qt 6 Widgets** for the desktop interface.
- **Qt Network** for UDP, TCP and HTTP communication.
- **SQLite** for local preferences, device history and operation logs.
- **CMake** as the cross-platform build system.
- **Qt Test / GoogleTest** for automated tests.

## Proposed module boundaries

```text
src/
  app/                 Application composition and startup
  core/                Domain models and shared types
  discovery/           UDP discovery transport and protocol codec
  devices/             Device repository and device operations
  network/             Interface enumeration and network utilities
  provisioning/        Activation and network configuration workflows
  firmware/            Firmware transfer and upgrade orchestration
  storage/             SQLite repositories and migrations
  ui/                  Qt Widgets presentation layer
tests/
docs/
```

The initial repository contains the application shell. Modules will be added behind small interfaces as their wire contracts are confirmed.

## Dependency direction

The UI calls application services. Application services depend on domain interfaces. Network transports and SQLite repositories implement those interfaces. The domain layer must not depend on Qt Widgets.

## Safety and reliability principles

- Never ship a default camera password or store plaintext credentials.
- Require explicit user confirmation before activation, network changes, batch operations or firmware upgrades.
- Validate firmware metadata and integrity before sending an upgrade.
- Report per-device results for batch operations; do not present a batch as successful if some devices failed.
- Keep network I/O off the UI thread.
- Add timeouts, bounded retries and cancellation to network operations.
- Log operation metadata without logging passwords, tokens or sensitive payloads.

## Discovery constraints

The project protocol uses UDP port 9988. Standard ONVIF WS-Discovery uses UDP port 3702 and is a separate interoperability path. The discovery adapter must follow the agreed firmware contract exactly.

A limited broadcast such as 255.255.255.255 does not cross routers by itself. Discovery across routed subnets requires an explicitly designed per-interface scan, directed broadcast where permitted, or a relay/agent. The UI must not claim subnet-independent discovery until that behavior is implemented and tested.

## Current implementation status

- [x] Qt Widgets application shell
- [x] CMake build definition
- [ ] Network interface selection
- [ ] UDP discovery transport and Contract B codec
- [ ] Device model and device table binding
- [ ] SQLite schema and migrations
- [ ] Activation and network configuration workflows
- [ ] Batch operations
- [ ] Firmware upgrade workflows
- [ ] Cross-platform packaging and hardware validation
