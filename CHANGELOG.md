# Changelog

## 0.2.0 — 2026-09-08

- Passive PSRAM capture of 15-second pre-transition and 20-second post-transition windows.
- Raw pulse timing, decoder group observations, frame gaps and Unknown reasons.
- Read-only paginated API export with no additional HA entities or background uploads.
- Local collector, complete-cycle summary, retention/overflow markers and tests.
- Existing normal/fault classification behavior retained.

## 0.1.0 — 2026-09-08

Initial experimental public release.

- Freenove ESP32-WROVER-E camera configuration for an isolated green water-heater status LED.
- Local WT8840 single/strobe classification and supported slow fault-group recognition.
- One primary HA status sensor: Running, Not running, Fault or Unknown.
- Publish status only on change; optional camera and calibration controls disabled by default.
- Minutely numeric diagnostics internal by default and available for temporary calibration.
- Installation and calibration instructions, compatibility limits, MIT license and sanitizer tests.
- CI validates and compiles the ESPHome configuration with dummy credentials.
