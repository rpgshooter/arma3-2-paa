cmake_minimum_required(VERSION 3.20)
project(Arma3PAAConverter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set vcpkg toolchain if available
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Find packages
find_package(Boost REQUIRED)
find_package(unofficial-libsquish CONFIG REQUIRED)
#find_package(lzo CONFIG REQUIRED)  # LZO only has pkg-config, skip for now
find_package(PNG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(Stb REQUIRED)

# portable-file-dialogs is header-only
find_path(PORTABLE_FILE_DIALOGS_INCLUDE_DIRS "portable-file-dialogs.h")

# Optional: OpenImageIO for broader format support
find_package(OpenImageIO CONFIG)
if(OpenImageIO_FOUND)
    add_compile_definitions(USE_OPENIMAGEIO)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/paa.cpp
    src/image_loader.cpp
)

set(HEADERS
    include/paa.h
    include/image_loader.h
    include/utils.h
)

# CLI Executable
add_executable(arma3-paa-cli ${SOURCES} ${HEADERS})

target_link_libraries(arma3-paa-cli PRIVATE
    unofficial::libsquish::squish
    #lzo::lzo
    PNG::PNG
    Boost::boost
)

target_include_directories(arma3-paa-cli PRIVATE ${Stb_INCLUDE_DIR})

if(OpenImageIO_FOUND)
    target_link_libraries(arma3-paa-cli PRIVATE OpenImageIO::OpenImageIO)
endif()

# GUI Application with Dear ImGui
set(GUI_SOURCES
    src/gui_main.cpp
    src/paa.cpp
    src/image_loader.cpp
)

add_executable(arma3-paa-gui ${GUI_SOURCES})

target_link_libraries(arma3-paa-gui PRIVATE
    unofficial::libsquish::squish
    #lzo::lzo
    PNG::PNG
    Boost::boost
    imgui::imgui
    glfw
    glad::glad
)

target_include_directories(arma3-paa-gui PRIVATE
    ${Stb_INCLUDE_DIR}
    ${PORTABLE_FILE_DIALOGS_INCLUDE_DIRS}
)

if(OpenImageIO_FOUND)
    target_link_libraries(arma3-paa-gui PRIVATE OpenImageIO::OpenImageIO)
endif()

# Installation
install(TARGETS arma3-paa-cli arma3-paa-gui DESTINATION bin)

# Testing (optional)
enable_testing()