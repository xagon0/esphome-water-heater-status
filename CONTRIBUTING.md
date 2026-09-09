# Contributing

Fork the repository and open a pull request. Small, documented changes are welcome, especially verified board configurations, captured timing reports and decoder improvements.

## Development checks

```sh
bash tools/test.sh
python3 -m unittest discover -s tests -p 'test_*.py'
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python tools/write_ci_secrets.py
esphome config water-heater.yaml
esphome compile water-heater.yaml
```

`write_ci_secrets.py` creates an ignored **dummy** secrets file for compilation only and refuses to overwrite an existing file. Never flash a build made with those dummy values. For actual installation, use your own secrets as described in the README. CI compiles with dummy credentials and does not publish firmware binaries.

The C++17 host tests use AddressSanitizer and UndefinedBehaviorSanitizer and need Clang or GCC. They exercise real timing scenarios: idle/strobe acquisition, invalid patterns, faults, state changes, dropped frames, stale input and clock wraparound. Add timing regressions for behavior changes; retain conservative Unknown handling.

## Reporting compatibility

Include the board model, camera module, exact control model if visible, ESPHome version, and measured pulse/group timing. State which results were physically observed versus inferred from a manual. Trim logs and photos to remove Wi-Fi names, credentials, device identifiers, home addresses and unrelated images. Do not intentionally cause heater faults to obtain traces.

Keep another control family's scheme separate and explain its source. A strobe can mean heating on one control and something else on another. Hardware/JPEG changes also need an ESPHome firmware build, since the host tests cover timing rather than camera hardware.

## Releases

Update the YAML project version, changelog and release notes. Run the checks above and confirm GitHub CI on the release commit. Create an annotated version tag, push it, and publish a source release with a SHA-256 checksum. Package tracked files with `git archive`; do not package a working directory containing credentials, captures or compiled firmware. Mark releases experimental until the documented hardware validation supports broader use.

By contributing, you agree your contribution is available under the repository's MIT license. ESPHome and its build dependencies retain their own licenses; this repository distributes only its own source and configuration.
