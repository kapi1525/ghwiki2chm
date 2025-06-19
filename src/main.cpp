#include <cstdio>
#include <filesystem>

#include "RUtils/CommandLine.hpp"

#include "chm.hpp"
#include "config.hpp"



int main(int argc, const char *argv[]) {
    // Disable stdout and stderr buffering.
    std::setbuf(stdout, nullptr);
    std::setbuf(stderr, nullptr);

    chm::project proj;

    proj.title = "Untitled project";
    proj.root_path = std::filesystem::current_path();
    proj.temp_path = std::filesystem::current_path() / "temp";
    proj.out_file = std::filesystem::current_path() / "out.chm";

    std::filesystem::path default_file;
    uint32_t max_jobs = UINT32_MAX;
    size_t max_downloads = 8;
    bool ignore_ssl = false;
    bool curl_verbose = false;

    RUtils::CommandLine cmd = {
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
                'v',
                "version",
                [&]() {
                    std::printf("ghwiki2chm v" GHWIKI2CHM_VERSION "\n\n");
                    exit(0);
                },
                nullptr,
                "Display version information.",
            },
            {
                'n',
                "name",
                [&](std::string param) {
                    proj.title = param;
                },
                "name",
                "Project name. Will be visible in compiled chm.",
            },
            {
                'r',
                "root",
                [&](std::string param) {
                    proj.root_path = std::filesystem::absolute(param);
                },
                "directory",
                "Project root. (default: \".\")",
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
                "Temp directory. (default: \"./temp\")",
            },
            {
                'o',
                "out-file",
                [&](std::string param) {
                    proj.out_file = std::filesystem::absolute(param);
                },
                "file",
                "Output .chm file path. (default: \"./out.chm\")",
            },
            {
                'j',
                "jobs",
                [&](std::string param) {
                    if(std::sscanf(param.c_str(), "%u", &max_jobs) != 1 || max_jobs == 0) {
                        std::printf("-j, --jobs: expected a positive number but got: \"%s\". Ignored...\n", param.c_str());
                        max_jobs = UINT32_MAX;
                    }
                },
                "amount",
                "Max number of parallel conversion jobs. (default and limited by: number of cpu threads)",
            },
            {
                0,
                "max-downloads",
                [&](std::string param) {
                    if(std::sscanf(param.c_str(), "%zu", &max_downloads) != 1 || max_downloads == 0) {
                        std::printf("--max-downloads: expected a positive number but got: \"%s\". Ignored...\n", param.c_str());
                        max_downloads = 8;
                    }
                },
                "amount",
                "Max number of parallel file downloads. (default: 8)",
            },
            {
                0,
                "ignore-ssl",
                [&]() {
                    ignore_ssl = true;
                },
                nullptr,
                "Ignore ssl certificates when downloading files.",
            },
            {
                0,
                "curl-verbose",
                [&]() {
                    curl_verbose = true;
                },
                nullptr,
                "Enable verbose output for curl library.",
            },
        },
    };

    if(!cmd.parse(argc, argv)) {
        cmd.display_help_string();
        return 0;
    }

    std::filesystem::create_directories(proj.temp_path);

    if(!proj.create_from_ghwiki(default_file)) {
        std::printf("No .md files found in \"%s\", is this the right directory?\n", proj.root_path.string().c_str());
        return 1;
    }

    proj.convert_source_files(max_jobs);
    proj.download_dependencies(max_downloads, ignore_ssl, curl_verbose);
    proj.generate_project_files();

    auto* compiler = chm::find_available_compiler();
    if(!compiler) {
        std::printf("Couldn't find any compatible chm compiler, make sure one is installed.\n");
        return 1;
    }
    std::printf("Starting compiler...\n");
    if(!chm::compile(&proj, compiler)) {
        std::printf("Compilation failed.\n");
        return 1;
    }

    return 0;
}
