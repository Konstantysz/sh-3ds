#include "Input/InputCommand.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

void EncodeLuma3DSPacket(const SH3DS::Input::InputCommand &cmd, uint8_t *buf)
{
    auto writeLe32 = [](uint8_t *dst, uint32_t val) {
        dst[0] = static_cast<uint8_t>(val & 0xFF);
        dst[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
        dst[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
        dst[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    };

    uint32_t hid = 0x00000FFFu & ~(cmd.buttonsPressed & 0x0FFFu);
    writeLe32(buf + 0, hid);

    if (cmd.touch.touching)
    {
        uint32_t touch = (static_cast<uint32_t>(cmd.touch.y) << 16) | static_cast<uint32_t>(cmd.touch.x);
        writeLe32(buf + 4, touch);
    }
    else
    {
        writeLe32(buf + 4, 0x02000000u);
    }

    uint32_t cx = static_cast<uint32_t>(static_cast<int>(cmd.circlePad.x * 0x5D0) + 0x800) & 0xFFFu;
    uint32_t cy = static_cast<uint32_t>(static_cast<int>(cmd.circlePad.y * 0x5D0) + 0x800) & 0xFFFu;
    writeLe32(buf + 8, (cy << 12) | cx);

    uint32_t csx = static_cast<uint32_t>(static_cast<int>(cmd.cStick.x * 0x7F) + 0x80) & 0xFFu;
    uint32_t csy = static_cast<uint32_t>(static_cast<int>(cmd.cStick.y * 0x7F) + 0x80) & 0xFFu;
    writeLe32(buf + 12, (csy << 24) | (csx << 16) | 0x0081u);

    writeLe32(buf + 16, cmd.interfaceButtons);
}

uint32_t ReadLe32(const uint8_t *buf)
{
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) | (static_cast<uint32_t>(buf[2]) << 16)
           | (static_cast<uint32_t>(buf[3]) << 24);
}

TEST(InputEncoding, AllReleasedPacket)
{
    SH3DS::Input::InputCommand cmd;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 0), 0x00000FFFu);
    EXPECT_EQ(ReadLe32(buf.data() + 4), 0x02000000u);

    uint32_t circlePad = ReadLe32(buf.data() + 8);
    EXPECT_EQ(circlePad & 0xFFF, 0x800u);
    EXPECT_EQ((circlePad >> 12) & 0xFFF, 0x800u);

    EXPECT_EQ(ReadLe32(buf.data() + 16), 0u);
}

TEST(InputEncoding, PressA)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = static_cast<uint32_t>(SH3DS::Input::Button::A);
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 0), 0x00000FFEu);
}

TEST(InputEncoding, SoftResetCombo)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = static_cast<uint32_t>(SH3DS::Input::Button::L) | static_cast<uint32_t>(SH3DS::Input::Button::R)
                         | static_cast<uint32_t>(SH3DS::Input::Button::Start);
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 0), 0x00000CF7u);
}

TEST(InputEncoding, AllButtonsPressed)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = 0x0FFF;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 0), 0x00000000u);
}

TEST(InputEncoding, TouchCoordinates)
{
    SH3DS::Input::InputCommand cmd;
    cmd.touch.touching = true;
    cmd.touch.x = 160;
    cmd.touch.y = 120;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t touch = ReadLe32(buf.data() + 4);
    EXPECT_EQ(touch & 0xFFFF, 160u);
    EXPECT_EQ((touch >> 16) & 0xFFFF, 120u);
}

TEST(InputEncoding, NotTouchingSentinel)
{
    SH3DS::Input::InputCommand cmd;
    cmd.touch.touching = false;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 4), 0x02000000u);
}

TEST(InputEncoding, CirclePadFullRight)
{
    SH3DS::Input::InputCommand cmd;
    cmd.circlePad.x = 1.0f;
    cmd.circlePad.y = 0.0f;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t cp = ReadLe32(buf.data() + 8);
    uint32_t cpx = cp & 0xFFF;
    uint32_t cpy = (cp >> 12) & 0xFFF;
    EXPECT_GT(cpx, 0x800u);
    EXPECT_EQ(cpy, 0x800u);
}

TEST(InputEncoding, CirclePadFullLeft)
{
    SH3DS::Input::InputCommand cmd;
    cmd.circlePad.x = -1.0f;
    cmd.circlePad.y = 0.0f;
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t cp = ReadLe32(buf.data() + 8);
    uint32_t cpx = cp & 0xFFF;
    EXPECT_LT(cpx, 0x800u);
}

TEST(InputEncoding, InterfaceHomeButton)
{
    SH3DS::Input::InputCommand cmd;
    cmd.interfaceButtons = static_cast<uint32_t>(SH3DS::Input::InterfaceButton::Home);
    std::array<uint8_t, 20> buf = {};
    EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(ReadLe32(buf.data() + 16), 0x01u);
}
