#include "chm.hpp"
#include "utils.hpp"



const chm::compiler_info* chm::find_available_compiler() {
    // List of supported compilers
    // Their executable name, and with what arguments they should be called
    static const compiler_info compiler_infos[] = {
        {
            "chmcmd",
            {compiler_special_arg::project_file_path, "--no-html-scan"},
        },
        #ifdef PLATFORM_WINDOWS
        {
            "hhc",
            {compiler_special_arg::project_file_path},
        },
        #endif
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

    return utils::find_executable(compiler->executable).empty() == false;
}



// https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct visit_helper : Ts... { using Ts::operator()...; };

bool chm::compile(project* proj, const compiler_info* compiler) {
    std::vector<std::string> args;

    for (auto &&i : compiler->args) {
        std::visit(visit_helper{
            [](std::monostate arg) {
            },
            [&](std::string arg) {
                args.push_back(arg);
            },
            [&](compiler_special_arg arg) {
                switch (arg) {
                case compiler_special_arg::project_file_path:
                    args.push_back((proj->temp_path / "proj.hhp").string());
                    break;

                default:
                    std::printf("unreachable\n");
                    abort();
                }
            },
        }, i);
    }

    int status = utils::run_process(utils::find_executable(compiler->executable), args, proj->temp_path);
    std::printf("Compiler exited with exit code: %i.\n", status);
    return status == 0;
}