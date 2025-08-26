#pragma once

#include <filesystem>
#include <list>
#include <string>

#include "project_file.hpp"



namespace chm {
    struct TableOfContentsItem {
        std::string name;                                   // Name displayed in TOC tree
        std::string fragment;                               // HTML page fragment tag id
        project_file *file_link = nullptr;
        std::list<TableOfContentsItem> children;
        // TableOfContentsItem *parent = nullptr;

        std::string to_hhc(const std::filesystem::path& temp_path);
        std::string to_hhc_entry(const std::filesystem::path& temp_path);
    };
}