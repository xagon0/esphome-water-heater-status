# Calibration and troubleshooting

## First installation

1. Mount the camera over the external green status LED. Keep the view isolated from room light: mostly black between flashes, mostly green during each flash. The decoder averages the entire image; it does not find a small LED in a room scene.
2. In HA, open the ESPHome device and show its disabled entities. Enable **Classification enabled**, **LED green threshold**, **Camera exposure**, and optionally **Status LED view**. Enabling the switch entity makes the control available; you must then **turn that switch on** to start classification.
3. Begin with threshold **8** and exposure **100**. Turn classification on and wait at least 15 seconds for a normal pattern to be confirmed.
4. Compare the reported state with an independent observation during an ordinary idle period and an ordinary heating cycle. The expected patterns are in [compatibility.md](compatibility.md). Do not deliberately create an equipment fault for testing.
5. After calibration, keep the classification switch **on** but disable its HA entity and the other optional entities. HA entity disabling does not send a switch-off command. Its on/off value and the threshold restore across reboots. Each boot reacquires the LED pattern.

If you change mounting, threshold or exposure, reacquire and verify the result again. Camera exposure adjustments made through HA last until reboot; update the `camera_exposure` substitution and reflash to make an exposure change persistent. The threshold control restores its saved value, so changing its YAML initial value alone does not override a previously saved calibration.

## Inspecting the signal

For temporary minutely numeric diagnostics, set `diagnostics_internal: "false"` in the YAML substitutions and install that configuration. HA will expose green-signal peak, analyzed frame rate, flash interval, flash width, flash count, frame-gap count and Wi-Fi signal. The defaults keep these internal. Set the substitution back to `"true"` and reflash after diagnosis.

The green signal is the image-wide average of `max(0, green - max(red, blue))`. It turns on at the threshold and off below 60% of that threshold. The original mount measured a dark signal near zero and flashes around 45–65; other cameras and mounts require their own calibration. The diagnostic reports the maximum across a one-minute window, not instantaneous darkness or an average. To measure a dark baseline, cover the lens for a full diagnostic interval; Status will become Unknown.

For individual pulse timing, temporarily set `logger.level: DEBUG` and read ESPHome logs. `led_timing` logs rising-edge intervals, green signal and flash widths. Verbose logs and live camera viewing add network traffic; restore INFO and close viewers after calibration. The on-demand preview can be black simply because it arrived between flashes.

## Common symptoms

| Symptom | Check |
|---|---|
| Unknown on a fresh install | Enable the classification entity, then turn its switch on; wait at least 15 seconds. |
| Unknown with dark images | Alignment, LED visibility, power, threshold, and whether the controller is flashing at all. |
| Unknown with a continuously green image | Room light, reflections or exposure; solid light is deliberately not treated as idle. |
| Unknown with clear flashes | Compare the actual pattern to the supported timing scheme. Another valve family may need a decoder change. |
| Intermittent Unknown / frame gaps | Close live viewers, check stable USB power and Wi-Fi, and inspect analyzed frame rate. Gaps over 150 ms reset acquisition. |
| Unavailable in HA | ESP32 power, network reachability, integration address and API encryption key. |
| Fault | Read the physical control's LED and its own service manual; this sensor groups supported warning/fault codes and does not provide a repair diagnosis. |

The image parser requires exactly 160×120 JPEG input. Changing resolution or pixel format without updating and testing `led_camera.h` results in Unknown. Keep automatic gain and exposure disabled for stable flash detection.
