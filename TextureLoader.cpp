#include "TextureLoader.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <stdexcept>

namespace TextureLoader {

GLuint gen_tex(cv::Mat& image)
{
    if (image.empty()) {
        throw std::runtime_error("Image empty?");
    }

    GLuint ID = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &ID);

    const int w = image.cols;
    const int h = image.rows;
    const int ch = image.channels();

    switch (ch) {
        case 3:
            glTextureStorage2D(ID, 1, GL_RGB8, w, h);
            // OpenCV 3-channel is BGR
            glTextureSubImage2D(ID, 0, 0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, image.data);
            break;
        case 4:
            glTextureStorage2D(ID, 1, GL_RGBA8, w, h);
            // OpenCV 4-channel is BGRA
            glTextureSubImage2D(ID, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, image.data);
            break;
        default:
            glDeleteTextures(1, &ID);
            throw std::runtime_error(std::string("unsupported channel cnt. in texture: ") + std::to_string(ch));
    }

    // Filtering + mipmaps
    glTextureParameteri(ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateTextureMipmap(ID);

    // Wrap
    glTextureParameteri(ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return ID;
}

GLuint textureInit(const std::filesystem::path& file_name)
{
    cv::Mat image = cv::imread(file_name.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        throw std::runtime_error(std::string("No texture in file: ") + file_name.string());
    }

    if (image.channels() != 3 && image.channels() != 4) {
        std::cerr << "Warning: Image has unsupported channel count: " << image.channels() << std::endl;
    }

    return gen_tex(image);
}

} // namespace TextureLoader
