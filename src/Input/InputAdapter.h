#pragma once

#include "InputCommand.h"

#include <chrono>
#include <memory>
#include <string>

namespace SH3DS::Input
{
    /**
     * @brief Abstract base class for 3DS input injection.
     */
    class InputAdapter
    {
    public:
        virtual ~InputAdapter() = default;

        /**
         * @brief Connect to the input adapter.
         * @param address The address of the input adapter.
         * @param port The port of the input adapter.
         * @return True if the input adapter was connected successfully, false otherwise.
         */
        virtual bool Connect(const std::string &address, uint16_t port = 4950) = 0;

        /**
         * @brief Send a command to the input adapter.
         * @param cmd The command to send.
         * @return True if the command was sent successfully, false otherwise.
         */
        virtual bool Send(const Input::InputCommand &cmd) = 0;

        /**
         * @brief Release all buttons.
         * @return True if all buttons were released successfully, false otherwise.
         */
        virtual bool ReleaseAll() = 0;

        /**
         * @brief Press and release a button.
         * @param buttons The buttons to press and release.
         * @param holdDuration The duration to hold the buttons.
         * @param releaseDelay The delay between releasing the buttons and pressing them again.
         * @return True if the buttons were pressed and released successfully, false otherwise.
         */
        virtual bool PressAndRelease(uint32_t buttons,
            std::chrono::milliseconds holdDuration = std::chrono::milliseconds(100),
            std::chrono::milliseconds releaseDelay = std::chrono::milliseconds(50)) = 0;

        /**
         * @brief Check if the input adapter is connected.
         * @return True if the input adapter is connected, false otherwise.
         */
        virtual bool IsConnected() const = 0;

        /**
         * @brief Get a description of the input adapter.
         * @return A description of the input adapter.
         */
        virtual std::string Describe() const = 0;
    };
} // namespace SH3DS::Input
