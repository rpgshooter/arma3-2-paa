#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace arma3 {

struct ImageData {
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data; // RGBA format
};

class ImageLoader {
public:
    // Load PNG file
    static ImageData loadPNG(const std::string& filename);

    // Load TGA file
    static ImageData loadTGA(const std::string& filename);

    // Auto-detect and load
    static ImageData load(const std::string& filename);

    // Save PNG file
    static void savePNG(const std::string& filename, const ImageData& image);

private:
    static bool isPNG(const std::string& filename);
    static bool isTGA(const std::string& filename);
};

} // namespace arma3