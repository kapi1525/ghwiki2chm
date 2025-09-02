#include <regex>

#include "project.hpp"
#include "helpers.hpp"



// TODO: This is ugly
chm::TableOfContentsItem chm::create_toc_entries_from_sidebar(const ProjectConfig &config, ProjectData &data, std::filesystem::path sidebar_path) {
    std::string html_out = convert_markdown_file_to_html(sidebar_path);

    std::string_view tag_name_and_attribs;
    std::string_view tag_contents;

    std::string::iterator tag_open_index = html_out.begin();
    std::string::iterator tag_contents_begin_index = html_out.begin();

    // tokenizer state
    bool inside_tag = false;

    // toc item creation state
    bool inside_item_name = false;
    bool was_added = false;

    std::regex link_tag_test("\\s*a\\s+href=\"(.*?)\"\\s*");
    std::match_results<std::string_view::const_iterator> match;


    TableOfContentsItem temp_toc_root;
    TableOfContentsItem temp_toc_item;

    std::vector<TableOfContentsItem*> toc_tree_ptrs;
    toc_tree_ptrs.push_back(&temp_toc_root);

    for (std::string::iterator it = html_out.begin(); it != html_out.end(); it++) {
        char &c = *it;

        if (c == '<' && !inside_tag) {
            inside_tag = true;
            tag_open_index = it + 1;

            tag_contents = std::string_view(tag_contents_begin_index, it);
        }
        else if (c == '>' && inside_tag) {
            inside_tag = false;
            tag_name_and_attribs = std::string_view(tag_open_index, it);

            tag_contents_begin_index = it + 1;


            if(inside_item_name) {
                temp_toc_item.name += tag_contents;
            }

            if (tag_name_and_attribs == "li") {
                inside_item_name = true;
                temp_toc_item = {};
                was_added = false;
            }
            else if (tag_name_and_attribs == "/li") {
                inside_item_name = false;
                if (!was_added) {
                    temp_toc_item.name = trim_whitespace(remove_hashes(temp_toc_item.name));
                    toc_tree_ptrs.back()->children.push_back(temp_toc_item);
                    was_added = true;
                }
            }
            else if (tag_name_and_attribs == "ul") {
                inside_item_name = false;
                if (!was_added && !temp_toc_item.name.empty() && toc_tree_ptrs.size() > 0) {
                    temp_toc_item.name = trim_whitespace(remove_hashes(temp_toc_item.name));
                    toc_tree_ptrs.back()->children.push_back(temp_toc_item);
                    toc_tree_ptrs.push_back(&toc_tree_ptrs.back()->children.back());
                    was_added = true;
                }
            }
            else if (tag_name_and_attribs == "/ul") {
                if (toc_tree_ptrs.size() > 0) {
                    toc_tree_ptrs.pop_back();
                }
            }
            else if (inside_item_name && std::regex_match(tag_name_and_attribs.begin(), tag_name_and_attribs.end(), match, link_tag_test)) {
                temp_toc_item.file_link = find_local_file_pointed_by_url(config, data, match[1]);
            }
        }
    }


    return temp_toc_root;
}



chm::TableOfContentsItem chm::create_toc_entries(const ProjectConfig &config, ProjectFile* file, const std::string& html) {
    TableOfContentsItem toc_entry;

    toc_entry.name = file->target.filename().replace_extension("").string();
    toc_entry.file_link = file;

    // replace dashes with spaces
    // github wiki web editor puts them in the file names
    for (auto& c : toc_entry.name) {
        if (c == '-') {
            c = ' ';
        }
    }

    if(config.toc_no_section_links) {
        return toc_entry;
    }

    std::regex heading_tag_test("<h[1-6] +id=\"(.+?)\">(.+?)</h[1-6]>");

    auto begin = std::sregex_iterator(html.begin(), html.end(), heading_tag_test);
    auto end = std::sregex_iterator();

    std::size_t found_fragments = 0;

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string heading_id = (*i)[1];
        std::string heading_name = (*i)[2];

        heading_name = trim_whitespace(remove_html_tags(heading_name));

        if(heading_name.empty()) {
            continue;
        }

        toc_entry.children.push_back({.name = heading_name, .fragment = heading_id, .file_link = file});

        found_fragments++;
    }

    if(found_fragments == 1) {
        toc_entry.children.clear();
    }

    return toc_entry;
}