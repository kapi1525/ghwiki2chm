#include "utils.hpp"

#include <string_view>
#include <memory>
#include <cstdlib>          // getenv

#ifdef PLATFORM_POSIX
    #include <spawn.h>      // posic_spawn
    #include <unistd.h>     // environ
    #include <sys/wait.h>   // waitpid
#endif



// Look for executable in PATH
// Made for posix compatible systems but might work on windows
std::filesystem::path utils::find_executable(std::string command) {
    if(std::filesystem::exists(command)) {
        return std::filesystem::absolute(command);
    }

    std::string_view env_path(std::getenv("PATH"));

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



bool utils::run_process(std::filesystem::path executable, std::vector<std::string> args) {
#ifdef PLATFORM_WINDWS
    // TODO

#elif defined(PLATFORM_POSIX)
    std::size_t argc = args.size() + 1;                                                 // +1 for filename
    std::unique_ptr<const char*[]> argv = std::make_unique<const char*[]>(argc + 1);    // +1 for null terminator

    argv[0] = executable.filename().c_str();

    for (std::size_t i = 1; i < argc; i++) {
        argv[i] = args[i - 1].c_str();
    }

    pid_t pid;
    if(posix_spawn(&pid, executable.c_str(), nullptr, nullptr, (char**)argv.get(), environ)) {
        perror("posix_spawn() failed");
        return false;
    }

    int status;
    if(waitpid(pid, &status, 0) != pid) {
        perror("waitpid() failed");
        return false;
    }

    if(status == 0) {
        return true;
    }

#endif

    return false;
}