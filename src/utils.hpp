#pragma once

#include <vector>
#include <string>
#include <filesystem>



#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define PLATFORM_POSIX
#endif

namespace utils {
    std::filesystem::path find_executable(std::string command);
    bool run_process(std::filesystem::path executable, std::vector<std::string> args);
}