/**
 * @file emCore_dummy.cpp
 * @brief Dummy source file to trigger CMake processing for emCore library
 * 
 * This file exists solely to ensure that PlatformIO/ESP-IDF processes
 * the emCore library's CMakeLists.txt, which triggers automatic task
 * generation from YAML files.
 * 
 * Without this file, the library would be header-only and CMake would
 * never process the library's build configuration.
 */

// This function is never called - it just ensures the library has compiled code
namespace emCore {
    void __dummy_function_to_trigger_cmake() {
        // This function intentionally does nothing
        // It exists only to make CMake process this library
    }
}
