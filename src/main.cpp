#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>

#include "app/application.h"

int main(int argc, char** argv) {
    try {
        const std::filesystem::path scene_path = argc > 1 ? std::filesystem::path{argv[1]} : std::filesystem::path{};
        vulchovy::Application application{scene_path};
        application.run();
    } catch (const std::exception& error) {
        std::cerr << "vulchovy: fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
