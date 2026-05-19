#pragma once

#include "cdc_core/IService.h"
#include "esp_err.h"
#include <cstdint>

namespace cdc::hal {

// Opaque device handle
using I2cDeviceHandle = void*;

/**
 * I2C Bus interface
 */
class II2cBus : public core::IService {
public:
    virtual ~II2cBus() = default;

    /**
     * Add a device to the bus
     * @param addr 7-bit I2C address
     * @param out_dev Output device handle
     */
    virtual esp_err_t addDevice(uint8_t addr, I2cDeviceHandle* out_dev) = 0;

    /**
     * Write to a device register
     */
    virtual esp_err_t writeReg(I2cDeviceHandle dev, uint8_t reg,
                               const uint8_t* data, size_t len) = 0;

    /**
     * Read from a device register
     */
    virtual esp_err_t readReg(I2cDeviceHandle dev, uint8_t reg,
                              uint8_t* data, size_t len) = 0;
};

// Factory functions for the two buses
II2cBus* getI2cBus0();  // BQ25895 + TCA9535
II2cBus* getI2cBus1();  // Expansion header

} // namespace cdc::hal
