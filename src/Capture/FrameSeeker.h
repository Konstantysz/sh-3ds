#pragma once

#include <cstddef>

namespace SH3DS::Capture
{
    /**
     * @brief Abstract interface for random-access frame seeking.
     *
     * Implemented by frame sources that support non-sequential access
     * (e.g., image directories, video files). Live stream sources do not
     * implement this interface.
     */
    class FrameSeeker
    {
    public:
        virtual ~FrameSeeker() = default;

        /**
         * @brief Seeks to a specific frame index.
         * @param frameIndex Zero-based frame index.
         * @return True if seek succeeded.
         */
        virtual bool Seek(size_t frameIndex) = 0;

        /**
         * @brief Returns total number of frames available.
         * @return Frame count.
         */
        virtual size_t GetFrameCount() const = 0;
    };
} // namespace SH3DS::Capture
