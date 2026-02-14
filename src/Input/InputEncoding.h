#pragma once

#include "InputCommand.h"

#include <cstdint>

namespace SH3DS::Input
{
    constexpr uint32_t kLuma3DSPacketSize = 20; ///< Size of a Luma3DS input packet

    /**
     * @brief Write a 32-bit value to a byte array in little-endian format.
     * @param dst The byte array to write to.
     * @param val The value to write.
     */
    inline void WriteLe32(uint8_t *dst, uint32_t val)
    {
        dst[0] = static_cast<uint8_t>(val & 0xFF);
        dst[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        dst[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    }

    /**
     * @brief Read a 32-bit value from a byte array in little-endian format.
     * @param buf The byte array to read from.
     * @return The 32-bit value.
     */
    inline uint32_t ReadLe32(const uint8_t *buf)
    {
        return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8)
               | (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
    }

    /**
     * @brief Encode an input command into a Luma3DS input packet.
     * @param cmd The input command to encode.
     * @param buf The byte array to encode the input command into.
     */
    inline void EncodeLuma3DSPacket(const InputCommand &cmd, uint8_t *buf)
    {
        uint32_t hid = 0x00000FFFu & ~(cmd.buttonsPressed & 0x0FFFu);
        WriteLe32(buf + 0, hid);

        if (cmd.touch.touching)
        {
            uint32_t touch = (static_cast<uint32_t>(cmd.touch.y) << 16) | static_cast<uint32_t>(cmd.touch.x);
            WriteLe32(buf + 4, touch);
        }
        else
        {
            WriteLe32(buf + 4, 0x02000000u);
        }

        uint32_t cx = static_cast<uint32_t>(static_cast<int>(cmd.circlePad.x * 0x5D0) + 0x800) & 0xFFFu;
        uint32_t cy = static_cast<uint32_t>(static_cast<int>(cmd.circlePad.y * 0x5D0) + 0x800) & 0xFFFu;
        WriteLe32(buf + 8, (cy << 12) | cx);

        uint32_t csx = static_cast<uint32_t>(static_cast<int>(cmd.cStick.x * 0x7F) + 0x80) & 0xFFu;
        uint32_t csy = static_cast<uint32_t>(static_cast<int>(cmd.cStick.y * 0x7F) + 0x80) & 0xFFu;
        WriteLe32(buf + 12, (csy << 24) | (csx << 16) | 0x0081u);

        WriteLe32(buf + 16, cmd.interfaceButtons);
    }
} // namespace SH3DS::Input
