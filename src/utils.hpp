#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <source_location>
#include <variant>
#include <functional>



#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #define PLATFORM_POSIX
#endif

namespace utils {
    // Process creation
    std::filesystem::path find_executable(std::string command);
    int run_process(std::filesystem::path executable, const std::vector<std::string>& args, std::filesystem::path process_current_path = std::filesystem::current_path());

    // Very simple parser to split urls into parts
    struct url {
        std::string protocol, login, password, host, port, resource_path, query, fragment;
    };

    bool parse_url(std::string& str, utils::url* out);
    std::string to_string(url& in);

    void unreachable(const std::source_location location = std::source_location::current());

    // https://en.cppreference.com/w/cpp/utility/variant/visit
    template<class... Ts>
    struct visit_helper : Ts... { using Ts::operator()...; };

    // Command line parsing
    struct cmd_parser_arg_definition {
        char short_flag;
        const char* long_flag;
        std::variant<std::function<void()>, std::function<void(std::string)>> callback;  // called when arg is present argument value is passed
        const char* argument_name;
        const char* help_string;
    };

    class cmd_parser {
    public:
        const char* program_name;
        std::vector<cmd_parser_arg_definition> arg_definitions;

        // Returns false on invalid command line input, can be used to print help message.
        bool parse(int argc, const char* argv[]);

        void display_help_string();

    private:
        bool is_short_flag(const char* arg);
        bool is_long_flag(const char* arg);
        bool is_a_flag(const char* arg);
    };

}