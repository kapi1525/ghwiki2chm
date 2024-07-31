#pragma once

#include <string>
#include <filesystem>
#include <deque>
#include <vector>
#include <memory>



struct toc_item {
    std::string name;
    std::filesystem::path* file_link = nullptr;
    std::vector<toc_item> children;
};

using table_of_contents = std::vector<toc_item>;


class project {
public:
    project() = default;
    ~project() = default;
    
    std::filesystem::path root_path;
    std::filesystem::path temp_path;
    std::filesystem::path out_file;
    std::filesystem::path* default_file_link = nullptr;
    std::string title = "test";
    std::deque<std::filesystem::path> files;
    table_of_contents toc;

    // create a project config automaticaly from md files and _Sidebar.
    void create_from_ghwiki(std::filesystem::path default_file);

    void convert_source_files();    // Copy or covert project source files to temp dir
    // void scan_html();               // Scan generated html files for dependencies (images, js files) and include them into the project
    void create_default_toc();      // Create toc by looking for header tags in generated html
    void create_project_files();    // Create .hhc .hhp
    void compile_project();         // Run chmcmd after compiling

private:
    std::string to_hhc(toc_item& item);
};
