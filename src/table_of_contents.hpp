#pragma once

#include <filesystem>
#include <list>
#include <string>



namespace chm {
    struct ProjectFile;

    struct TableOfContentsItem {
        std::string name;                                   // Name displayed in TOC tree
        std::string fragment;                               // HTML page fragment tag id
        ProjectFile *file_link = nullptr;
        std::list<TableOfContentsItem> children;
        // TableOfContentsItem *parent = nullptr;

        std::string to_hhc(const std::filesystem::path& temp_path) const;
        std::string to_hhc_entry(const std::filesystem::path& temp_path) const;
    };
}