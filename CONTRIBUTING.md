# Contributing

## Workflow

1. Create a focused branch from `main`.
2. Keep protocol changes aligned with the firmware contract in `docs/protocol-contract.md`.
3. Add or update tests with every behavior change.
4. Build on the target platform before opening a pull request.
5. Describe platform-specific behavior and any hardware validation performed.

## Code conventions

- Use C++20.
- Prefer clear ownership and RAII.
- Keep blocking network operations out of the UI thread.
- Use Qt signals/slots for UI updates from asynchronous services.
- Avoid inventing or silently changing wire-protocol fields.
- Never commit credentials, private keys, firmware images or device passwords.
