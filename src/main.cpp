#include <cstdio>
#include <iostream>
#include <filesystem>

#include "args.hpp"
#include "chm.hpp"



// ghwiki2chm -n "My help!" -r "/path/to/md/files" -o "/path/to/out.chm" -t "/path/to/temp/dir"


void print_help() {
    const char* help_string = ""
        "Usage: ghwiki2chm [options]\n"
        "\n"
        "Options:\n"
        "  -h             Show this help message.\n"
        "  -n <name>      Project name, will be visible in compiled chm.\n"
        "  -r <directory> Project root.          (Default: .)\n"
        "  -d <file>      Project default file.\n"
        "  -t <directory> Temp directory.        (Default: ./temp)\n"
        "  -o <file>      Output .chm file path. (Default: ./out.chm)\n"
        "";

    std::printf("%s\n", help_string);

    exit(0);
}


int main(int argc, const char *argv[]) {
    chm::project proj;

    proj.title = "Untitled project";
    proj.root_path = std::filesystem::current_path();
    proj.temp_path = std::filesystem::current_path() / "temp";
    proj.out_file = std::filesystem::current_path() / "out.chm";

    std::filesystem::path default_file;

    cmd_parser cmd = {
        {
            {'h', []() {
                print_help();
            }},
        },
        {
            // Project name
            {'n', [&](std::string param) {
                proj.title = param;
            }},
            // Project root path, default: .
            {'r', [&](std::string param) {
                proj.root_path = std::filesystem::absolute(param);
            }},
            // Project default file.
            {'d', [&](std::string param) {
                default_file = std::filesystem::absolute(param);
            }},
            // Temp path default: ./temp
            {'t', [&](std::string param) {
                proj.temp_path = std::filesystem::absolute(param);
            }},
            // Output chm file path
            {'o', [&](std::string param) {
                proj.out_file = std::filesystem::absolute(param);
            }},
        }
    };

    if(!cmd.parse(argc, argv)) {
        print_help();
    }

    std::filesystem::create_directories(proj.temp_path);

    proj.create_from_ghwiki(default_file);
    proj.convert_source_files();
    // proj.scan_html_for_dependencies();
    proj.create_default_toc();
    proj.generate_project_files();

    auto* compiler = chm::find_available_compiler();
    if(!compiler) {
        std::printf("Could'nt find any compatible chm compiler, make sure one is installed.\n");
        return 0;
    }
    if(!chm::compile(&proj, compiler)) {
        std::printf("Compilation failed.\n");
    }

    return 0;
}
