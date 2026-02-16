#include "TextureUploader.h"

#include <opencv2/imgproc.hpp>

namespace SH3DS::App
{
    GLuint TextureUploader::CreateTexture()
    {
        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return textureId;
    }

    void TextureUploader::Upload(const cv::Mat &mat, GLuint textureId)
    {
        if (mat.empty() || textureId == 0)
        {
            return;
        }

        cv::Mat rgb;
        if (mat.channels() == 4)
        {
            cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
        }
        else if (mat.channels() == 3)
        {
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        }
        else
        {
            rgb = mat;
        }

        glBindTexture(GL_TEXTURE_2D, textureId);
        GLenum format = (rgb.channels() == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, rgb.cols, rgb.rows, 0, format, GL_UNSIGNED_BYTE, rgb.data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace SH3DS::App
