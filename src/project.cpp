#include <cstdio>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>
#include <format>

#include "RUtils/Link.hpp"      // TODO: Replace with curl_url
#include "RUtils/ForEach.hpp"
#include "RUtils/Defer.hpp"

#include "maddy/parser.h"

#define NOMINMAX // Maybe a bug in curl.wrap: on windows min max macros are added and collide with std::min/std::max
                 // why microsoft didn't make this the default already??? no one uses those.
#include "curl/curl.h"

#include "chm.hpp"
#include "helpers.hpp"



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

    if (!toc_root_item_name.empty()) {
        toc.push_back({.name = toc_root_item_name, .file_link = nullptr});
    }

    return true;
}



void chm::project::convert_source_files(std::uint32_t max_jobs) {
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

        RUtils::Error::unreachable();
    }

    // Copy or convert files
    RUtils::for_each_threaded(files.begin(), files.end(), [&](auto& file) {
        std::filesystem::create_directories(std::filesystem::absolute(file.target).remove_filename());
        std::printf("%s\n", std::filesystem::relative(file.original, root_path).string().c_str());

        switch (file.converter) {
        case conversion_type::copy:
            std::filesystem::copy_file(file.original, file.target, std::filesystem::copy_options::overwrite_existing);
            return;

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
            update_html_remote_links_to_open_in_new_broser_window(html_out);

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

                auto* toc_root = &toc;

                if (!toc_root_item_name.empty()) {
                    toc_root = &toc[0].children;

                    if (is_default_file) {
                        toc[0].file_link = toc_entry.file_link;
                    }
                }

                if (is_default_file) {
                    // Default file shoud be first in toc
                    toc_root->push_front(toc_entry);
                } else {
                    toc_root->push_back(toc_entry);
                }
            }

            return; }
        }

        RUtils::Error::unreachable();
    }, max_jobs);
}



// for curl
static size_t write_callback(char* ptr, size_t size, size_t nmemb, std::ofstream* file) {
    size_t bytes_to_write = size * nmemb;

    file->write(ptr, bytes_to_write);

    return bytes_to_write;
}

// Download remote dependencies
void chm::project::download_dependencies(size_t max_downloads, bool ignore_ssl, bool verbose) {
    // Nothing to download.
    if(remote_dependencies.size() == 0) {
        return;
    }

    std::printf("Will download %zu remote dependencies...\n", remote_dependencies.size());


    max_downloads = std::min(max_downloads, remote_dependencies.size());
    size_t next_dep_to_download_index = 0;  // index to remote_dependencies[]

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURLM* multi_handle = curl_multi_init();


    struct downloader_state {
        CURL* handle;
        remote_dependency* dep_ptr; // assigned file or nullptr
        std::ofstream* file_ptr;    // target file output stream or nullptr
    };

    std::vector<downloader_state> downloaders(max_downloads);

    for (auto& download : downloaders) {
        CURL* handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, !ignore_ssl);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, !ignore_ssl);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, verbose);
        download.handle = handle;
    }


    int running_handles = 0;
    size_t downloaded_count = 0;

    do {
        if((size_t)running_handles < max_downloads && next_dep_to_download_index < remote_dependencies.size()) {
            for (auto& download : downloaders) {
                if (download.dep_ptr == nullptr && download.file_ptr == nullptr) {
                    download.dep_ptr = &remote_dependencies[next_dep_to_download_index];
                    next_dep_to_download_index++;

                    std::filesystem::create_directories(std::filesystem::absolute(download.dep_ptr->target).remove_filename());
                    download.file_ptr = new std::ofstream(download.dep_ptr->target);

                    curl_easy_setopt(download.handle, CURLOPT_URL, download.dep_ptr->link.c_str());
                    curl_easy_setopt(download.handle, CURLOPT_WRITEDATA, download.file_ptr);
                    curl_multi_add_handle(multi_handle, download.handle);
                }

                if(next_dep_to_download_index >= remote_dependencies.size()) {
                    break;
                }
            }
        }

        curl_multi_perform(multi_handle, &running_handles);

        int msgs_in_queue = 0;
        while (CURLMsg* msg = curl_multi_info_read(multi_handle, &msgs_in_queue)) {
            if(msg->msg == CURLMSG_DONE) {
                for (auto& download : downloaders) {
                    if(download.handle == msg->easy_handle) {
                        curl_multi_remove_handle(multi_handle, download.handle);
                        std::printf("%s\n", download.dep_ptr->link.c_str());
                        download.dep_ptr = nullptr;
                        delete download.file_ptr;
                        download.file_ptr = nullptr;
                        downloaded_count++;
                        break;
                    }
                }
            }
        };

        int fds;
        curl_multi_poll(multi_handle, nullptr, 0, 1000, &fds);
    } while (running_handles || next_dep_to_download_index < remote_dependencies.size());


    for (auto& download : downloaders) {
        curl_easy_cleanup(download.handle);
        download.handle = nullptr;
    }

    curl_multi_cleanup(multi_handle);

    std::printf("%zu/%zu.\n", downloaded_count, remote_dependencies.size());
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


// If url has host add `target="_blank"`
void chm::project::update_html_remote_links_to_open_in_new_broser_window(std::string& html) {
    std::regex link_tag_test("(<a +href=\")(.*?)(\")(>)");

    std::match_results<std::string_view::const_iterator> match;
    for (size_t i = 0; i < html.length(); i += match.position() + match.length()) {
        std::string_view html_sv(html);
        html_sv = html_sv.substr(i);

        if(!std::regex_search(html_sv.begin(), html_sv.end(), match, link_tag_test)) {
            break;
        }

        std::string url_str = match[2];

        CURLU* url_handle = curl_url();
        RUtils::Defer( curl_url_cleanup(url_handle); );

        if (CURLUcode err = curl_url_set(url_handle, CURLUPART_URL, url_str.c_str(), CURLU_DEFAULT_SCHEME | CURLU_NO_AUTHORITY | CURLU_ALLOW_SPACE); err != CURLUE_OK) {
            std::puts(std::format("Failed to parse link: \"{}\": {}.", url_str, curl_url_strerror(err)).c_str());
            continue;
        }

        char* url_host;
        if (CURLUcode err = curl_url_get(url_handle, CURLUPART_HOST, &url_host, 0)) {
            RUtils::Error(curl_url_strerror(err), RUtils::ErrorType::library).print();
            continue;
        }

        RUtils::Defer( curl_free(url_host); );

        size_t url_host_sz = std::strlen(url_host);

        if (url_host_sz == 0) {
            continue;
        }

        constexpr std::string_view to_insert = " target=\"_blank\"";
        html.insert(i + match.position(4), to_insert);
        i += to_insert.size();
    }
}



chm::toc_item chm::project::create_toc_entries(std::filesystem::path* file, const std::string& html) {
    toc_item toc_entry;

    toc_entry.name = file->filename().replace_extension("").string();
    toc_entry.file_link = file;

    // replace dashes with spaces
    // github wiki web editor puts them in the file names
    for (auto& c : toc_entry.name) {
        if (c == '-') {
            c = ' ';
        }
    }

    if(toc_no_section_links) {
        return toc_entry;
    }

    std::regex heading_tag_test("<h[1-6] +id=\"(.+?)\">(.+?)</h[1-6]>");

    auto begin = std::sregex_iterator(html.begin(), html.end(), heading_tag_test);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string heading_id = (*i)[1];
        std::string heading_name = (*i)[2];

        heading_name = trim_whitespace(remove_html_tags(heading_name));

        if(heading_name.empty()) {
            continue;
        }

        toc_entry.children.push_back({heading_name, file, heading_id});
    }

    if(toc_entry.children.size() == 1) {
        toc_entry.children.clear();
    }

    return toc_entry;
}