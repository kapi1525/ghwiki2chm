#include <format>

#include "project.hpp"

using namespace RUtils;



ErrorOr<chm::ProjectData> chm::create_project_data_from_ghwiki(const ProjectConfig &config, std::filesystem::path default_file) {
    if(!std::filesystem::exists(config.root)) {
        return Error("Root path doesn't exist.", ErrorType::invalid_argument);
    }

    chm::ProjectData data;

    std::filesystem::path sidebar_path;

    for (auto &&dir_entry : std::filesystem::recursive_directory_iterator(config.root)) {
        // Skip all non-file entries
        if(!dir_entry.is_regular_file()) {
            continue;
        }

        auto file = dir_entry.path();

        // Skip files inside temp directory
        if(file.compare(config.temp) > 0) {
            continue;
        }

        // Add compatible file types to project
        // TODO: Refactor
        if(file.filename() == "_Sidebar.md") {
            sidebar_path = file;
        }
        else if(file.extension() == ".md") {
            data.files.push_back({file});
        }
        else if(file.extension() == ".html") {
            data.files.push_back({file});
        }
    }

    // Search for default file, if provided.
    if(!default_file.empty()) {
        for (auto &&file : data.files) {
            if(file.original == default_file) {
                data.default_file_link = &file;
                break;
            }
        }
    }

    if(data.files.size() == 0) {
        return Error(std::format("Found no files in project root path: \"{}\".", config.root.string()), ErrorType::invalid_argument);
    }

    // Look for common files that may be the defalt.
    if(!data.default_file_link) {
        data.default_file_link = &data.files.front();

        for (auto &&file : data.files) {
            if(file.original.filename() == "Home.md") {
                data.default_file_link = &file;
                break;
            }
        }
    }

    // Custom TOC root
    if (config.toc_root_item_name.empty()) {
        data.toc = &data.toc_root;
    } else {
        data.toc_root.children.push_back({.name = config.toc_root_item_name, .file_link = nullptr});
        data.toc = &data.toc_root.children.front();
    }

    // TOC from _Sidebar
    if (config.toc_use_sidebar) {
        if (sidebar_path.empty()) {
            return Error("No _Sidebar.md file found.", ErrorType::invalid_argument);
        }
        std::printf("TOC will be created from sidebar: %s\n", sidebar_path.c_str());
        *data.toc = create_toc_entries_from_sidebar(config, data, sidebar_path);
    }

    return data;
}