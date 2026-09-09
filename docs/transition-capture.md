# Transition capture

Version 0.2.0 adds passive evidence collection. It does not change the normal/fault classifier or add a Home Assistant entity. Every change of the interpreted status opens a trace with up to **15 seconds before** and **20 seconds after** the transition. Clean transitions, Unknown, Fault and startup acquisition are all recorded.

The ESP32 retains its latest **64 trace windows**, each with at most **256 events**, in approximately 270 KB of PSRAM. A rolling buffer supplies the lead-in. There are no flash writes or background trace uploads. Allocation failure disables capture while leaving classification working. Rebooting loses uncollected traces; a different boot identifier prevents records from separate boots being joined. Full buffers explicitly report truncated events or overwritten windows.

## What is recorded

- LED rising/falling timestamps, rising-edge intervals, pulse widths and green signal.
- One-second samples of analyzed frame rate, cumulative frame gaps and signal.
- Frame gaps over 150 ms, image validation/decode errors and recovery.
- Completed normal and fault group observations: pulse count, interval, candidate validity, matching streak and confirmed normal pattern/fault code.
- Status transitions, the decoder's reason for Unknown, the threshold and current recognized fault code.

A generic acquiring reason can follow an earlier dropped frame. Inspect the raw lead-in events as well as the reason on the transition. No images, Wi-Fi credentials or water-heater control commands are part of a trace.

## Collect a batch

Install the updated firmware with all four headers alongside `water-heater.yaml`: `blink_decoder.h`, `led_camera.h`, `transition_recorder.h`, and `trace_export.h`.

Install the collector dependencies on a computer that can reach the ESP32:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements-collector.txt
```

Create a private `connection.json` containing `host` (your device hostname or IP) and `api_key` (its ESPHome API encryption key). Do not commit this file; keep it readable only by your account. Then run:

```sh
python tools/collect_traces.py --connection connection.json --output captures --target-cycles 20
python tools/analyze_traces.py captures
```

Each invocation connects briefly, reads the completed windows, and disconnects. It uses the encrypted `get_transition_trace` API action directly. It neither subscribes to images/logs nor writes to HA. The action is also available to compatible HA clients for manual diagnosis, but nothing calls it periodically by default.

Run collection periodically, for example every 30 minutes, using a scheduler on an available computer. The firmware captures while the computer is offline; the finite RAM buffer and reboots still limit retention. No scheduler is installed by the public source package. Captures are written atomically under `captures/traces/<boot-id>/`, with a collection checkpoint and `summary.json`. Retries skip already saved traces. No data is removed from the ESP32 by collection.

The summary counts complete **observed idle → heating → idle cycles** and reports how many starts/stops contained Unknown. Startup acquisition, missing trace sequences, faults and incomplete windows are handled conservatively. A target of 20 is an initial sample size, not proof that an effect always happens. Once enough cycles are available, inspect timing and image-health evidence before changing the classifier.

## Format and timing

Schema 1 exports at most 16 events per page. Request `sequence: 0, offset: 0` for the index. Then request a ready sequence with `offset: 0` and follow `next_offset` until it is -1. Pending windows cannot be exported; completed windows are immutable until eviction. The collector verifies boot ID, sequence, offsets and event counts before saving.

Events are compact arrays `[millis, kind, a, b, signal_times_10, flags]`:

| Kind | Meaning | a | b | flags |
|---|---|---|---|---|
| 1 | LED rises | Interval since prior rise, ms | Cumulative pulse count | 0 |
| 2 | LED falls | Pulse width, ms | Cumulative pulse count | 0 |
| 3 | Image health | Analyzed fps × 100 | Cumulative frame gaps | Classification enabled |
| 4 | Frame gap | Time since last valid frame, ms | Cumulative frame gaps | 0 |
| 5 | Image error/recovery | 7: dimensions, 8: decode, 0: recovered | 0 | 0 |
| 6 | Normal group | Group period, ms | Packed count/streak/confirmed pattern | Candidate valid |
| 7 | Fault group | Gap before group, ms | Packed count/streak/confirmed code | Candidate valid |
| 8 | Status change | Previous status | New status | Unknown reason |

Packed group values use pulse count in bits 0–7, streak in 8–15 and confirmed value in 16–23. Normal confirmed values are 0: none, 1: idle, 2: heating. Status codes are 0: Unknown, 1: Not running, 2: Running, 3: Fault, 255: startup. Reason codes are listed in `tools/analyze_traces.py` and `transition_recorder.h`.

Event timestamps are the ESP32's unsigned 32-bit milliseconds. Relative differences across wraparound must use unsigned arithmetic. The collector estimates UTC from device uptime and the midpoint of the API request, and records the round-trip duration. These UTC timestamps are approximate; the on-device intervals are the primary timing evidence. Fetch traces within 49 days of capture to avoid ambiguity in the 32-bit clock.

The headers contain read-only instrumentation of decoder decisions. The C++ tests cover overlapping windows, wraparound, truncation, eviction and unchanged classification. Python tests cover cycle counting, Unknown bridges and gaps/reboots.
