#pragma once

#include <string>
#include <vector>
#include <variant>

#include "project.hpp"



namespace chm {
    // Special arguments that shoud be replaced with actual value before calling the compiler
    enum class compiler_special_arg {
        project_file_path,
    };

    struct compiler_info {
        std::string executable;
        std::vector<std::variant<std::string, compiler_special_arg>> args;
    };


    const compiler_info* find_available_compiler();
    bool is_compiler_valid(const compiler_info *compiler);
    bool compile(const ProjectConfig &config, const compiler_info *compiler);
}