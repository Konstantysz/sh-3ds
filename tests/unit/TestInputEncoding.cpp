#include "Input/InputEncoding.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

TEST(InputEncoding, AllReleasedPacket)
{
    SH3DS::Input::InputCommand cmd;
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 0), 0x00000FFFu);
    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 4), 0x02000000u);

    uint32_t circlePad = SH3DS::Input::ReadLe32(buf.data() + 8);
    EXPECT_EQ(circlePad & 0xFFF, 0x800u);
    EXPECT_EQ((circlePad >> 12) & 0xFFF, 0x800u);

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 16), 0u);
}

TEST(InputEncoding, PressA)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = static_cast<uint32_t>(SH3DS::Input::Button::A);
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 0), 0x00000FFEu);
}

TEST(InputEncoding, SoftResetCombo)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = static_cast<uint32_t>(SH3DS::Input::Button::L) | static_cast<uint32_t>(SH3DS::Input::Button::R)
                         | static_cast<uint32_t>(SH3DS::Input::Button::Start);
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 0), 0x00000CF7u);
}

TEST(InputEncoding, AllButtonsPressed)
{
    SH3DS::Input::InputCommand cmd;
    cmd.buttonsPressed = 0x0FFF;
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 0), 0x00000000u);
}

TEST(InputEncoding, TouchCoordinates)
{
    SH3DS::Input::InputCommand cmd;
    cmd.touch.touching = true;
    cmd.touch.x = 160;
    cmd.touch.y = 120;
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t touch = SH3DS::Input::ReadLe32(buf.data() + 4);
    EXPECT_EQ(touch & 0xFFFF, 160u);
    EXPECT_EQ((touch >> 16) & 0xFFFF, 120u);
}

TEST(InputEncoding, NotTouchingSentinel)
{
    SH3DS::Input::InputCommand cmd;
    cmd.touch.touching = false;
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 4), 0x02000000u);
}

TEST(InputEncoding, CirclePadFullRight)
{
    SH3DS::Input::InputCommand cmd;
    cmd.circlePad.x = 1.0f;
    cmd.circlePad.y = 0.0f;
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t cp = SH3DS::Input::ReadLe32(buf.data() + 8);
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
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    uint32_t cp = SH3DS::Input::ReadLe32(buf.data() + 8);
    uint32_t cpx = cp & 0xFFF;
    EXPECT_LT(cpx, 0x800u);
}

TEST(InputEncoding, InterfaceHomeButton)
{
    SH3DS::Input::InputCommand cmd;
    cmd.interfaceButtons = static_cast<uint32_t>(SH3DS::Input::InterfaceButton::Home);
    std::array<uint8_t, 20> buf = {};
    SH3DS::Input::EncodeLuma3DSPacket(cmd, buf.data());

    EXPECT_EQ(SH3DS::Input::ReadLe32(buf.data() + 16), 0x01u);
}
