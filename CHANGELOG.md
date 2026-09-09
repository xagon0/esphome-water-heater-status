# Changelog

## 0.1.0 — 2026-09-08

Initial experimental public release.

- Freenove ESP32-WROVER-E camera configuration for an isolated green water-heater status LED.
- Local WT8840 single/strobe classification and supported slow fault-group recognition.
- One primary HA status sensor: Running, Not running, Fault or Unknown.
- Publish status only on change; optional camera and calibration controls disabled by default.
- Minutely numeric diagnostics internal by default and available for temporary calibration.
- Installation and calibration instructions, compatibility limits, MIT license and sanitizer tests.
- CI validates and compiles the ESPHome configuration with dummy credentials.
