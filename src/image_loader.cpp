#include "image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <stdexcept>
#include <algorithm>

namespace arma3 {

ImageData ImageLoader::loadPNG(const std::string& filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

    if (!data) {
        throw std::runtime_error("Failed to load PNG: " + filename);
    }

    ImageData img;
    img.width = width;
    img.height = height;
    img.data = std::vector<uint8_t>(data, data + (width * height * 4));

    stbi_image_free(data);
    return img;
}

ImageData ImageLoader::loadTGA(const std::string& filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

    if (!data) {
        throw std::runtime_error("Failed to load TGA: " + filename);
    }

    ImageData img;
    img.width = width;
    img.height = height;
    img.data = std::vector<uint8_t>(data, data + (width * height * 4));

    stbi_image_free(data);
    return img;
}

ImageData ImageLoader::load(const std::string& filename) {
    // stb_image auto-detects format
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);

    if (!data) {
        throw std::runtime_error("Failed to load image: " + filename + " - " + stbi_failure_reason());
    }

    ImageData img;
    img.width = width;
    img.height = height;
    img.data = std::vector<uint8_t>(data, data + (width * height * 4));

    stbi_image_free(data);
    return img;
}

void ImageLoader::savePNG(const std::string& filename, const ImageData& image) {
    int result = stbi_write_png(
        filename.c_str(),
        image.width,
        image.height,
        4,
        image.data.data(),
        image.width * 4
    );

    if (!result) {
        throw std::runtime_error("Failed to save PNG: " + filename);
    }
}

bool ImageLoader::isPNG(const std::string& filename) {
    std::string ext = filename.substr(filename.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".png";
}

bool ImageLoader::isTGA(const std::string& filename) {
    std::string ext = filename.substr(filename.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".tga";
}

} // namespace arma3