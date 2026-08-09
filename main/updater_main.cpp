/**
 * \file
 * \brief CDC Badge Updater main application.
 *
 * Single-purpose firmware that conditionally wipes NVS + TROPIC01 user data
 * (ECC slots, R-Memory slots; never R-Config / pairing keys), then asks the
 * user for confirmation on the e-paper, then performs the TROPIC01 firmware
 * update with the embedded blobs.
 *
 * Boot phases:
 *   PHASE 0 - System init (NVS handle, I2C, Power, Display, Keypad)
 *   PHASE 1 - Power validation (battery >= 3.6V or USB present)
 *   PHASE 2 - TROPIC01 init + secure session
 *   PHASE 3 - Conditional wipe (NVS + ECC + R-Memory, only what is populated)
 *   PHASE 4 - Read current firmware versions
 *   PHASE 5 - User confirmation via Y/N keypad
 *   PHASE 6 - TROPIC01 firmware update (CRITICAL, brick-on-power-loss)
 *   PHASE 7 - Verify versions, display result, halt
 *
 * Output:
 *   - E-paper: 1 of 8 static screens (STARTING / LOW POWER / CLEANING /
 *     CONFIRM / UPDATING / SUCCESS / FAILED / ABORTED)
 *   - UART0  : verbose ESP_LOG of every step
 */

#include "cdc_log.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_hal/IKeypad.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/ISecureElement.h"
#include "cdc_hal/tropic01_fw_update.h"
#include "cdc_core/FactoryReset.h"
#include "tropic_fw_blob.h"

#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstdio>
#include <cstring>

#include <goodisplay/gdey029T94.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

static constexpr const char *TAG = "Updater";

static constexpr uint16_t MIN_BATTERY_MV = 3600;

#ifndef UPDATER_DRY_RUN
#define UPDATER_DRY_RUN 0
#endif

#if UPDATER_DRY_RUN
static constexpr const char *MODE_BANNER = "DRY-RUN (no flash)";
#else
static constexpr const char *MODE_BANNER = "LIVE FLASH MODE";
#endif

static constexpr uint16_t EPD_BLACK_COLOR = 0x0000;
static constexpr uint16_t EPD_WHITE_COLOR = 0xFFFF;

/** \brief Buffer size for a "MAJ.MIN.PAT" firmware version string. */
static constexpr size_t FW_VERSION_STR_LEN = 12;

using cdc::hal::IDisplay;
using cdc::hal::IKeypad;
using cdc::hal::IPowerManager;
using cdc::hal::ISecureElement;
using cdc::hal::Key;
using cdc::hal::RefreshMode;
using cdc::hal::SeResult;

/**
 * \brief Renders one screen: clears, writes title + multi-line body, flushes.
 */
static void drawScreen(IDisplay *display, const char *title, const char *body) {
    display->clear();
    display->setTextColor(EPD_BLACK_COLOR);
    display->setFont(&FreeMonoBold12pt7b);
    display->setCursor(4, 22);
    display->print(title);

    if (body) {
        display->setFont(&FreeMonoBold9pt7b);
        int16_t y = 50;
        const char *line = body;
        char buf[64];
        while (*line) {
            const char *eol = std::strchr(line, '\n');
            size_t len = eol ? static_cast<size_t>(eol - line) : std::strlen(line);
            if (len >= sizeof(buf)) len = sizeof(buf) - 1;
            std::memcpy(buf, line, len);
            buf[len] = '\0';
            display->setCursor(4, y);
            display->print(buf);
            y += 18;
            if (!eol) break;
            line = eol + 1;
        }
    }
    display->flushSync(RefreshMode::FULL);
}

[[noreturn]] static void deepSleepForever() {
    // We deliberately do NOT call esp_deep_sleep_start() because it powers down
    // the ESP32-S3 USB-Serial-JTAG endpoint, which makes post-mortem log
    // retrieval impossible. The board sits idle in a low-priority loop instead
    // and the USB serial stays connected so the user can pull the boot log at
    // any time. Reflash (FLASH + RESET buttons) to leave this state.
    LOG_W(TAG, "Halted. Reset+reflash to continue. USB serial stays up for log capture.");
    int sec = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        sec++;
        if (sec % 30 == 0) {
            LOG_I(TAG, "Still halted (%d s)...", sec);
        }
    }
}

[[noreturn]] static void fail(IDisplay *display, const char *reason) {
    LOG_E(TAG, "FATAL: %s", reason);
    if (display) {
        drawScreen(display, "FAILED", reason);
    }
    deepSleepForever();
}

/**
 * \brief Formats a TROPIC01 version array as "MAJ.MIN.PAT".
 *
 * \param buf Destination buffer, at least FW_VERSION_STR_LEN bytes.
 * \param len Size of buf.
 * \param ver Version array laid out as {reserved, patch, minor, major}.
 */
static void fmtFwVersion(char *buf, size_t len, const uint8_t ver[4]) {
    std::snprintf(buf, len, "%u.%u.%u", ver[3], ver[2], ver[1]);
}

/**
 * \brief Late-binds the e-paper display so an early TROPIC01 failure can still
 *        be rendered. Returns nullptr if the display itself refuses to come up.
 *
 * In normal operation Display::init() runs as part of Phase 0 *after* the
 * secure element has finished its handshake, because doing it the other way
 * around leaves the shared SPI bus in a state that makes MISO from the
 * TROPIC01 read as 0xFF (LT_L1_CHIP_ALARM_MODE). This helper is only used to
 * recover from a pre-display failure.
 */
static IDisplay *initDisplayForError() {
    auto *display = cdc::hal::getDisplayInstance();
    if (display && display->init() && display->start()) {
        return display;
    }
    return nullptr;
}

/**
 * \brief Polls the keypad until Y or N is pressed.
 * \return Key::KEY_YES or Key::KEY_NO.
 */
static Key waitForYesNo(IKeypad *kp) {
    kp->clearBuffer();
    for (;;) {
        kp->poll();
        Key k;
        while ((k = kp->getNextKey()) != Key::KEY_NONE) {
            if (k == Key::KEY_YES || k == Key::KEY_NO) {
                return k;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * \brief Returns true if the chip is on USB power or the battery is healthy.
 */
static bool powerOk(IPowerManager *pm, uint16_t *out_mv, bool *out_usb) {
    pm->refresh();
    uint16_t mv = pm->getBatteryVoltage();
    bool usb = pm->isUsbConnected();
    if (out_mv)  *out_mv  = mv;
    if (out_usb) *out_usb = usb;
    return usb || mv >= MIN_BATTERY_MV;
}

extern "C" void app_main() {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    // Mute the ESP-IDF SPI / boot tags that flood the log on every transfer.
    // Keep our own modules ("Updater", "TR01", "EpaperDisplay", ...) verbose
    // so each phase is fully traceable on the serial console.
    esp_log_level_set("bus_lock",       ESP_LOG_WARN);
    esp_log_level_set("spi_master",     ESP_LOG_WARN);
    esp_log_level_set("spi",            ESP_LOG_WARN);
    esp_log_level_set("spi_hal",        ESP_LOG_WARN);
    esp_log_level_set("intr_alloc",     ESP_LOG_WARN);
    esp_log_level_set("memory_layout",  ESP_LOG_WARN);
    esp_log_level_set("cpu_start",      ESP_LOG_WARN);
    esp_log_level_set("memspi",         ESP_LOG_WARN);
    esp_log_level_set("heap_init",      ESP_LOG_WARN);
    esp_log_level_set("partition",      ESP_LOG_WARN);
    esp_log_level_set("mmap",           ESP_LOG_WARN);
    esp_log_level_set("efuse_init",     ESP_LOG_WARN);
    esp_log_level_set("clk",            ESP_LOG_WARN);
    esp_log_level_set("gdma",           ESP_LOG_WARN);
    esp_log_level_set("gpio",           ESP_LOG_WARN);
    esp_log_level_set("ledc",           ESP_LOG_WARN);
    esp_log_level_set("nvs",            ESP_LOG_INFO);
    esp_log_level_set("BQ25895",        ESP_LOG_INFO);
    esp_log_level_set("EpaperDisplay",  ESP_LOG_INFO);
    esp_log_level_set("EpdSpi",         ESP_LOG_INFO);

    char emb_riscv[FW_VERSION_STR_LEN];
    char emb_spect[FW_VERSION_STR_LEN];
    fmtFwVersion(emb_riscv, sizeof(emb_riscv), fw_CPU_ver);
    fmtFwVersion(emb_spect, sizeof(emb_spect), fw_SPECT_ver);

    LOG_I(TAG, "============================================");
    LOG_I(TAG, "== CDC Badge Updater v%s", APP_VERSION);
    LOG_I(TAG, "== Mode: %s", MODE_BANNER);
    LOG_I(TAG, "== Embedded TROPIC01 FW:");
    LOG_I(TAG, "==   boot %s, RISC-V %s, SPECT %s",
          TROPIC_FW_BOOT_VERSION, emb_riscv, emb_spect);
    LOG_I(TAG, "============================================");

    // ----- PHASE 0a: NVS, I2C, Power (no UI yet) -----
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        LOG_W(TAG, "NVS init returned %s, erasing and retrying", esp_err_to_name(nvs_err));
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
    }
    if (nvs_err != ESP_OK) {
        LOG_E(TAG, "nvs_flash_init failed: %s", esp_err_to_name(nvs_err));
    }

    auto *i2c0 = cdc::hal::getI2cBus0();
    if (!i2c0 || !i2c0->init() || !i2c0->start()) {
        LOG_E(TAG, "I2C bus0 init failed - cannot continue without power/keypad I2C");
        deepSleepForever();
    }

    auto *pm = cdc::hal::getPowerManagerInstance();
    if (!pm || !pm->init() || !pm->start()) {
        LOG_E(TAG, "PowerManager init failed");
        deepSleepForever();
    }

    // Power check BEFORE TROPIC01 init. If power fails we late-bind the
    // display just for the error screen.
    uint16_t mv = 0;
    bool usb = false;
    if (!powerOk(pm, &mv, &usb)) {
        char detail[64];
        std::snprintf(detail, sizeof(detail),
                      "Battery %u mV, USB=%d\nConnect USB to start", mv, usb ? 1 : 0);
        LOG_E(TAG, "Power check failed: %s", detail);
        if (auto *d = initDisplayForError()) {
            drawScreen(d, "LOW POWER", detail);
        }
        deepSleepForever();
    }
    LOG_I(TAG, "PHASE 0a power OK: %u mV, USB=%d", mv, usb ? 1 : 0);

    // ----- PHASE 0b: TROPIC01 init + session BEFORE display -----
    // The shared SPI2 bus is initialized by whoever calls
    // spi_bus_initialize() first. If the display init runs first the bus is
    // brought up with the display's matrix routing and the TROPIC01 reads
    // MISO=0xFF (interpreted by libtropic as LT_L1_CHIP_ALARM_MODE).
    // Initializing the secure element first replicates the order used by
    // the production cdc-badge-os firmware and is the working sequence.
    //
    // Cross-device SPI safety: ESP-IDF's spi_master driver lock (bus_lock)
    // automatically serializes per-transfer access between display and
    // TROPIC01. Tropic01Element additionally calls spi_device_acquire_bus()
    // around multi-frame libtropic operations so they are atomic against
    // any later display refresh.
    auto *se = cdc::hal::getSecureElementInstance();
    if (!se || !se->init()) {
        LOG_E(TAG, "TROPIC01 init failed");
        if (auto *d = initDisplayForError()) {
            drawScreen(d, "FAILED", "TROPIC01 init failed\nCheck SPI / power");
        }
        deepSleepForever();
    }
    if (!se->start()) {
        if (auto *d = initDisplayForError()) drawScreen(d, "FAILED", "TROPIC01 start failed");
        deepSleepForever();
    }
    if (!se->sessionStart()) {
        if (auto *d = initDisplayForError()) {
            drawScreen(d, "FAILED", "Session failed\nPairing key mismatch?");
        }
        deepSleepForever();
    }
    LOG_I(TAG, "PHASE 0b secure-element session active");

    // ----- PHASE 0c: Display + Keypad (now safe; SPI bus already up) -----
    auto *display = cdc::hal::getDisplayInstance();
    if (!display || !display->init() || !display->start()) {
        LOG_E(TAG, "Display init failed - no UI surface, halting");
        deepSleepForever();
    }

    auto *keypad = cdc::hal::getKeypadInstance();
    if (!keypad || !keypad->init() || !keypad->start()) {
        fail(display, "Keypad init failed");
    }

    drawScreen(display, "STARTING", nullptr);
    LOG_I(TAG, "PHASE 0 complete (TR01 + display + keypad up)");

    // ----- PHASE 3: Conditional wipe -----
#if UPDATER_DRY_RUN
    drawScreen(display, "CLEANING", "DRY-RUN: inspect\nonly, no erase");
    {
        nvs_stats_t stats{};
        if (nvs_get_stats(nullptr, &stats) == ESP_OK) {
            LOG_I(TAG, "DRY-RUN NVS stats: used=%u, free=%u, total=%u",
                  static_cast<unsigned>(stats.used_entries),
                  static_cast<unsigned>(stats.free_entries),
                  static_cast<unsigned>(stats.total_entries));
            LOG_I(TAG, "DRY-RUN: would %s NVS",
                  stats.used_entries > 0 ? "erase" : "skip (empty)");
        } else {
            LOG_W(TAG, "DRY-RUN nvs_get_stats failed");
        }

        uint16_t eccUsed = 0;
        for (uint8_t s = 0; s < cdc::hal::ISecureElement::ECC_SLOT_COUNT; ++s) {
            if (se->eccSlotUsed(s)) eccUsed++;
        }
        uint16_t rmemUsed = 0;
        for (uint16_t s = 0; s < cdc::hal::ISecureElement::RMEM_SLOT_COUNT; ++s) {
            if (se->rmemSlotUsed(s)) rmemUsed++;
        }
        LOG_I(TAG, "DRY-RUN TROPIC01: %u ECC slots populated, %u R-Mem slots populated",
              eccUsed, rmemUsed);
        LOG_I(TAG, "DRY-RUN: would erase %u ECC + %u R-Mem slots (R-Config untouched)",
              eccUsed, rmemUsed);
    }
    LOG_I(TAG, "PHASE 3 dry-run inspection complete (nothing erased)");
#else
    drawScreen(display, "CLEANING", "Erasing NVS\nand secure data");

    nvs_stats_t stats{};
    if (nvs_get_stats(nullptr, &stats) == ESP_OK) {
        LOG_I(TAG, "NVS stats: used=%u, free=%u, total=%u",
              static_cast<unsigned>(stats.used_entries),
              static_cast<unsigned>(stats.free_entries),
              static_cast<unsigned>(stats.total_entries));
        if (stats.used_entries > 0) {
            LOG_I(TAG, "NVS contains data - erasing");
            esp_err_t err = cdc::core::wipeNvs();
            if (err != ESP_OK) {
                LOG_W(TAG, "wipeNvs returned %s (continuing)", esp_err_to_name(err));
            }
        } else {
            LOG_I(TAG, "NVS already empty - skipping erase");
        }
    } else {
        LOG_W(TAG, "nvs_get_stats failed - skipping NVS wipe to avoid unnecessary erase");
    }

    cdc::core::TropicWipeResult wipe = cdc::core::wipeTropic(se);
    LOG_I(TAG, "TROPIC01 wipe: session=%d, ECC=%u, R-Mem=%u",
          wipe.sessionReady, wipe.eccDeleted, wipe.rmemDeleted);
    if (!wipe.sessionReady) {
        fail(display, "Wipe aborted\nNo SE session");
    }
    LOG_I(TAG, "PHASE 3 wipe complete (R-Config / pairing keys untouched)");
#endif

    // ----- PHASE 4: Read current versions -----
    uint8_t cur_riscv[4] = {0};
    uint8_t cur_spect[4] = {0};
    if (!se->getFwVersion(cur_riscv, cur_spect)) {
        LOG_W(TAG, "getFwVersion failed - showing zeros");
    }
    LOG_I(TAG, "Current  RISC-V %u.%u.%u.%u  SPECT %u.%u.%u.%u",
          cur_riscv[3], cur_riscv[2], cur_riscv[1], cur_riscv[0],
          cur_spect[3], cur_spect[2], cur_spect[1], cur_spect[0]);
    LOG_I(TAG, "Embedded RISC-V %s  SPECT %s", emb_riscv, emb_spect);

    // ----- PHASE 5: User confirmation -----
    char body[160];
#if UPDATER_DRY_RUN
    std::snprintf(body, sizeof(body),
                  "DRY-RUN MODE\nCur %u.%u.%u/%u.%u.%u\nNew %s/%s\n[Y] Simulate [N] Skip",
                  cur_riscv[3], cur_riscv[2], cur_riscv[1],
                  cur_spect[3], cur_spect[2], cur_spect[1],
                  emb_riscv, emb_spect);
    drawScreen(display, "DRY-RUN?", body);
#else
    std::snprintf(body, sizeof(body),
                  "Cur %u.%u.%u/%u.%u.%u\nNew %s/%s\n[Y] Update  [N] Skip",
                  cur_riscv[3], cur_riscv[2], cur_riscv[1],
                  cur_spect[3], cur_spect[2], cur_spect[1],
                  emb_riscv, emb_spect);
    drawScreen(display, "FW UPDATE?", body);
#endif
    LOG_I(TAG, "PHASE 5 awaiting Y/N");

    Key choice = waitForYesNo(keypad);
    if (choice == Key::KEY_NO) {
        LOG_I(TAG, "User chose N - aborting before FW update");
        drawScreen(display, "ABORTED", "Safe to reflash\nthe normal FW");
        se->sessionEnd();
        deepSleepForever();
    }

    // ----- PHASE 6: FW update (CRITICAL in live mode) -----
    LOG_W(TAG, "PHASE 6 USER CONFIRMED - critical phase begins");
    if (!powerOk(pm, &mv, &usb)) {
        char detail[64];
        std::snprintf(detail, sizeof(detail),
                      "Power dropped:\n%u mV, USB=%d\nABORT before flash",
                      mv, usb ? 1 : 0);
        fail(display, detail);
    }

#if UPDATER_DRY_RUN
    drawScreen(display, "DRY-RUN", "Simulating flash\nDO NOT POWER OFF");
    LOG_W(TAG, "==== DRY-RUN: skipping real FW update ====");
    LOG_I(TAG, "Would call tropic01_perform_fw_update("
              "fw_cpu=%u bytes, fw_spect=%u bytes)",
          static_cast<unsigned>(tropic_fw_cpu_size),
          static_cast<unsigned>(tropic_fw_spect_size));
    LOG_I(TAG, "Sequence would be:");
    LOG_I(TAG, "  1. open secure session (pairing key)");
    LOG_I(TAG, "  2. read I-Config (maintenance permitted?)");
    LOG_I(TAG, "  3. enable Maintenance Mode in R-Config if needed");
    LOG_I(TAG, "  4. reboot to MAINTENANCE, verify bootloader %s", TROPIC_FW_BOOT_VERSION);
    LOG_I(TAG, "  5. lt_do_mutable_fw_update(fw_CPU %u B, fw_SPECT %u B, both bank pairs)",
          static_cast<unsigned>(tropic_fw_cpu_size),
          static_cast<unsigned>(tropic_fw_spect_size));
    LOG_I(TAG, "  6. disable Maintenance Mode in R-Config + verify unreachable");
    vTaskDelay(pdMS_TO_TICKS(3000));
    LOG_W(TAG, "==== DRY-RUN: simulated FW update complete ====");
#else
    drawScreen(display, "UPDATING...", "DO NOT POWER OFF\nDO NOT RESET");
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_task_wdt_deinit();
    LOG_I(TAG, "Watchdog disabled for FW-update window");

    // Expected bootloader version of the embedded blobs (safety gate inside the update).
    unsigned eb[3] = {0, 0, 0};
    sscanf(TROPIC_FW_BOOT_VERSION, "%u.%u.%u", &eb[0], &eb[1], &eb[2]);

    int lt_ret = 0;
    tropic01_fw_update_result_t fw_res = tropic01_perform_fw_update(
        fw_CPU,   static_cast<uint16_t>(tropic_fw_cpu_size),
        fw_SPECT, static_cast<uint16_t>(tropic_fw_spect_size),
        static_cast<uint8_t>(eb[0]), static_cast<uint8_t>(eb[1]), static_cast<uint8_t>(eb[2]),
        &lt_ret);

    if (fw_res != TROPIC01_FW_UPDATE_OK) {
        char detail[96];
        const char *stage = "?";
        switch (fw_res) {
            case TROPIC01_FW_UPDATE_BUS_FAILED:                 stage = "SPI bus";            break;
            case TROPIC01_FW_UPDATE_SESSION_FAILED:             stage = "secure session";     break;
            case TROPIC01_FW_UPDATE_MAINTENANCE_NOT_ALLOWED:    stage = "maint not allowed";  break;
            case TROPIC01_FW_UPDATE_RCONFIG_FAILED:             stage = "R-Config";           break;
            case TROPIC01_FW_UPDATE_REBOOT_MAINTENANCE_FAILED:  stage = "maint reboot";       break;
            case TROPIC01_FW_UPDATE_BOOTLOADER_MISMATCH:        stage = "bootloader mismatch"; break;
            case TROPIC01_FW_UPDATE_FAILED:                     stage = "FW write";           break;
            case TROPIC01_FW_UPDATE_MAINTENANCE_DISABLE_FAILED: stage = "maint disable";      break;
            case TROPIC01_FW_UPDATE_REBOOT_APP_FAILED:          stage = "app reboot";         break;
            case TROPIC01_FW_UPDATE_PARAM_ERR:                  stage = "param";              break;
            default:                                            stage = "unknown";            break;
        }
        std::snprintf(detail, sizeof(detail), "%s\nlt_ret=%d", stage, lt_ret);
        LOG_E(TAG, "FW-Update failed at %s (lt_ret=%d)", stage, lt_ret);
        fail(display, detail);
    }
    LOG_I(TAG, "PHASE 6 FW-update OK");
#endif

    // ----- PHASE 7: Verify versions -----
#if UPDATER_DRY_RUN
    uint8_t new_riscv[4] = {0};
    uint8_t new_spect[4] = {0};
    if (!se->getFwVersion(new_riscv, new_spect)) {
        LOG_W(TAG, "DRY-RUN post-readback getFwVersion failed");
    }
    LOG_I(TAG, "DRY-RUN readback RISC-V %u.%u.%u.%u  SPECT %u.%u.%u.%u",
          new_riscv[3], new_riscv[2], new_riscv[1], new_riscv[0],
          new_spect[3], new_spect[2], new_spect[1], new_spect[0]);
    LOG_I(TAG, "DRY-RUN: versions unchanged (expected, no real flash)");

    char ok_body[112];
    std::snprintf(ok_body, sizeof(ok_body),
                  "All phases OK\nRISC-V %u.%u.%u\nSPECT %u.%u.%u\nNo flash performed",
                  new_riscv[3], new_riscv[2], new_riscv[1],
                  new_spect[3], new_spect[2], new_spect[1]);
    drawScreen(display, "DRY-RUN OK", ok_body);
    LOG_I(TAG, "PHASE 7 DRY-RUN complete - halting");
    se->sessionEnd();
    deepSleepForever();
#else
    if (!se->sessionStart()) {
        LOG_W(TAG, "Post-update session re-establishment failed - reporting SUCCESS based on lt return");
        char ok_body[64];
        std::snprintf(ok_body, sizeof(ok_body),
                      "RISC-V %s\nSPECT %s\nReflash normal FW",
                      emb_riscv, emb_spect);
        drawScreen(display, "SUCCESS", ok_body);
        deepSleepForever();
    }

    uint8_t new_riscv[4] = {0};
    uint8_t new_spect[4] = {0};
    se->getFwVersion(new_riscv, new_spect);
    LOG_I(TAG, "New RISC-V %u.%u.%u.%u  SPECT %u.%u.%u.%u",
          new_riscv[3], new_riscv[2], new_riscv[1], new_riscv[0],
          new_spect[3], new_spect[2], new_spect[1], new_spect[0]);

    // Cross-check against the embedded blobs' version arrays. Any mismatch is
    // reported as FAILED so the user can re-run the updater instead of trusting
    // a silent SUCCESS.
    bool match = (new_riscv[3] == fw_CPU_ver[3] && new_riscv[2] == fw_CPU_ver[2]
               && new_riscv[1] == fw_CPU_ver[1]
               && new_spect[3] == fw_SPECT_ver[3] && new_spect[2] == fw_SPECT_ver[2]
               && new_spect[1] == fw_SPECT_ver[1]);

    char ok_body[112];
    if (match) {
        std::snprintf(ok_body, sizeof(ok_body),
                      "RISC-V %u.%u.%u\nSPECT %u.%u.%u\nReflash normal FW",
                      new_riscv[3], new_riscv[2], new_riscv[1],
                      new_spect[3], new_spect[2], new_spect[1]);
        drawScreen(display, "SUCCESS", ok_body);
        LOG_I(TAG, "PHASE 7 SUCCESS - versions match embedded");
    } else {
        std::snprintf(ok_body, sizeof(ok_body),
                      "Mismatch:\nGot RISC-V %u.%u.%u\nGot SPECT %u.%u.%u",
                      new_riscv[3], new_riscv[2], new_riscv[1],
                      new_spect[3], new_spect[2], new_spect[1]);
        LOG_E(TAG, "PHASE 7 version mismatch (expected %s / %s)", emb_riscv, emb_spect);
        drawScreen(display, "FAILED", ok_body);
    }

    se->sessionEnd();
    deepSleepForever();
#endif
}
