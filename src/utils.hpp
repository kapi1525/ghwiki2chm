#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <source_location>



#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define PLATFORM_POSIX
#endif

namespace utils {
    std::filesystem::path find_executable(std::string command);
    int run_process(std::filesystem::path executable, std::vector<std::string> args, std::filesystem::path process_current_path = std::filesystem::current_path());

    // Very simple parser to split urls into parts
    struct url {
        std::string protocol, login, password, host, port, resource_path, query, fragment;
    };

    bool parse_url(std::string& str, utils::url* out);
    std::string to_string(url& in);

    void unreachable(const std::source_location location = std::source_location::current());
}