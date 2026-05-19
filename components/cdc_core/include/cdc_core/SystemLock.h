#pragma once

#include <atomic>
#include <cstdint>

namespace cdc::core {

/**
 * \brief Reason the system entered lockdown.
 */
enum class LockdownReason : uint8_t {
    NONE = 0,
    TR01_ALARM_MODE,    /**< TROPIC01 reported ALARM bit (tamper/protocol violation). */
    TR01_UNREACHABLE,   /**< TROPIC01 not responding (MISO stuck at 0x00 / 0xFF). */
    TR01_INIT_FAILED,   /**< TROPIC01 could not be initialized at boot. */
    NVS_UNREADABLE,     /**< NVS partition cannot be read for an unexpected reason. */
};

/**
 * \brief Global lockdown latch.
 *
 * Any task may call \ref triggerLockdown to mark the system as compromised.
 * The lockdown is monotonic: once set it cannot be cleared except by a power
 * cycle / hard reset.
 *
 * The main loop calls \ref enforceIfLocked at the top of every iteration to
 * perform the shutdown sequence: optional UI handler (drawn from main context)
 * then \c esp_deep_sleep_start with no wake source enabled. Other tasks
 * check \ref isLocked to fail-fast their own operations.
 */
class SystemLock {
public:
    /** \brief UI handler invoked from main context just before deep sleep. */
    using ShutdownHandler = void (*)(LockdownReason reason, const char* detail);

    static SystemLock& instance();

    /** \brief Returns true once a lockdown has been latched. */
    bool isLocked() const { return locked_.load(std::memory_order_acquire); }

    /** \brief Reason captured at the first \ref triggerLockdown call. */
    LockdownReason getReason() const { return reason_.load(std::memory_order_acquire); }

    /**
     * \brief Returns the optional detail string captured at the first
     *        \ref triggerLockdown call, or \c nullptr if none was provided.
     *        Caller must ensure pointed-to memory has static lifetime.
     */
    const char* getDetail() const { return detail_.load(std::memory_order_acquire); }

    /**
     * \brief Latches the lockdown flag. Idempotent and ISR-safe.
     * \param reason Reason to record; ignored if already locked.
     * \param detail Optional pointer to a string literal with static lifetime
     *               describing the specific failure (for example
     *               `lt_ret_verbose(ret)` or `"PSA crypto init failed"`).
     */
    void triggerLockdown(LockdownReason reason, const char* detail = nullptr);

    /**
     * \brief Installs an optional UI handler invoked just before deep sleep.
     *        Must be set from main task before main loop starts polling.
     * \param handler Function pointer, or nullptr to clear.
     */
    void setShutdownHandler(ShutdownHandler handler);

    /**
     * \brief If locked, runs the shutdown sequence and never returns.
     *        Otherwise returns immediately. Call from main loop top.
     */
    void enforceIfLocked();

private:
    SystemLock() = default;
    [[noreturn]] void performShutdown();

    std::atomic<bool> locked_{false};
    std::atomic<LockdownReason> reason_{LockdownReason::NONE};
    std::atomic<const char*> detail_{nullptr};
    std::atomic<ShutdownHandler> handler_{nullptr};
};

} // namespace cdc::core
