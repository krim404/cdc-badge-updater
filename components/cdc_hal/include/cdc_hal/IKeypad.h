#pragma once

#include "cdc_core/IService.h"
#include <cstdint>

namespace cdc::hal {

/**
 * Key codes for the 12-button keypad
 * Physical layout:
 *   [1] [2] [3]
 *   [4] [5] [6]
 *   [7] [8] [9]
 *   [N] [0] [Y]   (N=Cancel, Y=OK)
 */
enum class Key : char {
    KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6',
    KEY_7 = '7', KEY_8 = '8', KEY_9 = '9',
    KEY_NO = 'N', KEY_0 = '0', KEY_YES = 'Y',
    KEY_NONE = 0
};

/**
 * Keypad event callback
 */
using KeyCallback = void(*)(Key key, bool pressed);

/**
 * Keypad interface for button input
 */
class IKeypad : public core::IService {
public:
    virtual ~IKeypad() = default;

    /**
     * Poll for key state changes
     * Should be called periodically from main loop
     */
    virtual void poll() = 0;

    /**
     * Check if a specific key is currently pressed
     */
    virtual bool isKeyPressed(Key key) const = 0;

    /**
     * Get next key from buffer (consumes the key)
     * @return Key code or KEY_NONE if buffer empty
     */
    virtual Key getNextKey() = 0;

    /**
     * Check if there are keys in the buffer
     */
    virtual bool hasKey() const = 0;

    /**
     * Check if any key is currently pressed
     */
    virtual bool anyKeyDown() const = 0;

    /**
     * Set callback for key events
     * @param callback Function to call on key press/release
     */
    virtual void setCallback(KeyCallback callback) = 0;

    /**
     * Enable/disable long-press detection
     * @param enabled Enable long-press
     * @param thresholdMs Time in ms to trigger long-press
     */
    virtual void setLongPressEnabled(bool enabled, uint32_t thresholdMs = 800) = 0;

    /**
     * Set callback for long-press events
     */
    using LongPressCallback = void(*)(Key key);
    virtual void setLongPressCallback(LongPressCallback callback) = 0;

    /**
     * Prepare keypad for sleep mode
     * Disables ISR and interrupt to prevent spurious wakeups
     */
    virtual void prepareForSleep() = 0;

    /**
     * Recover keypad after sleep wakeup
     * Waits for keys to be released, clears buffer, re-enables ISR
     */
    virtual void recoverFromSleep() = 0;

    /**
     * Clear key buffer (consume all pending keys)
     */
    virtual void clearBuffer() = 0;
};

// Factory function to get keypad instance
IKeypad* getKeypadInstance();

} // namespace cdc::hal
