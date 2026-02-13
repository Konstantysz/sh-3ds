#pragma once

#include "Core/Types.h"

#include <memory>
#include <optional>
#include <string>

namespace SH3DS::Capture
{
    /**
     * @brief Abstract base class for frame acquisition from any source.
     */
    class FrameSource
    {
    public:
        virtual ~FrameSource() = default;

        /**
         * @brief Opens the frame source.
         * @return True if the frame source was opened successfully, false otherwise.
         */
        virtual bool Open() = 0;

        /**
         * @brief Closes the frame source.
         */
        virtual void Close() = 0;

        /**
         * @brief Grabs a frame from the frame source.
         * @return An optional containing the frame if successful, an empty optional otherwise.
         */
        virtual std::optional<Core::Frame> Grab() = 0;

        /**
         * @brief Checks if the frame source is open.
         * @return True if the frame source is open, false otherwise.
         */
        virtual bool IsOpen() const = 0;

        /**
         * @brief Describes the frame source.
         * @return A string describing the frame source.
         */
        virtual std::string Describe() const = 0;
    };
} // namespace SH3DS::Capture
