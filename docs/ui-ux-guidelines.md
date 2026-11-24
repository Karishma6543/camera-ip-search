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

## Current redesign scope

The current UI refresh establishes the neutral/red theme, reduces the inventory table to its operational columns, moves technical metadata into details, removes unfinished Bulk Config and NVR Channels placeholders from the active navigation, adds explicit inventory empty/filter states, makes device actions scrollable and replaces quick-input prompts with guided manual-entry and rename dialogs.

## Follow-up UI work still required

- Add visual regression/screenshot review on Windows, macOS and Linux at common DPI/scaling settings.
- Audit all modal dialogs, including static IPv4 preview/apply, restore confirmation, authentication, Help and Settings, for consistent wording and keyboard flow.
- Review table sorting, column visibility preferences and a dedicated device context menu.
- Add progress surfaces for discovery, subnet scans and reachability rather than relying only on the status bar.
- Add a complete first-run walkthrough and a focused troubleshooting/help surface.
- Conduct keyboard-only and screen-reader review on supported operating systems.
