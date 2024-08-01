#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

// POSIX specific
#include <unistd.h>
#include <sys/wait.h>

#include "chm.hpp"



// Look for executable in PATH
// Made for posix compatible systems but might work on windows
std::filesystem::path chm::find_executable(std::string command) {
    if(std::filesystem::exists(command)) {
        return std::filesystem::absolute(command);
    }

    char* path = std::getenv("PATH");

    std::string_view path_sv(path);

    size_t last_index = 0;
    for (;;) {
        size_t index = path_sv.find(':', last_index);
        size_t size = index - last_index;

        last_index = index + 1;     // +1 to skip : character

        if(index == std::string_view::npos) {
            break;
        }

        // skip if ::
        if(size <= 1) {
            continue;
        }

        std::filesystem::path path = path_sv.substr(last_index, size);

        // std::cout << path << std::endl;

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



const chm::compiler_info* chm::find_available_compiler() {
    // List of supported compilers
    // Their executable name, and with what arguments they should be called
    static const compiler_info compiler_infos[] = {
        {
            "chmcmd",
            {compiler_special_arg::project_file_path, "--no-html-scan"},
        },
    };

    for (auto &&compiler : compiler_infos) {
        if(is_compiler_valid(&compiler)) {
            return &compiler;
        }
    }

    return nullptr;
}



// Currently only checks if it exists
// TODO: Maybe run compilers with --help arg to check if they work?
bool chm::is_compiler_valid(const compiler_info* compiler) {
    if(!compiler) {
        return false;
    }

    return find_executable(compiler->executable).empty() == false;
}



bool chm::compile(project* proj, const compiler_info* compiler) {
    bool ret = false;

    // Create argv for new compiler process
    // This code looks great
    char** argv = new char*[compiler->args.size() + 2]();       // +2 argv[0] is executable name and last element is null terminator

    argv[0] = new char[compiler->executable.size() + 1]();      // +1 for null terminator
    std::strcpy(argv[0], compiler->executable.c_str());

    for (size_t i = 0; i < compiler->args.size(); i++) {
        auto* arg_string = std::get_if<std::string>(&compiler->args[i]);
        if(arg_string) {
            argv[i + 1] = new char[arg_string->size() + 1]();
            std::strcpy(argv[i + 1], arg_string->c_str());
            continue;
        }

        auto arg_special = std::get<compiler_special_arg>(compiler->args[i]);
        switch (arg_special) {
        case compiler_special_arg::project_file_path: {
            auto project_file_path = (proj->temp_path / "proj.hhp").string();
            argv[i + 1] = new char[project_file_path.size() + 1]();
            std::strcpy(argv[i + 1], project_file_path.c_str());
            break;
        }
        default:
            std::printf("unreachable\n");
            abort();
        }
    }


    // Run the compiler
    // Posix only
    // TODO: Windows
    pid_t pid = fork();
    if(pid == 0) {
        // Child process
        std::filesystem::current_path(proj->temp_path);

        if(execvp(compiler->executable.c_str(), argv) == -1) {
            perror("execvp failed");
        }
    } else if(pid > 0) {
        int status;
        if(waitpid(pid, &status, 0) != pid) {
            perror("waitpid() failed");
        }

        if(status == 0) {
            ret = true;
        }
    } else {
        perror("fork() failed");
    }


    // cleanup
    for (size_t i = 0; i < compiler->args.size() + 1; i++) {
        delete[] argv[i];
    }

    delete[] argv;

    return ret;
}