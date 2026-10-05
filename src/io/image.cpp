#include "io/image.h"

#include <cstdio>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace vulchovy {

ImageData load_image(const std::filesystem::path& path, int desired_channels,
                     bool flip_vertically) {
    ImageData image;

    // stb's flip flag is process-global state; set it on every call.
    stbi_set_flip_vertically_on_load(flip_vertically ? 1 : 0);

    const std::string file = path.string();
    int width = 0;
    int height = 0;
    int channels = 0;

    if (stbi_is_hdr(file.c_str())) {
        float* data = stbi_loadf(file.c_str(), &width, &height, &channels, desired_channels);
        if (data == nullptr) {
            std::fprintf(stderr, "vulchovy: could not load HDR image '%s': %s\n", file.c_str(),
                         stbi_failure_reason());
            return image;
        }
        image.hdr = true;
        image.channels = static_cast<uint32_t>(desired_channels > 0 ? desired_channels : channels);
        image.floats.assign(data, data + static_cast<size_t>(width) * height * image.channels);
        stbi_image_free(data);
    } else {
        stbi_uc* data = stbi_load(file.c_str(), &width, &height, &channels, desired_channels);
        if (data == nullptr) {
            std::fprintf(stderr, "vulchovy: could not load image '%s': %s\n", file.c_str(),
                         stbi_failure_reason());
            return image;
        }
        image.channels = static_cast<uint32_t>(desired_channels > 0 ? desired_channels : channels);
        image.bytes.assign(data, data + static_cast<size_t>(width) * height * image.channels);
        stbi_image_free(data);
    }

    if (width <= 0 || height <= 0 || image.channels == 0)
        return ImageData{};

    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    return image;
}

} // namespace vulchovy
