#include "FrameSeeker.h"
#include "FrameSource.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace SH3DS::Capture
{

    /**
     * @brief Reads frames sequentially from a directory of PNG/JPG files.
     * Critical for replay testing — enables fully deterministic offline pipeline.
     */
    class FileFrameSource
        : public FrameSource
        , public FrameSeeker
    {
    public:
        /**
         * @brief Constructs a new FileFrameSource.
         * @param directory The directory containing the frames.
         * @param playbackFps The playback speed in frames per second.
         */
        explicit FileFrameSource(const std::filesystem::path &directory, double playbackFps = 0.0);

        bool Open() override;

        void Close() override;

        std::optional<Core::Frame> Grab() override;

        bool IsOpen() const override;

        std::string Describe() const override;

        bool Seek(size_t frameIndex) override;
        size_t GetFrameCount() const override;

        /**
         * @brief Creates a file frame source from a directory of frames.
         * @param directory The directory containing the frames.
         * @param playbackFps The playback speed in frames per second.
         * @return A unique pointer to the frame source.
         */
        static std::unique_ptr<FrameSource> CreateFileFrameSource(const std::filesystem::path &directory,
            double playbackFps);

    private:
        std::filesystem::path directory;
        double playbackFps;
        std::vector<std::filesystem::path> framePaths;
        size_t currentIndex = 0;
        bool open = false;
    };
} // namespace SH3DS::Capture
