#include "RUtils/Process.hpp"
#include "RUtils/Helpers.hpp"

#include "compiler.hpp"




const chm::compiler_info* chm::find_available_compiler() {
    // List of supported compilers
    // Their executable name, and with what arguments they should be called
    static const compiler_info compiler_infos[] = {
        #ifdef PLATFORM_WINDOWS
        {
            "hhc",
            {compiler_special_arg::project_file_path},
        },
        #endif
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
bool chm::is_compiler_valid(const compiler_info *compiler) {
    if(!compiler) {
        return false;
    }

    return RUtils::find_executable(compiler->executable).empty() == false;
}



bool chm::compile(const ProjectConfig &config, const compiler_info *compiler) {
    std::vector<std::string> args;

    for (auto &&i : compiler->args) {
        std::visit(RUtils::visit_helper{
            [](std::monostate arg) {
                RUtils::Error::unreachable();
            },
            [&](std::string arg) {
                args.push_back(arg);
            },
            [&](compiler_special_arg arg) {
                switch (arg) {
                case compiler_special_arg::project_file_path:
                    args.push_back((config.temp / "proj.hhp").string());
                    return;
                }

                RUtils::Error::unreachable();
            },
        }, i);
    }

    int status = RUtils::run_process(RUtils::find_executable(compiler->executable), args, config.temp);
    std::printf("Compiler exited with exit code: %i.\n", status);
    return status == 0;
}