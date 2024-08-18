#include <cstdio>
#include <filesystem>

#include "chm.hpp"
#include "utils.hpp"



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

    utils::cmd_parser cmd = {
        .program_name = "ghwiki2chm",
        .arg_definitions = {
            {
                'h',
                "help",
                [&]() {
                    cmd.display_help_string();
                    exit(0);
                },
                nullptr,
                "Display this help message.",
            },
            {
                'n',
                "name",
                [&](std::string param) {
                    proj.title = param;
                },
                "name",
                "Project name. will be visible in compiled chm.",
            },
            {
                'r',
                "root",
                [&](std::string param) {
                    proj.root_path = std::filesystem::absolute(param);
                },
                "directory",
                "Project root.",
            },
            {
                'd',
                "default-page",
                [&](std::string param) {
                    default_file = std::filesystem::absolute(param);
                },
                "file",
                "Page that will be opened when .chm file is opened.",
            },
            {
                't',
                "temp-path",
                [&](std::string param) {
                    proj.temp_path = std::filesystem::absolute(param);
                },
                "directory",
                "Temp directory.",
            },
            {
                'o',
                "out-file",
                [&](std::string param) {
                    proj.out_file = std::filesystem::absolute(param);
                },
                "file",
                "Output .chm file path.",
            },
        },
    };

    if(!cmd.parse(argc, argv)) {
        cmd.display_help_string();
        return 0;
    }

    std::filesystem::create_directories(proj.temp_path);

    proj.create_from_ghwiki(default_file);
    proj.convert_source_files();
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
