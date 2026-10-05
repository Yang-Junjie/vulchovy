#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace vulchovy {

/// @brief A CPU-decoded image.
///
/// LDR images keep their 8-bit bytes (colour maps stay sRGB encoded, as stored);
/// HDR images are exposed as linear 32-bit floats. Pixels are row-major with the
/// first row at the top, unless @c load_image was asked to flip them, and there
/// are @c width * @c height * @c channels of them.
struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    bool hdr = false;
    std::vector<uint8_t> bytes; // filled when !hdr
    std::vector<float> floats;  // filled when hdr

    bool valid() const { return width > 0 && height > 0 && channels > 0; }
};

/// @brief Decodes an image through stb_image (PNG, JPG, HDR, ...).
/// @param path Image file.
/// @param desired_channels Force a channel count (0 keeps the source count).
/// @param flip_vertically Flip the image so row 0 is the bottom (OpenGL style).
/// @return The decoded image, or an invalid one (with a message on stderr) on failure.
ImageData load_image(const std::filesystem::path& path,
                     int desired_channels = 0,
                     bool flip_vertically = false);

/// @brief Decodes an image from an in-memory buffer (used for embedded textures).
ImageData load_image_from_memory(const void* data,
                                 size_t size,
                                 int desired_channels = 0,
                                 bool flip_vertically = false);

} // namespace vulchovy
