#include <cstdio>
#include <filesystem>

#include "RUtils/CommandLine.hpp"

#include "project.hpp"
#include "config.hpp"
#include "compiler.hpp"



int main(int argc, const char *argv[]) {
    // Disable stdout and stderr buffering.
    std::setbuf(stdout, nullptr);
    std::setbuf(stderr, nullptr);

    chm::ProjectConfig config;

    auto pwd = std::filesystem::current_path();
    config.root = pwd;
    config.temp = pwd / "temp";
    config.out_file = pwd / "out.chm";

    std::filesystem::path default_file;

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
                    config.title = param;
                },
                "name",
                "Project name. Will be visible in compiled chm.",
            },
            {
                'r',
                "root",
                [&](std::string param) {
                    config.root = std::filesystem::absolute(param);
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
                    config.temp = std::filesystem::absolute(param);
                },
                "directory",
                "Temp directory. (default: \"./temp\")",
            },
            {
                'o',
                "out-file",
                [&](std::string param) {
                    config.out_file = std::filesystem::absolute(param);
                },
                "file",
                "Output .chm file path. (default: \"./out.chm\")",
            },
            {
                'j',
                "jobs",
                [&](std::string param) {
                    if(std::sscanf(param.c_str(), "%u", &config.max_jobs) != 1) {
                        std::printf("-j --jobs: expected a positive number or 0 but got: \"%s\". Ignored...\n", param.c_str());
                    }
                },
                "amount",
                "Max nuber of threads to use during conversion. (default: number of threads)",
            },
            // {
            //     0,
            //     "toc-no-section-links",
            //     [&]() {
            //         config.toc_no_section_links = true;
            //     },
            //     nullptr,
            //     "Don't create TOC items for page sections.",
            // },
            {
                0,
                "toc-auto-generate",
                [&]() {
                    config.toc_generate_automagically = true;
                    config.toc_use_sidebar = false;
                },
                nullptr,
                "Create TOC items automatically. Use if you don't have a _Sidebar.md.",
            },
            {
                0,
                "toc-root-item-name",
                [&](std::string param) {
                    config.toc_root_item_name = param;
                },
                "name",
                "Don't create TOC items for page sections.",
            },
            {
                0,
                "max-downloads",
                [&](std::string param) {
                    if(std::sscanf(param.c_str(), "%u", &config.max_downloads) != 1 || config.max_downloads == 0) {
                        std::printf("--max-downloads: expected a positive number but got: \"%s\". Ignored...\n", param.c_str());
                        config.max_downloads = 8;
                    }
                },
                "amount",
                "Max number of parallel file downloads. (default: 8)",
            },
            {
                0,
                "ignore-ssl",
                [&]() {
                    config.dep_download_ignore_ssl = true;
                },
                nullptr,
                "Ignore ssl certificates when downloading files.",
            },
            {
                0,
                "curl-verbose",
                [&]() {
                    config.dep_download_curl_verbose = true;
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

    std::filesystem::create_directories(config.temp);

    chm::ProjectData data = chm::create_project_data_from_ghwiki(config, default_file);

    chm::convert_project_files(config, data);
    chm::download_dependencies(config, data);
    chm::generate_project_files(config, data);

    auto* compiler = chm::find_available_compiler();
    if(!compiler) {
        std::printf("Couldn't find any compatible chm compiler, make sure one is installed.\n");
        return 1;
    }

    std::printf("Starting compiler...\n");

    if(!chm::compile(config, compiler)) {
        std::printf("Compilation failed.\n");
        return 1;
    }

    return 0;
}
