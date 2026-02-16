#pragma once

#include <opencv2/core.hpp>

#include <glad/glad.h>

namespace SH3DS::App
{
    /**
     * @brief Utility for uploading cv::Mat images to OpenGL textures.
     */
    class TextureUploader
    {
    public:
        /**
         * @brief Creates a new OpenGL texture with default parameters.
         * @return OpenGL texture ID.
         */
        static GLuint CreateTexture();

        /**
         * @brief Uploads a cv::Mat to an existing OpenGL texture.
         * @param mat Source image (BGR or BGRA).
         * @param textureId Target OpenGL texture ID.
         */
        static void Upload(const cv::Mat &mat, GLuint textureId);
    };
} // namespace SH3DS::App
