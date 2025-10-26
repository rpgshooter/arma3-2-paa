#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

namespace arma3 {
namespace utils {

// Read functions
template<typename T>
T readBytes(std::istream& stream) {
    T value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

template<typename T>
std::vector<T> readBytes(std::istream& stream, size_t count) {
    std::vector<T> data(count);
    stream.read(reinterpret_cast<char*>(data.data()), count * sizeof(T));
    return data;
}

inline std::string readString(std::istream& stream, size_t length) {
    std::vector<char> buffer(length);
    stream.read(buffer.data(), length);
    return std::string(buffer.begin(), buffer.end());
}

inline uint32_t readBytesAsArmaUShort(std::istream& stream) {
    // Arma uses 3-byte unsigned integers for some fields
    uint8_t b1 = readBytes<uint8_t>(stream);
    uint8_t b2 = readBytes<uint8_t>(stream);
    uint8_t b3 = readBytes<uint8_t>(stream);
    return (b3 << 16) | (b2 << 8) | b1;
}

template<typename T>
T peekBytes(std::istream& stream) {
    T value;
    std::streampos pos = stream.tellg();
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    stream.seekg(pos);
    return value;
}

// Write functions
template<typename T>
void writeBytes(std::ostream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
void writeBytes(std::ostream& stream, const std::vector<T>& data) {
    stream.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T));
}

inline void writeString(std::ostream& stream, const std::string& str) {
    stream.write(str.data(), str.length());
}

inline void writeBytesAsArmaUShort(std::ostream& stream, uint32_t value) {
    // Arma uses 3-byte unsigned integers
    uint8_t b1 = value & 0xFF;
    uint8_t b2 = (value >> 8) & 0xFF;
    uint8_t b3 = (value >> 16) & 0xFF;
    writeBytes(stream, b1);
    writeBytes(stream, b2);
    writeBytes(stream, b3);
}

} // namespace utils
} // namespace arma3