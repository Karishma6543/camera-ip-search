# Oxita Camera Device Manager — UI/UX design rules

This document defines the visual and interaction contract for the Qt desktop application. New screens and changes to existing screens should follow it rather than introduce local styling or one-off patterns.

## Brand palette

| Token | Value | Use |
|---|---|---|
| Oxita red | `#C62836` | Primary action, selected navigation, keyboard focus and important brand accents |
| Deep red | `#AD202D` | Primary action hover |
| Charcoal | `#202329` | Headings and high-emphasis text |
| Body text | `#24272D` | Default readable content |
| Secondary text | `#737A84` | Supporting copy and metadata |
| Page grey | `#F2F3F5` | Main application background |
| Surface white | `#FFFFFF` | Tables, cards, dialogs and working areas |
| Border grey | `#E1E4E8` | Card boundaries and separators |
| Soft grey | `#F7F8F9` | Secondary surfaces and hover states |

Use red with restraint. Do not use it for every button, every icon, or large page backgrounds. White is the primary working surface; greys establish hierarchy; charcoal provides readable contrast. Status colors may use semantic colors, but every state must also have a text label.

## Page hierarchy

1. **Brand header:** Oxita logo and product name, with Help and Settings as secondary actions.
2. **Discovery toolbar:** search, network interface, manufacturer/status filters and a clearly dominant Discover cameras action.
3. **At-a-glance metrics:** discovered, reachable, ONVIF verified and needs-review counts.
4. **Primary inventory:** a compact, readable camera table with the most useful fields visible without horizontal scrolling at the standard desktop size.
5. **Context panel:** selected-device overview and the local network context.
6. **Workspaces:** device details, device actions and operation history. Unimplemented workflows must not be presented as active workspaces.

Technical identifiers such as MAC, firmware, discovery source, serial and hardware ID remain searchable and available in device details/export, but should not crowd the default inventory table.

## Interaction rules

- Every action needs a clear verb, a useful tooltip where the action is not self-explanatory, and a visible outcome.
- Use a single primary action per toolbar or dialog. Secondary actions remain neutral.
- Do not use a disabled button to advertise a feature that has no implementation. Keep future capabilities in documentation or a clearly labeled roadmap, not in the active workflow.
- Empty inventory, no filter matches, loading, cancellation, success and failure must each have a distinct explanation and next step.
- Forms use visible field labels, examples, optional/required indicators, validation near the relevant field and explicit Save/Cancel actions.
- Destructive or connectivity-changing actions require a target summary and confirmation that explains the recovery risk.
- Do not claim a manually added device is discovered, reachable or verified.
- Preserve user selections and inventory when scans are cancelled or refreshed.
- Keep long action workspaces scrollable instead of allowing controls to fall below the visible window.

## Dialog and form conventions

- Use a short title that names the task.
- Start with one sentence explaining what the dialog does.
- Group related fields in a form with consistent spacing.
- Mark optional fields explicitly.
- Use placeholders for examples, not as substitutes for labels.
- Keep Save/Apply as the primary action and Cancel as secondary.
- Validate before closing the dialog; keep the dialog open and focus the invalid field when validation fails.

## Accessibility and responsive behavior

- Maintain readable contrast between text and surfaces.
- Provide tooltips and accessible names for icon-only or abbreviated controls.
- Keep keyboard focus visible using the Oxita red focus border.
- Avoid communicating status by color alone.
- Keep primary inventory fields visible at the minimum supported desktop size; move secondary technical data into details rather than forcing horizontal scrolling.
- Ensure long content can scroll and dialogs fit within smaller displays.

## Current implementation

The current desktop workspace includes the Oxita neutral/red theme, a resizable inventory/details split, scrollable device profiles and action workspaces, searchable technical metadata, explicit empty states, row sorting, a header context menu for column visibility, and a shared progress indicator for discovery, subnet scanning and reachability.

Camera network configuration supports a guarded single-camera static IPv4 workflow and a sequential bulk IPv4 workflow. Bulk mode requires checked or selected cameras, refreshed ONVIF interface details, a unique new address per camera, local validation and per-camera result/history reporting. The selected camera's discovered service host is kept separate from the ONVIF-reported interface address to avoid sending requests to a mismatched endpoint.

Camera maintenance exposes ONVIF \`SetSystemFactoryDefault\` with explicit Soft/Hard mode selection, a target summary and two confirmation steps. This is a protocol request, not a universal vendor reset guarantee: the camera must expose the ONVIF operation, authenticate the request and implement the selected reset mode. A reset can erase settings and change the camera's IP address.

Backup/restore, diagnostics export, settings storage details, Help, operation history, manual inventory entry and rename workflows remain available. Technical device data is selectable and long profile content scrolls rather than being clipped.

## Remaining production validation

- Run the latest Windows, macOS and Ubuntu CI matrix and resolve every test failure.
- Visually review common display scaling and smaller window sizes on all supported operating systems.
- Verify static IPv4, bulk assignment and factory reset against real ONVIF cameras from multiple vendors; simulator/loopback tests cannot prove physical-camera interoperability.
- Add vendor-specific configuration adapters where a camera does not implement the relevant ONVIF operation.
- Complete keyboard-only and screen-reader review, including focus order in dialogs and the table's column menu.
- Add automated tests for column visibility, sorting, long-value wrapping, all dialog validation paths and partial failures in a multi-camera batch.
- Add a guided first-run walkthrough and a focused troubleshooting surface.
