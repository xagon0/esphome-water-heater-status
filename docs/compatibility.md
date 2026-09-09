# Compatibility and validation

## Control pattern

The reference pattern is the WT8840 family described in [Resideo manual 34-00013EF, Table 4](https://customer.resideo.com/resources/Techlit/TechLitDocuments/34-00000s/34-00013EF.pdf). A single flash around every three seconds means idle; a rapid strobe around every three seconds means a call for heat. Abnormal codes use slower flashes and a pause between groups. The implemented abnormal counts are 2, 4, 5, 7 and 8; the last includes a controller warning. WT8860-only codes 6, 9 and 10 are not supported.

The decoder's timing tolerances are implementation choices derived from the manual and captured normal patterns; they are not manufacturer specifications:

| Detection | Implementation |
|---|---|
| Normal group period | 2.7–3.6 seconds |
| Rapid double pulse spacing | 50–600 ms |
| Normal confirmation | Four matching complete groups, roughly 10–12 seconds |
| Abnormal inner spacing | 750–1250 ms |
| Abnormal inter-group spacing | 2.5–4.5 seconds between rising edges |
| Fault confirmation | Two consecutive supported groups; supported counts may alternate |
| Lost analysis frames | Gap over 150 ms resets the decoders |
| Stale images | No new image for over 500 ms produces Unknown |

An established normal state is retained while another valid normal pattern is acquiring. A single missed half-strobe therefore does not immediately change Running to Not running. Irregular groups, sustained darkness, solid light and missing images clear confidence. Solid light remains Unknown even though a particular control may document it as OFF; the camera cannot distinguish that from a bad optical setup.

## Board and optics

Tested hardware: Freenove ESP32-WROVER-E camera kit, original ESP32, 4 MB flash, PSRAM, ESP-IDF. [Freenove documentation](https://docs.freenove.com/projects/fnk0060/en/latest/) describes the board family.

| Camera connection | GPIO |
|---|---|
| XCLK | 21 |
| SCCB SDA / SCL | 26 / 27 |
| Data D0–D7 | 4, 5, 18, 19, 36, 39, 34, 35 |
| VSYNC / HREF / PCLK | 25 / 23 / 22 |

The YAML uses JPEG 160×120, quality 10, two frame buffers, manual exposure/gain, a 30 fps target and no idle image requests. The processor uses Espressif's JPEG decoder to produce a 20×15 overview in a bounded buffer. Porting to another board requires verifying pinout, PSRAM and sustained image timing. AI-Thinker and ESP32-S3 boards have not been tested with this configuration.

## Evidence and limits

The initial installation was on a Bradford White RG275H6N with a Honeywell control suspected to be WT8840A1500 or similar. The exact fitted control number was not verified; the heater model alone does not establish compatibility.

- A person confirmed the main burner was off while single flashes were measured about 3.06 seconds apart.
- Captures later showed rapid pairs approximately 0.15–0.19 seconds apart, with a roughly three-second group period, and a transition back to single flashes. A separate physical burner-on confirmation was not obtained.
- A 90-second observation of the deployed detector measured about 27.3 fps and no analysis gaps over 150 ms.
- A later 60-second stable-state observation of the status-only deployment measured zero repeated state updates and zero images after connection warmup. Keepalives were not counted.
- Host tests exercise supported abnormal patterns without inducing physical heater faults. Physical fault recognition remains unvalidated.

The public template uses the deployed decoder code with generic names, no installation address, and optional diagnostics controlled by a substitution. Compilation and simulated timing tests verify software behavior; they do not establish compatibility with every control variant. Before relying on runtime history, verify both normal states on your own installation. Treat Unknown and unavailable as gaps in the record.
