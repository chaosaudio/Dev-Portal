#include <dlfcn.h>
#include <iostream>
#include "dsp.hpp" // Include the dsp header file

int main(int argc, char **argv) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <path_to_shared_object> <seconds> <sample_rate> <path_to_model_file>\n";
        return 1;
    }

    const char* libraryPath = argv[1];
    int seconds = std::stoi(argv[2]);
    int sampleRate = std::stoi(argv[3]);
    const char* modelFilePath = argv[4];

    // Load the shared object
    std::cout << "Loading library: " << libraryPath << '\n';
    void* handle = dlopen(libraryPath, RTLD_NOW);
    if (!handle) {
        std::cerr << "Cannot open library: " << dlerror() << '\n';
        return 1;
    }

    // Reset errors
    dlerror();

    // Load the version string
    const char** version_string_ptr = reinterpret_cast<const char**>(dlsym(handle, "dsp_version"));
    if (!version_string_ptr) {
        std::cout << "The 'dsp_version' symbol does not exist in the shared library.\n";
        return 1;
    }

    // Load the "create" symbol
    std::cout << "Loading symbol: create\n";
    dsp_creator_t create = reinterpret_cast<dsp_creator_t>(dlsym(handle, "create"));
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol 'create': " << dlsym_error << '\n';
        dlclose(handle);
        return 1;
    }

    // Create an instance of the class
    std::cout << "Creating instance\n";
    dsp* instance = create();

    // Set sample rate
    std::cout << "Setting sample rate: " << sampleRate << '\n';
    instance->setSampleRate(sampleRate);

    // Set file path
    std::cout << "Setting file path: " << modelFilePath << '\n';
    instance->setFilePath(modelFilePath);

    // Invoking instanceConstants method
    std::cout << "Invoking instanceConstants method\n";
    instance->instanceConstants();

    // Actually perform the benchmark
    std::cout << "Invoking benchmark method\n";
    instance->benchmark(seconds);

    // Close the library
    std::cout << "Deleting instance\n";
    dlclose(handle);
}