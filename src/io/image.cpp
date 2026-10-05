#include "io/image.h"

#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace vulchovy {

namespace {

ImageData build(float* floats, stbi_uc* bytes, int width, int height, int channels,
                int desired_channels) {
    ImageData image;
    if ((floats == nullptr && bytes == nullptr) || width <= 0 || height <= 0) {
        if (floats != nullptr)
            stbi_image_free(floats);
        if (bytes != nullptr)
            stbi_image_free(bytes);
        return image;
    }

    image.hdr = floats != nullptr;
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    image.channels = static_cast<uint32_t>(desired_channels > 0 ? desired_channels : channels);

    const size_t count = static_cast<size_t>(width) * height * image.channels;
    if (floats != nullptr) {
        image.floats.assign(floats, floats + count);
        stbi_image_free(floats);
    } else {
        image.bytes.assign(bytes, bytes + count);
        stbi_image_free(bytes);
    }
    return image;
}

} // namespace

ImageData load_image(const std::filesystem::path& path, int desired_channels,
                     bool flip_vertically) {
    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);

    const std::string file = path.string();
    int width = 0;
    int height = 0;
    int channels = 0;

    if (stbi_is_hdr(file.c_str())) {
        float* data = stbi_loadf(file.c_str(), &width, &height, &channels, desired_channels);
        if (data == nullptr)
            std::fprintf(stderr, "vulchovy: could not load HDR image '%s': %s\n", file.c_str(),
                         stbi_failure_reason());
        return build(data, nullptr, width, height, channels, desired_channels);
    }

    stbi_uc* data = stbi_load(file.c_str(), &width, &height, &channels, desired_channels);
    if (data == nullptr)
        std::fprintf(stderr, "vulchovy: could not load image '%s': %s\n", file.c_str(),
                     stbi_failure_reason());
    return build(nullptr, data, width, height, channels, desired_channels);
}

ImageData load_image_from_memory(const void* data, size_t size, int desired_channels,
                                 bool flip_vertically) {
    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);

    const stbi_uc* bytes = static_cast<const stbi_uc*>(data);
    const int length = static_cast<int>(size);
    int width = 0;
    int height = 0;
    int channels = 0;

    if (stbi_is_hdr_from_memory(bytes, length)) {
        float* decoded =
            stbi_loadf_from_memory(bytes, length, &width, &height, &channels, desired_channels);
        if (decoded == nullptr)
            std::fprintf(stderr, "vulchovy: could not decode embedded HDR image: %s\n",
                         stbi_failure_reason());
        return build(decoded, nullptr, width, height, channels, desired_channels);
    }

    stbi_uc* decoded =
        stbi_load_from_memory(bytes, length, &width, &height, &channels, desired_channels);
    if (decoded == nullptr)
        std::fprintf(stderr, "vulchovy: could not decode embedded image: %s\n",
                     stbi_failure_reason());
    return build(nullptr, decoded, width, height, channels, desired_channels);
}

} // namespace vulchovy
