# Flipper Wedge Documentation

This directory contains the technical reference for building, testing, and maintaining Flipper Wedge across supported Flipper Zero firmware projects.

## Start here

- [Architecture patterns](ARCHITECTURE_PATTERNS.md) explains the application structure and the compatibility patterns that should remain stable.
- [Quick reference](QUICK_REFERENCE.md) collects common build commands, troubleshooting steps, and release checks.
- [Firmware compatibility](FIRMWARE_COMPATIBILITY.md) records supported firmware versions and known differences.
- [Testing automation](TESTING_AUTOMATION.md) explains the automated checks and the manual hardware test process.
- [API migration log](API_MIGRATION_LOG.md) records changes made for firmware API compatibility.
- [Changelog](changelog.md) records user-facing changes by version.

The main [Flipper Wedge README](../README.md) covers installation, use, and the supported credential and HID workflows.

## Maintenance approach

Keep changes focused on confirmed defects, firmware compatibility, security, and requested behavior. Build against every supported firmware target affected by a change. Hardware-facing behavior such as NFC, RFID, USB HID, Bluetooth HID, settings persistence, and scan logging should be verified on a device before a release.

Preserve the existing state machine, nonblocking worker design, and saved settings behavior unless a documented compatibility issue requires a change.
