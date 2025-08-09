#pragma once

#include <filesystem>
#include <GL/glew.h>

// Forward declaration
namespace cv {
    class Mat;
}

namespace TextureLoader {
    GLuint textureInit(const std::filesystem::path& file_name);
    GLuint gen_tex(cv::Mat& image);
}
