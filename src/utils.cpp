#include "utils.hpp"

#include <string_view>
#include <memory>
#include <regex>
#include <cstdlib>          // getenv
#include <cstring>          // strlen

#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define STRICT
    #define NOMINMAX
    #include <windows.h>
#endif

#ifdef PLATFORM_POSIX
    #include <spawn.h>      // posic_spawn
    #include <unistd.h>     // environ
    #include <sys/wait.h>   // waitpid
#endif



// Look for executable in PATH
// Made for posix compatible systems but might work on windows
// FIXME: Windows
std::filesystem::path utils::find_executable(std::string command) {
    if(std::filesystem::exists(command)) {
        return std::filesystem::absolute(command);
    }

    std::string_view env_path(std::getenv("PATH"));

    if(env_path.empty()) {
        return {};
    }

    for (size_t i = 0; i < env_path.size(); i++) {
        std::size_t end = env_path.find(':', i);

        if(end == std::string_view::npos) {
            end = env_path.size();
        }

        std::filesystem::path path = env_path.substr(i, end - i);
        i = end;

        if(!std::filesystem::exists(path)) {
            continue;
        }

        for (auto &&i : std::filesystem::directory_iterator(path)) {
            if(!i.is_regular_file()) {
                continue;
            }

            if(i.path().filename() == command) {
                return i.path();
            }
        }
    }

    return {};
}



int utils::run_process(std::filesystem::path executable, const std::vector<std::string>& args, std::filesystem::path process_current_path) {
#ifdef PLATFORM_WINDWS
    std::string temp_cmd;

    for (auto &&arg : args) {
        auto escape_quotes = [](std::string str) -> std::string {
            std::string temp;
            for (auto& c : str) {
                if(c == '"') {
                    temp += '\\';
                }
                temp += c;
            }
            return temp;
        };

        temp_cmd += "\"";
        temp_cmd += escape_quotes(arg);
        temp_cmd += "\"";
    }

    std::unique_ptr<char[]> command_line = std::make_unique<char[]>(temp_cmd.length() + 1); // +1 for null terminator

    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};

    si.cb = sizeof(si);

    if(!CreateProcessA(executable.c_str(), command_line, nullptr, nullptr, false, 0, nullptr, process_current_path.c_str(), &si, &pi)) {
        printf( "CreateProcess failed: %d.\n", GetLastError());
        return -1;
    }

    // Wait until child process to exit
    WaitForSingleObject( pi.hProcess, INFINITE );

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;

#elif defined(PLATFORM_POSIX)
    std::size_t argc = args.size() + 1;                                                 // +1 for filename
    std::unique_ptr<const char*[]> argv = std::make_unique<const char*[]>(argc + 1);    // +1 for null terminator

    argv[0] = executable.filename().c_str();

    for (std::size_t i = 1; i < argc; i++) {
        argv[i] = args[i - 1].c_str();
    }

    pid_t pid = fork();
    if(pid == 0) {
        // child process
        std::filesystem::current_path(process_current_path);
        execv(executable.c_str(), (char**)argv.get());
        perror("execv() failed");
        exit(-1);
    } else if(pid < 0) {
        perror("fork() failed");
        return -1;
    }

    int status = -1;
    if(waitpid(pid, &status, 0) != pid) {
        perror("waitpid() failed");
    }

    return status;

#endif

    return -1;
}



bool utils::parse_url(std::string& str, utils::url* out) {
    // i have sinned
    // FIXME: This doesnt support both # and ? in one link, but i dont care i dont want to touch this mess any more.
    // original un-esacaped ^(?:(.+?):\/\/)?(?:(.+?)(?::(.+?))?@)?([^./]+\..+?)?(?:\/(?:(.+?)(?:$|#(.*)|\?(.*)))|$)
    // group 1 - protocol, 2 - login, 3 - password, 4 - host, 5 - resource path, 6 - document fragment, 7 - query
    std::regex main_regex("^(?:(.+?):\\/\\/)?(?:(.+?)(?::(.+?))?@)?([^./]+\\..+?)?(?:\\/(?:(.+?)(?:$|#(.*)|\\?(.*)))|$)");
    std::regex file_with_fragment_regex("^(.*?)(?:#(.+))?$");

    if(!out || str.empty()) {
        return false;
    }

    // if the url doesnt match the regex, assume its just a path to local resource.
    std::smatch match;
    if(std::regex_match(str, match, main_regex)) {
        out->protocol       = match[1];
        out->login          = match[2];
        out->password       = match[3];
        out->host           = match[4];
        out->resource_path  = match[5];
        out->fragment       = match[6];
        out->query          = match[7];
    } else if(std::regex_match(str, match, file_with_fragment_regex)) {
        out->resource_path = match[1];
        out->fragment = match[2];
    } else {
        out->resource_path = str;
    }

    return true;
}

std::string utils::to_string(url& in) {
    std::string temp;
    if(!in.protocol.empty()) {
        temp += in.protocol;
        temp += "://";
    }
    if(!in.login.empty()) {
        temp += in.login;
        if(!in.password.empty()) {
            temp += ":";
            temp += in.password;
        }
        temp += "@";
    }
    if(!in.host.empty()) {
        temp += in.host;
    }
    if(!in.port.empty()) {
        temp += ":";
        temp += in.port;
    }
    if(!in.resource_path.empty()) {
        temp += "/";
        temp += in.resource_path;
    }
    if(!in.query.empty()) {
        temp += "?";
        temp += in.query;
    }
    if(!in.fragment.empty()) {
        temp += "#";
        temp += in.fragment;
    }

    return temp;
}



void utils::unreachable(const std::source_location location) {
    std::fprintf(stderr, "%s:%d:%d: Unreachable!\n", location.file_name(), location.line(), location.column());
    std::fflush(stdout);
    std::fflush(stderr);
    abort();
}



#if 0
    #define debug_print(...) std::printf(__VA_ARGS__)
#else
    #define debug_print(...)
#endif

bool utils::cmd_parser::parse(int argc, const char* argv[]) {
    debug_print("Parsing command line...\n");

    for (int i = 1; i < argc; i++) {
        auto arg = argv[i];
        debug_print("%s\n", arg);

        if(!is_a_flag(arg)) {
            debug_print("Not a flag.\n");
            return false;
        }

        bool handled = false;
        for (auto &&arg_def : arg_definitions) {
            if((is_short_flag(arg) && arg[1] == arg_def.short_flag) || (is_long_flag(arg) && std::strcmp(arg_def.long_flag, arg+2) == 0)) {
                std::visit(utils::visit_helper{
                    [](std::monostate arg) {
                        unreachable();
                    },
                    [&](std::function<void()> callback) {
                        debug_print("Calling callback...\n");
                        callback();
                    },
                    [&](std::function<void(std::string)> callback) {
                        if(i + 1 < argc && !is_a_flag(argv[i + 1])) {
                            i++;
                            debug_print("Calling callback with \"%s\" argument...\n", argv[i]);
                            callback(argv[i]);
                        }
                    },
                }, arg_def.callback);
                handled = true;
                break;
            }
        }

        if(!handled) {
            debug_print("Unknown flag \"%s\".\n", arg);
            return false;
        }
    }

    return true;
}

void utils::cmd_parser::display_help_string() {
    std::printf("Usage: %s [options]\n\n", program_name);
    std::printf("Options:\n");

    for (auto &&arg : arg_definitions) {
        std::printf("  ");
        if(arg.short_flag) {
            std::printf("-%c ", arg.short_flag);
        }
        if(arg.long_flag) {
            std::printf("--%s ", arg.long_flag);
        }
        if(arg.argument_name) {
            std::printf("<%s> ", arg.argument_name);
        }
        if(arg.help_string) {
            std::printf("%s", arg.help_string);
        }
        std::printf("\n");
    }
    
    std::printf("\n");
}


bool utils::cmd_parser::is_short_flag(const char* arg) {
    return std::strlen(arg) == 2 && arg[0] == '-' && arg[1] != '-';
}

bool utils::cmd_parser::is_long_flag(const char* arg) {
    return std::strlen(arg) > 2 && arg[0] == '-' && arg[1] == '-';
}

bool utils::cmd_parser::is_a_flag(const char* arg) {
    return is_short_flag(arg) || is_long_flag(arg);
}