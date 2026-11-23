# Security and privacy review checklist

This is a review aid, not a security certification. Complete it against the exact release commit and supported device/firmware matrix.

## Current controls in source

- Discovery parses bounded UDP payloads and validates the documented Contract B magic, version, command, sequence and field types.
- ONVIF XAddrs are restricted to same-host HTTP(S) URLs and reject user information, query strings and fragments.
- Device-service redirects are disabled for ONVIF requests; request timeouts are applied and incoming SOAP responses are aborted once more than 1 MiB is buffered.
- Device identity merging avoids merging conflicting MAC addresses.
- Network writes require a matching ONVIF interface token and adapter-declared implemented operations.
- No default camera credentials or credential guessing are included.
- Application logs rotate at 2 MiB with three archives and redact common key/value, Bearer, URL user-info and XML secret patterns.
- Diagnostic export requires an explicit user confirmation and excludes the inventory database and ONVIF interface tokens.
- SQLite integrity is checked before schema changes; an invalid database is left untouched.

## Release blockers

- [ ] Review every log call and network error path for passwords, Authorization headers, cookies, WS-Security UsernameToken material, tokens and full SOAP payloads.
- [x] Add regression tests for common key/value, Bearer, URL user-info and XML secret redaction.
- [x] Add automated tests for log rotation and three-archive retention.
- [ ] Expand redaction edge cases, including multiline credentials, cookie headers, Authorization variants and malformed XML.
- [ ] Perform a source-level threat review of XML entity handling, XML depth/size, JSON nesting, URL parsing, redirects, DNS rebinding and endpoint trust.
- [x] Explicitly abort ONVIF requests when Qt reports TLS certificate validation errors; no certificate errors are ignored.
- [ ] Add cross-platform tests for trusted, expired, hostname-mismatched and self-signed certificates and verify operator guidance.
- [ ] Implement OS credential-vault integration before offering persistent credential storage. Until then, credentials remain session-only and must not be cached in app settings or SQLite.
- [ ] Review ONVIF authentication behavior, including WS-Security UsernameToken digest/nonce/timestamp requirements for target devices. HTTP Basic challenge support alone is not sufficient for every ONVIF camera.
- [ ] Review database file permissions and whether local inventory metadata is sensitive in the deployment environment.
- [ ] Review support-bundle contents on each OS and ensure users can inspect the file before sharing.
- [ ] Run dependency/SBOM and license review for the exact release build.
- [ ] Run static analysis and compiler sanitizers where supported.
- [ ] Test malformed, oversized, truncated and adversarial network inputs on all supported platforms.
- [x] Add a local oversized-SOAP regression test for the 1 MiB ONVIF response guard (execution still awaits a functioning CI runner).
- [ ] Verify that cancellation and shutdown abort pending sockets/replies and do not leave worker threads running.

## Credential-vault policy

Do not add a home-grown encryption key stored beside the database. Use a maintained OS-backed credential store integration (Windows Credential Manager, macOS Keychain, Linux Secret Service/libsecret) with an explicit user opt-in and a documented fallback when the service is unavailable. No credential-vault implementation is currently claimed.

## Diagnostic export privacy

The diagnostics text export can contain local IPv4 addresses, interface names, device IP targets, operation timestamps and error text. It excludes the SQLite inventory and ONVIF interface tokens. Log redaction is best-effort, not a proof that every secret format is removed. Operators must inspect the export before sending it to support.
