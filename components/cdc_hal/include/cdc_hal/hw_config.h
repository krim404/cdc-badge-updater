#pragma once

// CDC Badge v1.0 Hardware Configuration
// All pin definitions in one place

#include "driver/gpio.h"

// === I2C Buses ===
// I2C0: Charging IC (BQ25895) + IO Expander (TCA9535)
#define I2C0_SDA_PIN GPIO_NUM_17
#define I2C0_SCL_PIN GPIO_NUM_18

// I2C1: Expansion header
#define I2C1_SDA_PIN GPIO_NUM_47
#define I2C1_SCL_PIN GPIO_NUM_48

// === IO Expander (TCA9535) ===
#define EXPANDER_ADDR 0x20
#define EXP_IRQ_PIN GPIO_NUM_1

// === Power / Charging (BQ25895) ===
#define BQ25895_ADDR 0x6A
#define CHG_DSEL_PIN GPIO_NUM_21
#define CHG_IRQ_PIN GPIO_NUM_39

// === Display (E-Paper GDEY029T94) ===
// Note: SPI pins configured via CalEPD Kconfig
#define EPD_LED_PIN GPIO_NUM_8      // Backlight/Frontlight
#define EPD_CS_PIN GPIO_NUM_41
#define EPD_DC_PIN GPIO_NUM_45
#define EPD_RST_PIN GPIO_NUM_46
#define EPD_BUSY_PIN GPIO_NUM_42

// === SPI Bus (shared: Display + TROPIC01) ===
#define SPI_SCLK_PIN GPIO_NUM_12
#define SPI_MISO_PIN GPIO_NUM_11
#define SPI_MOSI_PIN GPIO_NUM_13

// === TROPIC01 Secure Element ===
#define TR01_CS_PIN GPIO_NUM_10

// === Buttons ===
#define FLASH_BTN_PIN GPIO_NUM_0    // Flash/Power button

// === Expansion Ports ===
// SAO (Shitty Add-On) Port
#define SAO_GPIO1_PIN GPIO_NUM_15
#define SAO_GPIO2_PIN GPIO_NUM_16
#define SAO_EEPROM_ADDR 0x50

// Grove Port
#define GROVE_0_PIN GPIO_NUM_2
#define GROVE_1_PIN GPIO_NUM_3
