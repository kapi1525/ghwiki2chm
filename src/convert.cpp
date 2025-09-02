#include <format>
#include <fstream>

#include <RUtils/ForEach.hpp>

#include "project.hpp"
#include "helpers.hpp"

using namespace RUtils;



void chm::convert_project_files(const ProjectConfig &config, ProjectData &data, std::uint32_t max_jobs) {
    // Determine converter
    for (auto &&file : data.files) {
        if(file.original.extension() == ".md") {
            file.converter = ConversionType::from_markdown;
            continue;
        }

        file.converter = ConversionType::copy;
    }


    // Determine target file path
    for (auto &&file : data.files) {
        switch (file.converter) {
        case ConversionType::copy:
            file.target = config.temp / std::filesystem::relative(file.original, config.root);
            continue;

        case ConversionType::from_markdown:
            file.target = config.temp / std::filesystem::relative(file.original, config.root).replace_extension(".html");
            continue;

        default:
            RUtils::Error::unreachable();
        }
    }


    // Copy or convert files
    RUtils::for_each_threaded(data.files.begin(), data.files.end(), [&](auto& file) {
        std::filesystem::create_directories(std::filesystem::absolute(file.target).remove_filename());
        std::printf("%s\n", std::filesystem::relative(file.original, config.root).string().c_str());

        switch (file.converter) {
        case ConversionType::copy:
            std::filesystem::copy_file(file.original, file.target, std::filesystem::copy_options::overwrite_existing);
            return;

        case ConversionType::from_markdown: {
            std::string html_out = convert_markdown_file_to_html(file.original);

            scan_html_for_local_dependencies(config, data, html_out);
            scan_html_for_remote_dependencies(config, data, html_out);
            update_html_headings_to_include_id(html_out);
            update_html_remote_links_to_open_in_new_broser_window(data, html_out);
            update_html_links_to_pages(config, data, html_out);

            {
                std::ofstream html_file(file.target);
                // html head body tags are required, chmcmd crashes if they are not present.
                // TODO: Custom html style templates
                html_file << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>";
                html_file << html_out;
                html_file << "</body></html>";
            }

            bool is_default_file = false;
            if (data.default_file_link && data.default_file_link == &file) {
                // data.default_file_link = &file.target;
                is_default_file = true;
            }

            if (config.toc_create_items_for_sections) {
                auto toc_entry = create_toc_entries(config, &file, html_out);

                auto* toc_append_root = &data.toc_root;

                if (!config.toc_root_item_name.empty()) {
                    toc_append_root = &data.toc_root.children.front();

                    if (is_default_file) {
                        toc_append_root->file_link = toc_entry.file_link;
                    }
                }

                if (is_default_file) {
                    // Default file shoud be first in toc
                    toc_append_root->children.push_front(toc_entry);
                } else {
                    toc_append_root->children.push_back(toc_entry);
                }
            }

            return; }

        default:
            RUtils::Error::unreachable();
        }
    }, max_jobs);
}