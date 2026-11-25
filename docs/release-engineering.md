# Release engineering and distribution

## Artifact workflow

The `Desktop release artifacts` workflow can be run manually or by pushing a version tag (`v*`). It creates:
- Windows ZIP containing the installed executable and Qt runtime deployment from `windeployqt`.
- macOS ZIP containing the Qt-deployed application bundle.
- Linux tarball containing the installed executable.

The Linux tarball is explicitly named `runtime-required`: it is not an AppImage and requires a compatible Qt 6 runtime and platform plugins on the target system. Do not advertise it as a self-contained Linux installer.

These are unsigned CI artifacts for evaluation. They are not signed installers, notarized builds, auto-updates or a production release.

## Release gates

Before publishing a public release:
1. Build from an immutable tag and record the exact commit SHA.
2. Run the full Windows, macOS and Ubuntu test matrix and sanitizer suite.
3. Run CodeQL and dependency review; review findings rather than suppressing them blindly.
4. Generate and review a software bill of materials (SBOM) and third-party license inventory.
5. Verify the license obligations for Qt and every linked/runtime dependency. The repository's current `LICENSE` is marked "Other" and is not a substitute for a reviewed distribution license.
6. Test clean install, upgrade, uninstall, data preservation and rollback on clean VMs for each supported OS.
7. Sign Windows executables/installers with the organization's code-signing certificate.
8. Sign and notarize macOS bundles using the organization's Apple Developer identity.
9. Build a real Linux AppImage or distribution package, test it on the supported distributions, and document glibc/Qt/platform requirements.
10. Publish checksums, release notes, privacy notice, support contact and a model/firmware compatibility matrix.

Signing secrets and certificates must be stored in GitHub environment secrets or an external release-signing service. Never commit signing keys or provisioning profiles.

## Data safety

The inventory and application settings must survive supported upgrades. Uninstall must not silently delete user inventory. A rollback must not downgrade the database schema destructively; use a backup created before upgrade and test restoration.

## Release channels

Use separate development/preview and stable release channels. A release is eligible for stable only when all P0 gates in [production-readiness.md](production-readiness.md) are evidenced for the exact release commit. CI green alone is not a production approval.
