#pragma once

#include <cstdint>

namespace cdc::core {

/**
 * Service lifecycle states
 */
enum class ServiceState : uint8_t {
    UNINITIALIZED,  // Not yet initialized
    INITIALIZED,    // init() called successfully
    STARTED,        // start() called successfully
    STOPPED,        // stop() called
    ERROR           // Error state
};

/**
 * Base interface for all services (HAL, Modules, etc.)
 *
 * Lifecycle:
 *   1. Constructor (lightweight, no hardware access)
 *   2. init() - One-time initialization
 *   3. start() - Begin operation
 *   4. stop() - Pause/cleanup
 */
class IService {
public:
    virtual ~IService() = default;

    /**
     * Initialize the service (called once during boot)
     * \return true on success
     */
    virtual bool init() = 0;

    /**
     * Start the service (can be called after init or stop)
     * \return true on success
     */
    virtual bool start() = 0;

    /**
     * Stop the service (reversible, can start again)
     */
    virtual void stop() = 0;

    /**
     * Get current service state
     */
    virtual ServiceState getState() const = 0;

    /**
     * Get service name (for logging/debugging)
     */
    virtual const char* getName() const = 0;
};

} // namespace cdc::core
