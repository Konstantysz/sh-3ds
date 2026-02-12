#pragma once

#include <cstdint>

namespace SH3DS::Input
{
    /**
     * @brief 3DS button constants matching Luma3DS HID bitmask.
     */
    enum class Button : uint32_t
    {
        A = 0x0001,
        B = 0x0002,
        Select = 0x0004,
        Start = 0x0008,
        DRight = 0x0010,
        DLeft = 0x0020,
        DUp = 0x0040,
        DDown = 0x0080,
        R = 0x0100,
        L = 0x0200,
        X = 0x0400,
        Y = 0x0800,
    };

    /**
     * @brief 3DS interface buttons (HOME, POWER).
     */
    enum class InterfaceButton : uint32_t
    {
        Home = 0x01,
        Power = 0x02,
        PowerLong = 0x04,
    };

    /**
     * @brief Analog stick state (circle pad or c-stick).
     */
    struct AnalogStick
    {
        float x = 0.0f; ///< X-axis value (-1.0 to 1.0)
        float y = 0.0f; ///< Y-axis value (-1.0 to 1.0)
    };

    /**
     * @brief Touch screen state.
     */
    struct TouchPoint
    {
        uint16_t x = 0;        ///< X-axis value (0 to 240)
        uint16_t y = 0;        ///< Y-axis value (0 to 400)
        bool touching = false; ///< Whether the screen is being touched
    };

    /**
     * @brief Complete input state to send to the 3DS.
     */
    struct InputCommand
    {
        uint32_t buttonsPressed = 0;   ///< Bitmask of buttons to press
        AnalogStick circlePad;         ///< Circle pad state
        AnalogStick cStick;            ///< C-stick state
        TouchPoint touch;              ///< Touch screen state
        uint32_t interfaceButtons = 0; ///< Bitmask of interface buttons to press
    };
} // namespace SH3DS::Input
