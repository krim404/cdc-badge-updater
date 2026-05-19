#pragma once

#include "cdc_core/IService.h"

namespace cdc::hal {

/**
 * ESP Hardware helper interface
 *
 * Provides access to ESP-specific hardware features.
 */
class IEspHardware : public core::IService {
public:
    virtual ~IEspHardware() = default;

    /**
     * Read internal temperature sensor in Celsius.
     * @param outC Output temperature
     * @return true on success
     */
    virtual bool getTemperatureC(float* outC) = 0;
};

// Factory function
IEspHardware* getEspHardwareInstance();

} // namespace cdc::hal
