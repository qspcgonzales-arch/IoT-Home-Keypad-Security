# Milestone 6.5 — ESP32/Uno UART Acceptance

> Historical Milestone 6.5 procedure for the separate Uno/UART architecture.
> Milestone 6.6 introduces a distinct ESP32-only path; use
> [ESP32_ONLY_ACCEPTANCE.md](./ESP32_ONLY_ACCEPTANCE.md) for that design. Do
> not combine the pin maps or firmware targets.

## Current status and blockers

- Source-level assignments agree: Uno `SoftwareSerial` RX=A2/TX=A3 and ESP32
  UART2 RX=GPIO16/TX=GPIO17, both at 9600 baud, 8N1.
- The local machine currently exposes only built-in COM1. No ESP32 USB serial
  port was found, so the physical module, flash capacity, board pinout, and
  physical UART link have **not** been verified.
- PlatformIO's `esp32dev` profile assumes a 4 MB ESP32. The default app slot is
  1,310,720 bytes, but the current `firmware.bin` is 1,323,120 bytes. It exceeds
  the slot by 12,400 bytes; **do not upload using `esp32dev`**.
- A separate `esp32dev_uart_only_4mb` build was measured at 1,323,120 bytes.
  It fits the installed `ota_nofs_4MB.csv` app slot of 2,031,616 bytes, with
  708,496 bytes remaining. This is conditional on identifying a compatible
  physical ESP32 with at least 4 MB flash and confirming the selected layout.
- GPIO16/17 are not available on some ESP32 module variants (including some
  PSRAM configurations). Confirm the exact module's pinout before wiring.
- No staging Firebase project or non-production administrator/device identities
  have been provided. No Firebase connection or live operation is authorized
  by this procedure.

## Firmware-grounded UART connection

| Signal | Uno | ESP32 | Requirement |
| --- | --- | --- | --- |
| ESP32 → Uno data | A2 RX | GPIO17 TX | Crossed TX-to-RX through a UART-compatible level translator |
| Uno → ESP32 data | A3 TX | GPIO16 RX | Crossed TX-to-RX; reduce Uno 5 V logic to ESP32-safe 3.3 V |
| Reference | GND | GND | Common ground |

Both directions are 9600 baud, 8 data bits, no parity, 1 stop bit. Use a
direction-fixed, push-pull UART level translator in both directions. Do not
connect Uno A3 directly to ESP32 GPIO16, and do not assume a passive I2C BSS138
shifter is suitable for UART. Confirm the translator's levels against both
board datasheets. Keep Uno D0/D1 free for its USB upload/debug connection.

The Uno sketch also assigns keypad rows/columns D2-D9, servo signal D10, IR
input D11, buzzer D12, and LCD I2C A4/A5. These are source assignments, not
proof of the assembled wiring.

## Read-only board identification

1. Disconnect power before changing wiring. Keep the servo's separate supply
   disconnected and remove the horn/linkage from any lock. Do not attach the
   hardware to an occupied door.
2. Identify the ESP32's exact board/module marking and confirm GPIO16/17 are
   available. Connect only the USB cable and run:

   ```powershell
   py -3.13 -m platformio device list
   ```

   Unplug/reconnect the intended board and confirm which USB serial port
   disappears and returns. Do not use COM1 unless that is demonstrably the
   ESP32 USB interface; the current COM1 is an ACPI built-in serial port.
3. Replace `COM7` below with the port identified by that unplug/replug check.
   `flash-id` reads the chip identifier/capacity; it does not erase or write:

   ```powershell
   $esp32Port = 'COM7'
   & "$env:USERPROFILE\.platformio\packages\tool-esptool\esptool.exe" --port $esp32Port flash-id
   ```

4. Record the board/module marking, flash manufacturer/ID, detected capacity,
   and available UART pins. Stop if the board is not compatible with the
   `esp32dev` profile or has less than 4 MB flash. Do not guess a partition or
   change flash size to bypass a fit error.

## Build and explicitly authorized upload

The UART-only environment defines `UART_ONLY=1`. `device_config.h` then skips
the optional local `secrets.h`, leaving Wi-Fi and Firebase values empty. This
profile must be used for the initial UART test; it must not contact Firebase.

After the physical board is identified as compatible with the 4 MB profile,
build and measure the target:

```powershell
py -3.13 -m platformio run -d hardware\esp32\smart_home_gateway -e esp32dev_uart_only_4mb
Get-Item hardware\esp32\smart_home_gateway\.pio\build\esp32dev_uart_only_4mb\firmware.bin |
  Select-Object FullName, Length
```

The matching `ota_nofs_4MB.csv` app slots are each 2,031,616 bytes. Require
the final `.bin` length to be no greater than that slot and retain the build
output with the board/flash-ID record. The current build measured 1,323,120
bytes, but rebuild after any source, library, or toolchain changes.

**No upload was performed.** Only after the board/partition fit, power/wiring
checks, and on-site supervision are confirmed may the owner explicitly run:

```powershell
py -3.13 -m platformio run -d hardware\esp32\smart_home_gateway -e esp32dev_uart_only_4mb -t upload --upload-port $esp32Port
```

The ESP32 Serial Monitor command is:

```powershell
py -3.13 -m platformio device monitor --port $esp32Port --baud 115200
```

Close the monitor before another process uses the port. Do not upload the
default `esp32dev` profile; its app slot is too small for the current image.
No Arduino upload is needed for this UART test; its existing source was built
with the Uno environment, without an upload.

## Sequential, unloaded UART acceptance checklist

1. **Pre-power inspection:** Have a teammate beside the bench and keep a
   manual power-disconnect immediately accessible. Servo positive supply
   remains disconnected and the horn/linkage is unloaded. Check polarity, no
   shorts, common ground, and both level-translator directions with a meter.
   Keep all lock/latch hardware disconnected.
2. **Power off both boards:** Wire Uno A3 TX → translator → ESP32 GPIO16 RX;
   ESP32 GPIO17 TX → translator → Uno A2 RX; connect ground-to-ground. Recheck
   that no Uno 5 V reaches an ESP32 input.
3. **UART-only firmware:** Confirm the 4 MB condition above, build the
   `esp32dev_uart_only_4mb` environment, and have the owner perform the
   explicit upload only if authorized. Do not provide Wi-Fi or Firebase
   credentials. Do not send an `UNLOCK` frame.
4. **Boot/monitor:** Start the Uno and ESP32, then open the ESP32 monitor at
   115200. The Uno emits its initial logical `STATE locked`; the ESP32 should
   log `UART TX PING p...` and a matching `UART RX PONG p... door=locked` about
   once per second, then `Arduino UART ONLINE (fresh PONG).`
5. **Sustained link:** Observe and record at least 60 consecutive matching
   PING/PONG IDs with no missing, malformed, or stale replies. Confirm the
   reported door value agrees with the Uno's logical state. This is a proposed
   bench acceptance threshold, not a result already achieved.
6. **Loss/recovery:** Power down before disconnecting the UART. With the Uno
   unavailable, PINGs should receive no PONG and the ESP32 should report
   `Arduino UART OFFLINE (PONG stale or missing)` after the 3-second freshness
   window. Power down before reconnecting, restore the link, and verify
   matching PONGs resume. Record elapsed times and any serial errors.
7. **Stop:** Power down both boards after collecting results. Keep the servo
   supply disconnected. Do not configure Wi-Fi, Firebase credentials, or
   cloud commands as part of this acceptance test.

Pass requires the observed serial evidence and recorded electrical checks;
successful compilation alone is not a physical pass. If any step fails, stop
and diagnose the UART, level conversion, pin availability, or power path
before proceeding.

## Gate before Firebase

Do not connect to Firebase until the exact ESP32 board and flash layout are
verified, the above UART acceptance evidence is recorded, and a separate
isolated staging project plus distinct administrator and device identities
are provisioned and reviewed. The current ESP32 REST client performs
synchronous HTTP requests with up to 5-second connect and 5-second request
timeouts in the main loop; network delays may interrupt the 1-second UART
poll cadence and should be assessed in a later heartbeat-only test before any
actuator command testing. No production rules/accounts are changed here.
