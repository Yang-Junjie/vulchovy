#include <cstdlib>
#include <exception>
#include <iostream>

#include "app/application.h"

int main() {
    try {
        vulchovy::Application application;
        application.run();
    } catch (const std::exception& error) {
        std::cerr << "vulchovy: fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
