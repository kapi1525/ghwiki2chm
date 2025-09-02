#pragma once

#include <deque>
#include <filesystem>
#include <string>

#include <RUtils/ErrorOr.hpp>

#include "project_file.hpp"
#include "remote_dependency.hpp"
#include "table_of_contents.hpp"



namespace chm {
    struct ProjectConfig {
        std::string title = "Untitled";
        std::filesystem::path root, temp, out_file;

        std::string toc_root_item_name;

        bool toc_create_items_for_sections = true;
        bool toc_no_section_links = false;
    };

    struct ProjectData {
        TableOfContentsItem toc_root;
        ProjectFile* default_file_link = nullptr;
        std::deque<ProjectFile> files;                      // Project files, that may be converted and are pages.
        std::deque<ProjectFile> local_dependencies;         // Other files like images, required by project pages
        std::deque<RemoteDependency> remote_dependencies;   // Other files like images, but needed to be downloaded.
    };

    // Search for compatible files in root path, create ProjectData from them.
    RUtils::ErrorOr<ProjectData> create_project_data_from_ghwiki(ProjectConfig &config, std::filesystem::path default_file);

    // Run converters for project files
    void convert_project_files(const ProjectConfig &config, ProjectData &data, std::uint32_t max_jobs);


    // Looks for local dependencies like images and includes them into the project
    void scan_html_for_local_dependencies(const ProjectConfig &config, ProjectData &data, const std::string &html);
    // Same as above but looks for remote images that should be downloaded and updates the url to point to a local file
    void scan_html_for_remote_dependencies(const ProjectConfig &config, ProjectData &data, std::string &html);

    // Download remote images that are used in the project
    void download_dependencies(const ProjectConfig &config, ProjectData &data, std::size_t max_downloads, bool ignore_ssl, bool verbose);
    // Create .hhc .hhp
    void generate_project_files(const ProjectConfig &config, const ProjectData &data);



    void update_html_headings_to_include_id(std::string &html);
    void update_html_links_to_pages(const ProjectConfig &config, ProjectData &data, std::string &html);
    void update_html_remote_links_to_open_in_new_broser_window(const ProjectData &data, std::string &html);

    ProjectFile* find_local_file_pointed_by_url(const ProjectConfig &config, ProjectData &data, const std::string &url);


    TableOfContentsItem create_toc_entries_from_sidebar(const ProjectConfig &config, ProjectData &data, std::filesystem::path sidebar_path);
    TableOfContentsItem create_toc_entries(const ProjectConfig &config, ProjectFile* file, const std::string& html);  // Create toc entry by looking for heading tags in generated html
}