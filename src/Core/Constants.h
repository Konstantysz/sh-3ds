#pragma once

namespace SH3DS::Core
{
    /** @brief 3DS top screen native width (pixels). */
    inline constexpr int kTopScreenWidth = 400;

    /** @brief 3DS top screen native height (pixels). */
    inline constexpr int kTopScreenHeight = 240;

    /** @brief 3DS top screen aspect ratio (400/240 = 5:3). */
    inline constexpr double kTopScreenAspectRatio = 400.0 / 240.0;

    /** @brief 3DS bottom screen native width (pixels). */
    inline constexpr int kBottomScreenWidth = 320;

    /** @brief 3DS bottom screen native height (pixels). */
    inline constexpr int kBottomScreenHeight = 240;

    /** @brief 3DS bottom screen aspect ratio (320/240 = 4:3). */
    inline constexpr double kBottomScreenAspectRatio = 320.0 / 240.0;
} // namespace SH3DS::Core
