# Camera initialization adapter contract

## Purpose

This contract defines the safe extension point for camera activation. It is not itself an activation implementation and does not make any manufacturer/model supported.

## Required evidence before an adapter is enabled

An adapter must be tied to exact vendor, model family, hardware revision and firmware constraints. Its evidence must identify the official protocol/SDK reference, version and licensing/redistribution approval. Brand names, open ports, blank credentials, web-page load success and ONVIF discovery are not activation evidence.

## Probe semantics

- `AlreadyInitialized`: an authoritative documented response proves activation is complete.
- `ActivationRequired`: an authoritative documented response proves activation is required.
- `AuthenticationRequired`: the device requires credentials for the requested probe; do not infer activation state.
- `Unsupported`: the adapter has no documented operation for this exact target.
- `Unknown`: no authoritative state could be established.
- `Error`: a bounded protocol failure occurred; retain a sanitized diagnostic.

## Activation semantics

Only `CompletedAndVerified` permits the application to mark activation complete. `AcceptedButUnverified` is explicitly not success. Rejected, timeout, cancelled, unsupported and failed outcomes must remain distinct. Adapters must define idempotency, duplicate-request behavior, safe retry rules, password policy and recovery behavior before they are enabled.

## Security and execution requirements

- Adapter calls must run outside the GUI thread.
- Every operation must honor the deadline and cancellation context.
- Credentials are passed as a non-owning byte view; the caller owns the temporary buffer. Adapters must not retain, log, serialize or persist credentials.
- Never use default credentials, brute force, guessed CGI endpoints, undocumented packet formats or authentication bypass.
- Validate the selected device identity immediately before every state-changing request.
- Bound request/response sizes and retries; reject redirects or endpoints outside the verified device origin unless the documented protocol explicitly requires a separately validated service endpoint.
- Record protocol evidence and sanitized outcome, never secrets.

## Current implementation status

The C++ interface and contract tests establish the extension boundary. There are no CP PLUS, Prama or Hikvision activation adapters in this repository yet. Those cannot responsibly be implemented from brand names alone: obtain the authorized SDK/protocol documentation and exact hardware/firmware test units first.
