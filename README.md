# Arma 3 PAA Converter - Native C++ Edition

High-performance native C++ texture converter for Arma 3's PAA format.

## Features

- **Native C++ Performance** - Uses libsquish for DXT1/DXT5 compression
- **CLI Tool** - Batch conversion from command line
- **GUI Application** - Dear ImGui interface with drag & drop support
- **Format Support** - PNG, TGA, JPG input formats
- **Auto Mipmap Generation** - Generates all mipmap levels automatically
- **DXT Compression** - DXT1 (no alpha) and DXT5 (with alpha)
- **LZO Compression** - Automatic LZO compression for large mipmaps (>128px)

## Requirements

- CMake 3.20+
- C++17 compiler (MSVC, GCC, or Clang)
- vcpkg (for dependency management)

## Installation

### 1. Install vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat  # Windows
```

Set environment variable:
```bash
setx VCPKG_ROOT "C:\path\to\vcpkg"
```

### 2. Build the Project

```bash
cd arma3-paa-converter-native
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

vcpkg will automatically install all dependencies:
- libsquish (DXT compression)
- LZO (additional compression)
- stb (image loading)
- Dear ImGui + GLFW + GLAD (GUI)
- Boost.GIL (image processing)

## Usage

### GUI Application

```bash
./build/Release/arma3-paa-gui.exe
```

Features:
- Drag & drop files directly into the window
- Select output format (Auto, DXT1, DXT5)
- Batch conversion with progress tracking
- Real-time conversion statistics

### CLI Tool

**Single file:**
```bash
arma3-paa-cli texture.png texture.paa
arma3-paa-cli texture.png texture.paa --format DXT5
```

**Batch conversion:**
```bash
arma3-paa-cli --batch "*.png" --output-dir ./paa/
```

## Technical Details

### PAA Format Implementation

**Compression:**
- DXT1: 8:1 compression (RGB, 1-bit alpha)
- DXT5: 4:1 compression (RGBA, 8-bit alpha)
- LZO: Additional compression for textures >128px

**Mipmap Generation:**
- Bilinear downsampling
- Stops at 4x4 minimum size
- Stored in descending order (largest to smallest)

**Tags:**
- AVGCOLOR (GGATCGVA): Average texture color
- MAXCOLOR (GGATCXAM): Maximum color values
- FLAGTRANSP (GGATGALF): Transparency flag
- OFFSETS (GGATSFFO): Mipmap offset table

## Dependencies

- **libsquish** - DXT compression library
- **LZO** - LZO1X compression
- **stb_image** - PNG/TGA/JPG loading
- **stb_image_write** - PNG writing
- **Boost.GIL** - Image resampling for mipmaps
- **Dear ImGui** - Immediate mode GUI
- **GLFW3** - Window management
- **GLAD** - OpenGL loader

## License

MIT License

## Credits

- Based on [gruppe-adler/grad_aff](https://github.com/gruppe-adler/grad_aff)
- Uses [libsquish](https://github.com/svn2github/libsquish) for DXT compression
- Uses [Dear ImGui](https://github.com/ocornut/imgui) for GUI
