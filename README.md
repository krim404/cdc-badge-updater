# CDC Badge Updater

One-shot firmware for the CDC Badge v1.0 hardware (ESP32-S3 + TROPIC01).
Two use cases, both end with re-flashing the normal
[`cdc-badge-os`](https://github.com/krim404/cdc-badge-os):

1. **TROPIC01 firmware update**: press **Y** at the confirmation prompt.
   The badge wipes user data, writes the embedded RISC-V and SPECT
   firmwares, verifies the new versions, then halts.
2. **Factory reset only**: press **N** at the confirmation prompt.
   The badge has already wiped NVS, all populated TROPIC01 ECC slots and
   R-Memory slots before reaching the prompt. Choosing N skips the
   firmware write and just halts, leaving the chip cleaned out. Re-flash
   `cdc-badge-os` afterwards and the device boots from a clean slate.

In both cases R-Config and the production pairing keys remain
untouched - the chip stays addressable, no out-of-band tooling required.

> WARNING: The TROPIC01 firmware update (option 1) is NOT interruptible.
> Power loss during the critical phase can brick the secure element.
> Keep the badge on USB power, do not press RESET, do not disconnect.
> Option 2 (factory reset only) carries no such risk.

## Embedded TROPIC01 firmware

| Component         | Version |
|-------------------|---------|
| Bootloader        | 2.0.1   |
| RISC-V CPU FW     | 2.0.0   |
| SPECT FW          | 1.0.0   |

Source: `third_party/libtropic` submodule, directory
`TROPIC01_fw_update_files/boot_v_2_0_1/fw_v_2_0_0/`.

## Web Flasher

Two mirrors, identical content:

- GitHub Pages: <https://krim404.github.io/cdc-badge-updater/>
- Codeberg Pages: <https://krim.codeberg.page/cdc-badge-updater/>

1. Connect the CDC Badge via USB.
2. Open one of the URLs in Chrome or Edge.
3. Press and hold FLASH, tap RESET, release FLASH (bootloader mode).
4. Click "Connect (Flash/Serial)" and select the badge.
5. After flashing, press RESET to start the updater.

## Boot flow on the device

1. STARTING - e-paper splash.
2. Power check (battery >= 3.6 V or USB present). Refuses to continue on low power.
3. CLEANING - unconditional wipe of NVS and every populated TROPIC01 ECC /
   R-Memory slot. R-Config and pairing keys remain untouched.
4. Reads current TROPIC01 firmware version.
5. Shows current vs. embedded firmware version, waits for **Y** (flash new
   firmware) or **N** (skip firmware write, factory reset only).
6. On Y: UPDATING - rewrites TROPIC01 RISC-V and SPECT firmware. DO NOT
   POWER OFF. On N: ABORTED - skip firmware write.
7. SUCCESS / FAILED / ABORTED screen, then halt with USB serial kept alive
   for log capture. Re-flash `cdc-badge-os` to resume normal use.

> The wipe in step 3 happens before the Y/N prompt, so pressing N still
> leaves the badge with NVS erased and TROPIC01 user data cleared. That
> is the supported "factory reset only" path.

## Logging

ESP32-S3 built-in USB-Serial-JTAG at 115200 8N1. View with:

```bash
~/.platformio/penv/bin/pio device monitor
```

There is no UART0 / external bridge - the USB cable is the only serial
transport. The firmware does not initialize TinyUSB.

## Build

```bash
git submodule update --init --recursive
~/.platformio/penv/bin/pio run
```

Output binaries: `.pio/build/cdc_badge_updater/{bootloader,partitions,firmware}.bin`.

## Flash

```bash
~/.platformio/penv/bin/pio run -t upload
# After upload completes, press RESET on the badge to start the updater.
```

## Mirrors

- GitHub:   <https://github.com/krim404/cdc-badge-updater>
- Codeberg: <https://codeberg.org/Krim/cdc-badge-updater>

## Safety notes

- The TROPIC01 firmware update is not interruptible. Keep the badge powered
  during the UPDATING screen.
- The ESP32-S3 brownout detector is enabled at level 7 (~3.0 V) to avoid
  half-written flash pages on a power dip.
- Only ECC slots (0..31) and R-Memory slots (0..511) are wiped. R-Config
  and the production pairing keys are untouched.

## Version

`v0.2.0` current release. `v0.1.0` was the initial release.

## License

See [`LICENSE.md`](LICENSE.md). Components and submodules retain their
upstream licenses (libtropic, CalEPD, Adafruit-GFX, esp-web-tools).
