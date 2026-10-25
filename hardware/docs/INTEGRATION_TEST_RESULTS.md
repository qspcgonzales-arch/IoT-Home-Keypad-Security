# Milestone 6 Integration Test Results

## Scope and limitations

These results distinguish static/build/emulator evidence from physical
verification. The project owner reports the Arduino components are connected,
but their assembly and behavior have not been independently observed. This
machine currently detects only built-in COM1, not an ESP32 USB serial port.
No physical UART test, ESP32 flash-ID query, firmware upload, Firebase
connection, live command, or real door actuation was performed. A successful
firmware build does not establish electrical or mechanical readiness.

## Milestone 6.5 — ESP32 UART preparation

| Check | Result | Evidence / limitation |
| --- | --- | --- |
| Uno firmware rebuild | **Compile pass; no upload** | PlatformIO `uno`; flash 21,224 / 32,256 bytes; RAM 948 / 2,048 bytes. |
| Default ESP32 rebuild | **Build pass; not flashable with default app slot** | Current `firmware.bin` is 1,323,120 bytes; default slot is 1,310,720 bytes (12,400 bytes over). |
| UART-only ESP32 candidate | **Build and conditional size pass** | `esp32dev_uart_only_4mb`, with `UART_ONLY=1` and `ota_nofs_4MB.csv`; image 1,323,120 / 2,031,616 bytes (708,496 bytes remaining). This verifies fit for the selected 4 MB profile only, not the unidentified physical ESP32. |
| PING/PONG serial diagnostics | **Source/build verified** | ESP32 monitor logs TX PING, matching RX PONG/door state, and transition to offline when no fresh PONG is received. |
| UART/software tests | **Pass** | `npm test` in `tools\device_simulator`: 40 passed, including pin/baud and serial-diagnostic source contracts. |
| Physical ESP32 identification | **Blocked** | `platformio device list` reports COM1 only (`ACPI\PNP0501`); no ESP32 port is present. Exact module, flash capacity, GPIO16/17 availability, and physical UART wiring remain unverified. |
| Firmware upload / UART acceptance / Firebase | **Not run** | No upload, hardware connection, Firebase connection, or live unlock command was performed. |

The UART-only profile skips the optional local `secrets.h`, leaves Wi-Fi/Firebase
configuration empty, and is the designated first ESP32 image for the UART
acceptance procedure in `MILESTONE_6_5_ACCEPTANCE.md`. The 4 MB OTA partition
is conditional; do not upload until the exact board and on-chip flash capacity
are verified with `flash-id`.

## Software and emulator results

| Check | Result | Evidence / limitation |
| --- | --- | --- |
| ESP32 firmware build | **Compile pass; default upload blocked** | PlatformIO Core 6.2.0 / Python 3.13.9, platform `espressif32@8.0.0`; no upload. Current default ELF report is 1,293,680 / 1,310,720 bytes (98.7%), RAM 49,232 / 327,680 bytes (15.0%). Current `firmware.bin` is 1,323,120 bytes, 12,400 bytes larger than the default app slot. |
| Uno firmware build | **Pass** | PlatformIO Core 6.2.0 / Python 3.13.9, run from `hardware\arduino\smart_home_lock`; platform `atmelavr@5.3.0`; no firmware upload. Flash 21,224 / 32,256 bytes (65.8%); static RAM 948 / 2,048 bytes (46.3%). ArduinoJson reports a deprecation warning for `StaticJsonDocument`; build succeeds. |
| Virtual hardware lifecycle | **Pass** | `npm test` in `tools\device_simulator`: 38 passed. Coverage includes UART state/ack ordering, PING/PONG, duplicate/expired commands, four-second logical relock, outage behavior, and audit synchronization. |
| Firebase proposed-rules suite | **Pass** | `npm test` in `tools\rules_tests`: 17 proposed-rules tests and 2 lifecycle tests passed in the RTDB emulator. Includes idempotent exact audit retry and rejection of changed-content overwrite. No production rules were changed or deployed. |
| Auth + RTDB device REST contract | **Pass** | Same `npm test` run: 1 Auth + RTDB emulator integration test passed, covering device sign-up/sign-in/refresh, registry read, bounded command query, command lifecycle, heartbeat/state/audit writes, exact audit retry, and revocation denial. Emulator only; it does not run the ESP32 C++ client. |
| Flutter tests/analyzer | **Pass** | `flutter test test\gateway_policy_test.dart` and `flutter analyze` passed; no live backend was used. |
| Flutter hot restart | **Not performed** | The Dart Tooling Daemon had no active Flutter app connection. No app was launched against the default Firebase configuration. |
| ESP32 UART/REST runtime | **Not run** | No board or isolated-network test device was available. |

## ESP32 flash and partition assessment

`esp32dev` in `platformio.ini` selects the PlatformIO board profile whose
configured flash capacity is 4 MB. That is a build assumption, not a verified
physical-board measurement. The project does not override
`board_build.partitions`; the clean build generated the framework's
`default.csv` layout:

| Partition | Offset | Size | Role |
| --- | --- | --- | --- |
| NVS | `0x9000` | 20 KiB | `Preferences`: active-command recovery and queued audit state |
| OTA data | `0xE000` | 8 KiB | OTA slot selection metadata |
| App 0 | `0x10000` | 1,310,720 bytes (1.25 MiB) | Current application slot |
| App 1 | `0x150000` | 1,310,720 bytes (1.25 MiB) | Second OTA-capable slot |
| SPIFFS | `0x290000` | 1,441,792 bytes (1,408 KiB) | Filesystem partition; firmware does not use it |
| Coredump | `0x3F0000` | 64 KiB | Crash-dump storage |

The current default build's `firmware.bin` is 1,323,120 bytes. That is 12,400
bytes larger than either default application slot. PlatformIO
reports the ELF section total as 1,293,392 bytes and labels it 98.7%, but this
does not account for the complete serialized image length; its successful
build result is **not** evidence that this binary fits. This is a flash-ready
blocker.

The size is due to more than gateway application code: the image links the
Wi-Fi and TLS/mbedTLS stack, HTTP client, Arduino core, and the built-in
certificate bundle. The linker map shows the embedded root certificate bundle
at 0x10D7B bytes (68,987 bytes). Do not remove TLS verification or the
certificate bundle as a quick size fix.

The installed ESP32 Arduino framework includes these 4 MB layout candidates:

| Partition CSV | App slots | OTA layout | Filesystem | Assessment |
| --- | --- | --- | --- | --- |
| `default.csv` | 2 x 1,310,720 bytes | Two slots | 1,408 KiB SPIFFS | Current selection; image does not fit |
| `min_spiffs.csv` | 2 x 1,966,080 bytes | Two slots | 128 KiB SPIFFS | Image fits if physical flash is 4 MB |
| `ota_nofs_4MB.csv` | 2 x 2,031,616 bytes | Two slots | None | Best conditional candidate if 4 MB flash is verified; current firmware has no filesystem use |
| `no_ota.csv` | 1 x 2,097,152 bytes | Single app, no OTA update slot | 1,966,080 bytes SPIFFS | Fits, but discards OTA slot capability |
| `huge_app.csv` | 1 x 3,145,728 bytes | Single app, no OTA update slot | 917,504 bytes SPIFFS | Fits, but discards OTA slot capability |

No physical layout has been verified because the ESP32 module is not
identified and its flash size has not been queried. The UART-only candidate
build uses `ota_nofs_4MB.csv`; its complete `.bin` fits by 708,496 bytes. If
and only if the physical module is verified compatible with the 4 MB profile,
this layout retains two OTA slots, NVS, OTA metadata, and coredump storage
while removing an unused filesystem. Rebuild after source/toolchain changes
and reconfirm the complete `.bin` is smaller than 2,031,616 bytes before
considering upload.
The project currently has no OTA update implementation, despite the default
partition table reserving two OTA slots. Preserve NVS; do not use full-chip
erase as a partition migration shortcut.

## Physical tests — all pending

| Test | Result |
| --- | --- |
| Arduino-only keypad/LCD/buzzer/TCRT checks | **Pending physical assembly** |
| ESP32 Wi-Fi connection and recovery | **Pending physical assembly** |
| Uno–ESP32 level-shifted UART at 9600 8N1 | **Pending physical assembly** |
| ESP32 sign-in/refresh against emulator from hardware | **Pending physical assembly** |
| Remote command reception and expiry on hardware | **Pending physical assembly** |
| Accepted/executed Arduino acknowledgement sequence | **Pending physical assembly** |
| Servo movement with no latch attached | **Pending physical assembly** |
| Four-second automatic relock timing | **Pending physical assembly** |
| Door-state and audit synchronization from physical events | **Pending physical assembly** |
| Wi-Fi loss/recovery without stale command replay | **Pending physical assembly** |
| Power-loss, brownout, stall current, and mechanical fail-secure behavior | **Pending mechanism and physical tests** |

Follow `HARDWARE_TEST_PLAN.md` in order and record measured voltages, timing,
serial traces, and observed results before changing any row from pending.
