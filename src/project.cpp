#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <cstdio>
#include <cctype>

#include "RUtils/Helpers.hpp"
#include "RUtils/Request.hpp"
#include "RUtils/Link.hpp"

#include "maddy/parser.h"

#include "chm.hpp"



bool chm::project::create_from_ghwiki(std::filesystem::path default_file) {
    if(!std::filesystem::exists(root_path)) {
        return false;
    }

    for (auto &&dir_entry : std::filesystem::recursive_directory_iterator(root_path)) {
        // Skip all non-file entries
        if(!dir_entry.is_regular_file()) {
            continue;
        }

        auto file = dir_entry.path();

        // Skip files inside temp directory
        if(file.compare(temp_path) > 0) {
            continue;
        }

        // Add compatible file types to project
        // TODO: Refactor
        if(file.extension() == ".md") {
            files.push_back({file});
        }
        else if(file.extension() == ".html") {
            files.push_back({file});
        }
    }

    if(!default_file.empty()) {
        for (auto &&file : files) {
            if(file.original == default_file) {
                default_file_link = &file.original;
                break;
            }
        }
    }

    if(files.size() == 0) {
        return false;
    }

    if(!default_file_link) {
        default_file_link = &files[0].original;

        for (auto &&file : files) {
            if(file.original.filename() == "Home.md") {
                default_file_link = &file.original;
                break;
            }
        }
    }

    return true;
}



void chm::project::convert_source_files() {
    // Determine converter
    for (auto &&file : files) {
        if(file.original.extension() == ".md") {
            file.converter = conversion_type::markdown;
            continue;
        }

        file.converter = conversion_type::copy;
    }

    // Determine target file path
    for (auto &&file : files) {
        switch (file.converter) {
        case conversion_type::copy:
            file.target = temp_path / std::filesystem::relative(file.original, root_path);
            continue;

        case conversion_type::markdown:
            file.target = temp_path / std::filesystem::relative(file.original, root_path).replace_extension(".html");
            continue;
        }

        RUtils::unreachable();
    }

    // Copy or convert files
    for (auto &&file : files) {
        std::filesystem::create_directories(std::filesystem::absolute(file.target).remove_filename());
        std::printf("%s\n", std::filesystem::relative(file.original, root_path).string().c_str());

        switch (file.converter) {
        case conversion_type::copy:
            std::filesystem::copy_file(file.original, file.target, std::filesystem::copy_options::overwrite_existing);
            continue;

        case conversion_type::markdown: {
            // md to html parser
            static maddy::Parser parser;
            std::string html_out;

            {
                std::ifstream md_file(file.original);
                html_out = parser.Parse(md_file);
            }

            scan_html_for_local_dependencies(html_out);
            scan_html_for_remote_dependencies(html_out);
            update_html_headings(html_out);
            update_html_links(html_out);

            {
                std::ofstream html_file(file.target);
                // html head body tags are required, chmcmd crashes if they are not present.
                // TODO: Custom html style templates
                html_file << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>";
                html_file << html_out;
                html_file << "</body></html>";
            }

            bool is_default_file = false;
            if(default_file_link && *default_file_link == file.original) {
                default_file_link = &file.target;
                is_default_file = true;
            }

            if(auto_toc) {
                auto toc_entry = create_toc_entries(&file.target, html_out);
                if(is_default_file) {
                    // Default file shoud be first in toc
                    toc.push_front(toc_entry);
                } else {
                    toc.push_back(toc_entry);
                }
            }

            continue; }
        }

        RUtils::unreachable();
    }
}



// Download remote dependencies
void chm::project::download_dependencies() {
    if(remote_dependencies.size() == 0) {
        return;
    }

    std::mutex dependencies_deque_mutex;
    std::vector<std::thread> download_threads;

    std::printf("Will download %zu remote dependencies...\n", remote_dependencies.size());
    for (std::uint32_t i = 0; i < std::thread::hardware_concurrency(); i++) {
        download_threads.emplace_back(&chm::project::download_depdendencies_thread, this, std::ref(dependencies_deque_mutex));
    }

    for (auto &&thread : download_threads) {
        thread.join();
    }

    size_t downloaded_count = 0;
    for (auto &&dependency : remote_dependencies) {
        if(dependency.state == download_state::downloaded) {
            downloaded_count++;
        }
    }

    std::printf("%zu/%zu.\n", downloaded_count, remote_dependencies.size());
}



// Call curl_global_init before!
void chm::project::download_depdendencies_thread(std::mutex& dependencies_deque_mutex) {
    for (auto &&dep : remote_dependencies) {
        {
            std::lock_guard<std::mutex> lock(dependencies_deque_mutex);
            if(dep.state != download_state::none) {
                continue;
            }
            dep.state = download_state::downloading;
        }
        if(download_file(dep.link, dep.target)) {
            std::lock_guard<std::mutex> lock(dependencies_deque_mutex);
            std::printf("%s\n", dep.link.c_str());
            dep.state = download_state::downloaded;
        } else {
            std::lock_guard<std::mutex> lock(dependencies_deque_mutex);
            std::printf("Failed to download: \"%s\"\n", dep.link.c_str());
            dep.state = download_state::download_failed;
        }
    }
}



bool chm::project::download_file(const std::string& link, std::filesystem::path target) {
    std::filesystem::create_directories(std::filesystem::absolute(target).remove_filename());

    RUtils::Request::Result request;
    if(auto ret = RUtils::Request::get(link); ret) {
        request = ret;
    } else {
        ret.print();
        return false;
    }

    if(request.http_code != 200) {
        return false;
    }

    if(auto result = request.to_file(target); !result) {
        result.print();
        return false;
    }

    return true;
}



void chm::project::generate_project_files() {
    std::ofstream file_stream;

    // .gitihnore
    file_stream.open(temp_path / ".gitignore");
    file_stream << "*";
    file_stream.close();


    // HTML Help Project .hhp
    file_stream.open(temp_path / "proj.hhp");

    // Configured with recomended settings https://www.nongnu.org/chmspec/latest/INI.html#HHP
    file_stream << "[OPTIONS]\n";
    file_stream << "Auto Index=Yes\n";
    // file_stream << "Auto TOC=Yes\n";
    file_stream << "Binary Index=Yes\n";
    file_stream << "Binary TOC=Yes\n";
    file_stream << "Compatibility=1.1 or later\n";
    file_stream << "Compiled file=" << std::filesystem::relative(out_file, temp_path).string() << "\n";
    file_stream << "Contents file=proj.hhc\n";
    file_stream << "Default Window=main\n";

    file_stream << "Flat=No\n";
    file_stream << "Full-text search=Yes\n";
    file_stream << "Title=test\n";

    file_stream << "[WINDOWS]\n";

    auto default_file = std::filesystem::relative(*default_file_link, temp_path);

    // NOTE: Switching styles sometimes might not work because https://shouldiblamecaching.com/
    // just why?????
    file_stream << "main=";                                                     // Window type
    file_stream << "\"" << title << "\",";                                      // Title bar text
    file_stream << "\"" << "proj.hhc" << "\",";                                 // Table of contents .hhc file
    file_stream << ",";                                                         // Index .hhk file
    file_stream << default_file << ",";                                         // Default html file
    file_stream << default_file << ",";                                         // File shown when home button was pressed
    file_stream << ",,";                                                        // Jump1 button file to open and text
    file_stream << ",,";                                                        // Jump2 button file to open and text
    file_stream << "0x" << std::hex << chm::default_style << std::dec << ",";   // Navigation pane style bitfield
    file_stream << ",";                                                         // Navigation pane width in pixels
    file_stream << "0x" << std::hex << chm::default_buttons << std::dec << ","; // Buttons to show bitfield
    file_stream << "[,,,],";                                                    // Initial position [left, top, right, bottom]
    file_stream << "0xB0000,";                                                  // why? Window style (https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles)
    file_stream << ",";                                                         // Extended window style...
    file_stream << ",";                                                         // Window show state (https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)
    file_stream << ",";                                                         // Navigation pane shoud be closed? 0/1
    file_stream << ",";                                                         // Default navigation pane tab 0 = Table of contents
    file_stream << ",";                                                         // Navigation pane tabs on the top
    file_stream << "0\n";                                                       // idk

    file_stream << "[FILES]\n";
    for (auto &&file : files) {
        file_stream << std::filesystem::relative(file.target, temp_path).string() << "\n";
    }

    for (auto &&file : local_dependencies) {
        file_stream << std::filesystem::relative(file.target, temp_path).string() << "\n";
    }

    for (auto &&file : remote_dependencies) {
        file_stream << std::filesystem::relative(file.target, temp_path).string() << "\n";
    }

    file_stream.close();


    // HTML Help table of Contents .hhc
    file_stream.open(temp_path / "proj.hhc");

    file_stream << "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n";
    file_stream << "<HTML>\n";
    file_stream << "<HEAD>\n";
    file_stream << "<meta name=\"GENERATOR\"content=\"ghwiki2chm test\">\n";
    file_stream << "<!-- Sitemap 1.0 -->\n";
    file_stream << "</HEAD>\n";

    file_stream << "<BODY>\n";

    file_stream << "<OBJECT type=\"text/site properties\">\n";
    // file_stream << "<param name=\"Property Name\" value=\"Property Value\">\n";
    file_stream << "</OBJECT>\n";

    file_stream << "<UL>\n";
    for (auto &&i : toc) {
        file_stream << to_hhc(i);
    }
    file_stream << "</UL>\n";

    file_stream << "</BODY>\n";
    file_stream << "</HTML>\n";

    file_stream.close();
}



std::string chm::project::to_hhc(toc_item& item) {
    std::string temp = "";

    temp += "<LI> <OBJECT type=\"text/sitemap\">\n";
    temp += "<param name=\"Name\" value=\"" + item.name + "\">\n";

    if(item.file_link) {
        std::string link = std::filesystem::relative(*item.file_link, temp_path).string();
        if(!item.target_fragment.empty()) {
            link += "#" + item.target_fragment;
        }
        temp += "<param name=\"Local\" value=\"" + link + "\">\n";
    }

    temp += "</OBJECT>\n";

    if(item.children.size() == 0) {
        return temp;
    }

    temp += "<UL>\n";

    for (auto &&i : item.children) {
        temp += to_hhc(i);
    }

    temp += "</UL>\n";

    return temp;
}



void chm::project::scan_html_for_local_dependencies(const std::string& html) {
    std::regex img_tag_test("<img +src=\"(.*?)\" *(alt=\"(.*?)\")?\\/>"); // 1 group - image url, 3 group alt text.
    std::regex web_link_test("(http|https)://.*\\..*");

    auto begin = std::sregex_iterator(html.begin(), html.end(), img_tag_test);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string url = (*i)[1];

        bool is_web_link = std::regex_match(url, web_link_test);

        auto file_path = root_path / url;

        // If local add the file to project.
        if(!is_web_link && std::filesystem::exists(file_path)) {
            // If already was added to dependencies skip it.
            for (auto &&dep : local_dependencies) {
                if(dep.original == file_path) {
                    return;
                }
            }

            local_dependencies.push_back({.original = file_path});
        }
    }
}


// FIXME: This is ugly as f...
void chm::project::scan_html_for_remote_dependencies(std::string& html) {
    std::regex img_tag_test("<img +src=\"(.*?)\"");
    std::regex web_link_test("(http|https)://.*\\..*?/(.+\\..+)");

    std::smatch match;
    std::size_t i = 0;
    std::string temp = html;

    while (std::regex_search(temp, match, img_tag_test)) {
        std::string url = match[1];

        std::smatch link_match;
        if(std::regex_match(url, link_match, web_link_test)) {
            bool already_added = false;
            for (auto &&dep : remote_dependencies) {
                if(dep.link == url) {
                    already_added = true;
                    break;
                }
            }
            if(!already_added) {
                remote_dependencies.push_back({.link = url, .target = temp_path / link_match[2].str()});
                // std::cout << "Found dependency: \"" << url << "\"" << std::endl;
            }

            std::string temp = "<img src=";
            temp += std::filesystem::relative(remote_dependencies.back().target, temp_path).string();
            html.replace(i + match.position(), match.length(), temp);

            i -= match.length();
            i += temp.length();
        }

        i += match.position() + match.length();
        temp = html.substr(i);
    }
}


void chm::project::update_html_headings(std::string& html) {
    std::regex heading_tag_test("<(h[1-6])>(.*?)(<\\/h[1-6]>)");

    while (true) {
        std::smatch match;
        if(!std::regex_search(html, match, heading_tag_test)) {
            break;
        }

        std::string new_tag = "<";
        new_tag += match[1];
        new_tag += " id=\"";

        for (auto c : match[2].str()) {
            if(std::isspace(c)) {
                new_tag += '-';
            }
            if(std::isalnum(c)) {
                new_tag += std::tolower(c);
            }
        }

        new_tag += "\">";
        new_tag += match[2];
        new_tag += match[3];
        html.replace(match.position(), match.length(), new_tag);
    }

    // html = std::regex_replace(html, regex, "<$1 id=\"\\L$2\">$2</$3>");
}



// Update urls
// - If file path points to one of the files in this->files.original, replace path to this->files.target
// - If file doesnt have an extension look for simmilar file path/name in this0>files.original and replace it with this->files.target
void chm::project::update_html_links(std::string& html) {
    std::regex link_tag_test("(<a +href=\")(.*?)(\">)");

    std::match_results<std::string_view::const_iterator> match;
    for (size_t i = 0; i < html.length(); i += match.position() + match.length()) {
        std::string_view html_sv(html);
        html_sv = html_sv.substr(i);

        if(!std::regex_search(html_sv.begin(), html_sv.end(), match, link_tag_test)) {
            break;
        }

        std::string url_str = match[2];
        auto url_parsed = Link::parse(url_str).value();

        // Parse the link and check if link is to a local resource, if not skip it.
        if(!url_parsed.host.empty() || url_parsed.resource_path.empty()) {
            continue;
        }


        bool handled = false;

        for (auto &&file : files) {
            // links to files
            if(std::filesystem::relative(url_parsed.resource_path, root_path) == std::filesystem::relative(file.original, root_path)) {
                url_parsed.resource_path = std::filesystem::relative(file.target, temp_path).string();
                handled = true;
                break;
            }

            // github wiki page links
            std::string link_target;

            for (auto c : std::filesystem::relative(file.original, root_path).replace_extension("").string()) {
                if(std::isspace(c)) {
                    link_target += '-';
                }
                if(std::isalnum(c)) {
                    link_target += std::tolower(c);
                }
                if(c == '/') {
                    link_target += c;
                }
            }

            if(url_parsed.resource_path == link_target) {
                url_parsed.resource_path = std::filesystem::relative(file.target, temp_path).string();
                handled = true;
                break;
            }
        }

        if(handled) {
            std::string new_link_tag = match[1];
            new_link_tag += url_parsed.to_string();
            new_link_tag += match[3];

            html.replace(i + match.position(), match.length(), new_link_tag);

            i += new_link_tag.length() - match.size();
        } else {
            std::printf("  Unknown link: \"%s\", it will be broken inside the compiled .chm file.\n", url_str.c_str());
        }
    }
}



chm::toc_item chm::project::create_toc_entries(std::filesystem::path* file, const std::string& html) {
    std::regex heading_tag_test("<h[1-6] +id=\"(.+?)\">(.+?)</h[1-6]>");

    auto begin = std::sregex_iterator(html.begin(), html.end(), heading_tag_test);
    auto end = std::sregex_iterator();

    toc_item toc_entry;

    toc_entry.name = file->filename().replace_extension("").string();
    toc_entry.file_link = file;

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string heading_id = (*i)[1];
        std::string heading_name = (*i)[2];

        toc_entry.children.push_back({heading_name, file, heading_id});
    }

    if(toc_entry.children.size() == 1) {
        toc_entry.children.clear();
    }

    return toc_entry;
}