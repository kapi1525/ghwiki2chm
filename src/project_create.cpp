#include <format>

#include "project.hpp"

using namespace RUtils;



ErrorOr<chm::ProjectData> chm::create_project_data_from_ghwiki(ProjectConfig &config, std::filesystem::path default_file) {
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
            config.toc_create_items_for_sections = false;
            sidebar_path = file;
        }
        else if(file.extension() == ".md") {
            data.files.push_back({file});
        }
        else if(file.extension() == ".html") {
            data.files.push_back({file});
        }
    }

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

    if(!data.default_file_link) {
        data.default_file_link = &data.files.front();

        for (auto &&file : data.files) {
            if(file.original.filename() == "Home.md") {
                data.default_file_link = &file;
                break;
            }
        }
    }

    if (!sidebar_path.empty()) {
        std::printf("TOC will be created from sidebar: %s\n", sidebar_path.c_str());
        data.toc_root = create_toc_entries_from_sidebar(config, data, sidebar_path);
    }
    else if (!config.toc_root_item_name.empty()) {
        data.toc_root.children.push_back({.name = config.toc_root_item_name, .file_link = nullptr});
    }

    return data;
}