#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace arma3 {

enum class PAAFormat {
    UNKNOWN = 0,
    DXT1 = 0xFF01,
    DXT2 = 0xFF02,
    DXT3 = 0xFF03,
    DXT4 = 0xFF04,
    DXT5 = 0xFF05,
    RGBA4444 = 0x4444,
    RGBA5551 = 0x1555,
    RGBA8888 = 0x8888,
    GRAY_ALPHA = 0x8080
};

struct MipMap {
    uint16_t width;
    uint16_t height;
    uint32_t dataLength;
    bool lzoCompressed = false;
    std::vector<uint8_t> data;
};

struct Tagg {
    std::string signature;  // 8 bytes
    uint32_t dataLength;
    std::vector<uint8_t> data;
};

struct Palette {
    uint16_t dataLength = 0;
    std::vector<uint8_t> data;
};

class PAA {
public:
    PAA();
    explicit PAA(const std::string& filename);
    explicit PAA(const std::vector<uint8_t>& data);

    // Read existing PAA file
    void readPAA();

    // Load image from file (PNG, TGA, etc.)
    void loadImage(const std::string& filename);

    // Write PAA file
    void writePAA(const std::string& filename, PAAFormat format = PAAFormat::UNKNOWN);

    // Write image file (PNG)
    void writeImage(const std::string& filename, int mipLevel = 0);

    // Get pixel data
    std::vector<uint8_t> getRawPixelData(uint8_t level = 0);

    // Set pixel data
    void setRawPixelData(const std::vector<uint8_t>& data, uint8_t level = 0);

    // Getters
    PAAFormat getFormat() const { return format; }
    const std::vector<MipMap>& getMipMaps() const { return mipMaps; }
    bool hasAlpha() const { return hasTransparency; }

private:
    void calculateMipmapsAndTaggs();
    void compressDXT1(MipMap& mipmap);
    void compressDXT5(MipMap& mipmap);
    void decompressDXT1(MipMap& mipmap);
    void decompressDXT5(MipMap& mipmap);
    void compressLZO(MipMap& mipmap);
    void decompressLZO(MipMap& mipmap);

    PAAFormat format = PAAFormat::DXT5;
    uint16_t magicNumber = 0xFF05;
    bool hasTransparency = false;

    std::vector<MipMap> mipMaps;
    std::vector<Tagg> taggs;
    Palette palette;

    // Average colors
    uint32_t averageRed = 0;
    uint32_t averageGreen = 0;
    uint32_t averageBlue = 0;
    uint32_t averageAlpha = 0;

    std::shared_ptr<std::istream> inputStream;
};

} // namespace arma3