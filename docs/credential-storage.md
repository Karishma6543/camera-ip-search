# Credential storage: design and release gate

## Current state

The application requests camera credentials interactively when an ONVIF endpoint challenges the request. Credentials are not intentionally stored in the SQLite inventory. There is currently no persistent operating-system credential-vault integration. Do not describe the current behavior as encrypted credential storage.

## Required design

Persistent credential storage must be optional and must use the operating system's own user-scoped credential service. Do not add a home-grown encryption layer or store a database encryption key beside the database.

| Platform | Required provider | Expected scope |
|---|---|---|
| Windows | Windows Credential Manager (CredWrite/CredRead/CredDelete) | Current user |
| macOS | Keychain Services | Current login keychain; access scoped to the signed application where practical |
| Linux | Secret Service API over D-Bus (for example GNOME Keyring/KWallet implementations) | Current user session |

If a vault is unavailable, locked, or denied, the application must continue in session-only mode and clearly tell the user that credentials will not persist. It must never silently fall back to plaintext, SQLite, preferences, logs, crash reports, or diagnostic exports.

## Credential identity

Use a stable, non-secret target key derived from normalized protocol, host, port, service path and account name. Never include the password in the key, log, UI accessibility name, telemetry or operation history. Separate credentials by endpoint and account; do not reuse credentials across different hosts automatically.

## Required interface

Introduce a small provider abstraction with `store(target, username, secret)`, `retrieve(target)`, `remove(target)` and `availability()` returning Available, Locked, Unavailable or Unsupported. Errors must never contain secret material. Keep provider calls outside the UI event loop when they can block. Clear temporary secret buffers on a best-effort basis after use. Do not persist a password merely because it was entered; require an explicit “Remember on this device” choice and explain where it is stored.

## Acceptance tests

- Store/read/delete round trip on each supported desktop OS.
- Locked keychain / unavailable Secret Service / access denied.
- Unicode account names and passwords.
- Multiple accounts for one camera endpoint.
- Credential rotation and stale-entry removal.
- No credential value in SQLite, logs, diagnostics, crash output or exported inventory.
- No fallback to plaintext when a provider fails.
- Clean uninstall does not delete user vault entries without explicit user action.

## Release status

**Not implemented / not validated.** The platform SDKs and Linux Secret Service runtime are not available in this repository's current build configuration. This document is the implementation contract, not evidence that credential-vault support exists. Add the provider only with platform-specific automated tests and a no-plaintext fallback test on all three CI operating systems.
