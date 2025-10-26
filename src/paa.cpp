#include "paa.h"
#include "utils.h"
#include "image_loader.h"

#include <squish.h>
//#include <lzo/lzo1x.h>  // LZO disabled for now
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace arma3 {

using namespace utils;

PAA::PAA() : format(PAAFormat::DXT5), magicNumber(0xFF05) {}

PAA::PAA(const std::string& filename) {
    inputStream = std::make_shared<std::ifstream>(filename, std::ios::binary);
}

PAA::PAA(const std::vector<uint8_t>& data) {
    inputStream = std::make_shared<std::stringstream>(
        std::string(data.begin(), data.end())
    );
}

void PAA::readPAA() {
    if (!inputStream) {
        throw std::runtime_error("No input stream available");
    }

    auto& stream = *inputStream;

    // Read magic number
    magicNumber = readBytes<uint16_t>(stream);

    switch (magicNumber) {
        case 0xFF01: format = PAAFormat::DXT1; break;
        case 0xFF02: format = PAAFormat::DXT2; break;
        case 0xFF03: format = PAAFormat::DXT3; break;
        case 0xFF04: format = PAAFormat::DXT4; break;
        case 0xFF05: format = PAAFormat::DXT5; break;
        case 0x4444: format = PAAFormat::RGBA4444; break;
        case 0x1555: format = PAAFormat::RGBA5551; break;
        case 0x8888: format = PAAFormat::RGBA8888; break;
        case 0x8080: format = PAAFormat::GRAY_ALPHA; break;
        default:
            throw std::runtime_error("Invalid PAA magic number: " + std::to_string(magicNumber));
    }

    // Read tags
    while (stream.peek() != 0) {
        Tagg tagg;
        tagg.signature = readString(stream, 8);
        tagg.dataLength = readBytes<uint32_t>(stream);
        tagg.data = readBytes<uint8_t>(stream, tagg.dataLength);
        taggs.push_back(tagg);

        if (tagg.signature == "GGATGALF") {
            hasTransparency = true;
        }
    }

    // Read palette
    palette.dataLength = readBytes<uint16_t>(stream);
    if (palette.dataLength > 0) {
        palette.data = readBytes<uint8_t>(stream, palette.dataLength);
    }

    // Read mipmaps
    while (peekBytes<uint16_t>(stream) != 0) {
        MipMap mipmap;
        mipmap.width = readBytes<uint16_t>(stream);
        mipmap.height = readBytes<uint16_t>(stream);
        mipmap.dataLength = readBytesAsArmaUShort(stream);
        mipmap.data = readBytes<uint8_t>(stream, mipmap.dataLength);

        // Check for LZO compression flag
        if ((mipmap.width & 0x8000) != 0) {
            mipmap.width &= 0x7FFF;
            mipmap.lzoCompressed = true;
            decompressLZO(mipmap);
        }

        // Decompress DXT
        if (format == PAAFormat::DXT1) {
            decompressDXT1(mipmap);
        } else if (format == PAAFormat::DXT5) {
            decompressDXT5(mipmap);
        }

        mipMaps.push_back(mipmap);
    }
}

void PAA::loadImage(const std::string& filename) {
    ImageData img = ImageLoader::load(filename);

    mipMaps.clear();

    MipMap mipmap;
    mipmap.width = img.width;
    mipmap.height = img.height;
    mipmap.data = img.data;
    mipmap.dataLength = img.data.size();

    mipMaps.push_back(mipmap);
    calculateMipmapsAndTaggs();
}

void PAA::calculateMipmapsAndTaggs() {
    if (mipMaps.empty()) {
        throw std::runtime_error("No mipmaps to calculate from");
    }

    uint32_t curWidth = mipMaps[0].width;
    uint32_t curHeight = mipMaps[0].height;

    // Generate mipmaps
    std::vector<MipMap> generatedMips;
    generatedMips.push_back(mipMaps[0]);

    while (std::min(curWidth, curHeight) > 4) {
        uint32_t newWidth = curWidth / 2;
        uint32_t newHeight = curHeight / 2;

        MipMap mipmap;
        mipmap.width = newWidth;
        mipmap.height = newHeight;
        mipmap.data.resize(newWidth * newHeight * 4);

        // Simple bilinear downsampling
        const auto& srcData = generatedMips.back().data;
        for (uint32_t y = 0; y < newHeight; y++) {
            for (uint32_t x = 0; x < newWidth; x++) {
                uint32_t sx = x * 2;
                uint32_t sy = y * 2;

                for (int c = 0; c < 4; c++) {
                    uint32_t p1 = srcData[(sy * curWidth + sx) * 4 + c];
                    uint32_t p2 = srcData[(sy * curWidth + sx + 1) * 4 + c];
                    uint32_t p3 = srcData[((sy + 1) * curWidth + sx) * 4 + c];
                    uint32_t p4 = srcData[((sy + 1) * curWidth + sx + 1) * 4 + c];

                    mipmap.data[(y * newWidth + x) * 4 + c] = (p1 + p2 + p3 + p4) / 4;
                }
            }
        }

        mipmap.dataLength = mipmap.data.size();
        generatedMips.push_back(mipmap);

        curWidth = newWidth;
        curHeight = newHeight;
    }

    mipMaps = generatedMips;

    // Calculate average color
    for (size_t i = 0; i < mipMaps[0].data.size(); i += 4) {
        averageRed += mipMaps[0].data[i];
        averageGreen += mipMaps[0].data[i + 1];
        averageBlue += mipMaps[0].data[i + 2];
        averageAlpha += mipMaps[0].data[i + 3];
    }

    uint32_t pixelCount = mipMaps[0].width * mipMaps[0].height;
    averageRed /= pixelCount;
    averageGreen /= pixelCount;
    averageBlue /= pixelCount;
    averageAlpha /= pixelCount;

    // Create tags
    taggs.clear();

    // Average color tag
    Tagg taggAvg;
    taggAvg.signature = "GGATCGVA";
    taggAvg.data = {
        static_cast<uint8_t>(averageRed),
        static_cast<uint8_t>(averageGreen),
        static_cast<uint8_t>(averageBlue),
        static_cast<uint8_t>(averageAlpha)
    };
    taggAvg.dataLength = 4;
    taggs.push_back(taggAvg);

    // Max color tag
    Tagg taggMax;
    taggMax.signature = "GGATCXAM";
    taggMax.data = {0xFF, 0xFF, 0xFF, 0xFF};
    taggMax.dataLength = 4;
    taggs.push_back(taggMax);

    // Transparency flag
    if (averageAlpha != 255) {
        hasTransparency = true;
        Tagg taggFlag;
        taggFlag.signature = "GGATGALF";
        taggFlag.data = {0x01, 0xFF, 0xFF, 0xFF};
        taggFlag.dataLength = 4;
        taggs.push_back(taggFlag);
    }
}

void PAA::writePAA(const std::string& filename, PAAFormat targetFormat) {
    if (mipMaps.size() <= 1) {
        calculateMipmapsAndTaggs();
    }

    // Determine format
    if (targetFormat == PAAFormat::UNKNOWN) {
        format = hasTransparency ? PAAFormat::DXT5 : PAAFormat::DXT1;
    } else {
        format = targetFormat;
    }

    // Copy mipmaps for encoding
    std::vector<MipMap> encodedMips = mipMaps;

    // Compress with DXT
    if (format == PAAFormat::DXT5) {
        magicNumber = 0xFF05;
        for (auto& mip : encodedMips) {
            compressDXT5(mip);
        }
    } else if (format == PAAFormat::DXT1) {
        magicNumber = 0xFF01;
        for (auto& mip : encodedMips) {
            compressDXT1(mip);
        }
    }

    // Apply LZO compression to large mipmaps (DISABLED - LZO not linked)
    /*if (encodedMips[0].width > 128) {
        if (lzo_init() != LZO_E_OK) {
            throw std::runtime_error("LZO initialization failed");
        }

        for (size_t i = 0; i < encodedMips.size() && encodedMips[i].width > 128; i++) {
            compressLZO(encodedMips[i]);
        }
    }*/

    // Calculate offsets tag
    Tagg taggOffs;
    taggOffs.signature = "GGATSFFO";

    uint32_t offset = 2; // magic number

    for (const auto& tagg : taggs) {
        offset += 8 + 4 + tagg.dataLength;
    }

    offset += 8 + 4 + 16 * 4; // OFFSTAGG itself
    offset += 2; // palette length

    for (const auto& mip : encodedMips) {
        uint32_t mipOffset = offset;
        taggOffs.data.push_back(mipOffset & 0xFF);
        taggOffs.data.push_back((mipOffset >> 8) & 0xFF);
        taggOffs.data.push_back((mipOffset >> 16) & 0xFF);
        taggOffs.data.push_back((mipOffset >> 24) & 0xFF);

        offset += 2 + 2 + 3 + mip.dataLength;
    }

    taggOffs.dataLength = taggOffs.data.size();

    // Write file
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Failed to open output file: " + filename);
    }

    writeBytes(ofs, magicNumber);

    for (const auto& tagg : taggs) {
        writeString(ofs, tagg.signature);
        writeBytes(ofs, tagg.dataLength);
        writeBytes(ofs, tagg.data);
    }

    writeString(ofs, taggOffs.signature);
    writeBytes(ofs, taggOffs.dataLength);
    writeBytes(ofs, taggOffs.data);

    writeBytes(ofs, palette.dataLength);

    for (const auto& mip : encodedMips) {
        uint16_t width = mip.width;
        if (mip.lzoCompressed) {
            width |= 0x8000;
        }
        writeBytes(ofs, width);
        writeBytes(ofs, mip.height);
        writeBytesAsArmaUShort(ofs, mip.dataLength);
        writeBytes(ofs, mip.data);
    }

    writeBytes<uint16_t>(ofs, 0);
    writeBytes<uint16_t>(ofs, 0);
    writeBytes<uint16_t>(ofs, 0);

    ofs.close();
}

void PAA::compressDXT1(MipMap& mipmap) {
    size_t compressedSize = mipmap.dataLength / 8;
    std::vector<uint8_t> compressed(compressedSize);

    squish::CompressImage(
        mipmap.data.data(),
        mipmap.width,
        mipmap.height,
        compressed.data(),
        squish::kDxt1
    );

    mipmap.data = compressed;
    mipmap.dataLength = compressedSize;
}

void PAA::compressDXT5(MipMap& mipmap) {
    size_t compressedSize = mipmap.dataLength / 4;
    std::vector<uint8_t> compressed(compressedSize);

    squish::CompressImage(
        mipmap.data.data(),
        mipmap.width,
        mipmap.height,
        compressed.data(),
        squish::kDxt5
    );

    mipmap.data = compressed;
    mipmap.dataLength = compressedSize;
}

void PAA::decompressDXT1(MipMap& mipmap) {
    size_t uncompressedSize = mipmap.dataLength * 8;
    std::vector<uint8_t> uncompressed(uncompressedSize);

    squish::DecompressImage(
        uncompressed.data(),
        mipmap.width,
        mipmap.height,
        mipmap.data.data(),
        squish::kDxt1
    );

    mipmap.data = uncompressed;
    mipmap.dataLength = uncompressedSize;
}

void PAA::decompressDXT5(MipMap& mipmap) {
    size_t uncompressedSize = mipmap.dataLength * 4;
    std::vector<uint8_t> uncompressed(uncompressedSize);

    squish::DecompressImage(
        uncompressed.data(),
        mipmap.width,
        mipmap.height,
        mipmap.data.data(),
        squish::kDxt5
    );

    mipmap.data = uncompressed;
    mipmap.dataLength = uncompressedSize;
}

void PAA::compressLZO(MipMap& mipmap) {
    // LZO DISABLED - not linked
    throw std::runtime_error("LZO compression not available in this build");
}

void PAA::decompressLZO(MipMap& mipmap) {
    // LZO DISABLED - not linked
    throw std::runtime_error("LZO decompression not available in this build");
}

std::vector<uint8_t> PAA::getRawPixelData(uint8_t level) {
    if (level >= mipMaps.size()) {
        return {};
    }
    return mipMaps[level].data;
}

void PAA::setRawPixelData(const std::vector<uint8_t>& data, uint8_t level) {
    if (level < mipMaps.size()) {
        mipMaps[level].data = data;
        mipMaps[level].dataLength = data.size();
    }
}

void PAA::writeImage(const std::string& filename, int mipLevel) {
    if (mipLevel >= mipMaps.size()) {
        throw std::out_of_range("Mipmap level out of range");
    }

    ImageData img;
    img.width = mipMaps[mipLevel].width;
    img.height = mipMaps[mipLevel].height;
    img.data = mipMaps[mipLevel].data;

    ImageLoader::savePNG(filename, img);
}

} // namespace arma3