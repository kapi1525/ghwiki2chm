#include <iostream>
#include <fstream>
#include <regex>
#include <cstdio>
#include <cctype>

#include "chm.hpp"
#include "maddy/parser.h"
#include <curl/curl.h>



void chm::project::create_from_ghwiki(std::filesystem::path default_file) {
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
        if(file.extension() == ".md") {
            source_files.push_back(file);
        }
        else if(file.extension() == ".html") {
            source_files.push_back(file);
        }
    }

    if(!default_file.empty()) {
        for (auto &&f : source_files) {
            if(f == default_file) {
                default_file_link = &f;
                break;
            }
        }
    }

    if(source_files.size() == 0) {
        return;
    }

    if(!default_file_link) {
        default_file_link = &source_files[0];

        for (auto &&f : source_files) {
            if(f.filename() == "Home.md") {
                default_file_link = &f;
                break;
            }
        }
    }
}



void chm::project::convert_source_files() {
    for (const auto& source_file_path : source_files) {
        if(source_file_path.extension() == ".md") {
            // md to html parser
            static maddy::Parser parser;
            std::string html_out;

            std::cout << "Converting: " << std::filesystem::relative(source_file_path) << std::endl;

            {
                std::ifstream md_file(source_file_path);
                html_out = parser.Parse(md_file);
            }

            scan_html_for_local_dependencies(html_out);
            scan_html_for_remote_dependencies(html_out);
            update_html_headings(html_out);

            // Update the path
            auto new_file_path = temp_path / std::filesystem::relative(source_file_path, root_path).replace_extension(".html");
            std::filesystem::create_directories(std::filesystem::absolute(new_file_path).remove_filename());

            {
                std::ofstream html_file(new_file_path);
                // html head body tags are required, chmcmd crashes if they are not present.
                // TODO: Custom html style templates
                html_file << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>";
                html_file << html_out;
                html_file << "</body></html>";
            }

            files_to_compile.push_back(new_file_path);

            bool is_default_file = false;
            if(default_file_link && *default_file_link == source_file_path) {
                default_file_link = &files_to_compile.back();
                is_default_file = true;
            }

            if(auto_toc) {
                auto toc_entry = create_toc_entries(&files_to_compile.back(), html_out);
                if(is_default_file) {
                    // Default file shoud be first in toc
                    toc.push_front(toc_entry);
                } else {
                    toc.push_back(toc_entry);
                }
            }
        }
        else {
            std::cout << "Copying: " << std::filesystem::relative(source_file_path) << std::endl;

            auto new_file_path = temp_path / std::filesystem::relative(source_file_path);
            std::filesystem::copy_file(source_file_path, new_file_path, std::filesystem::copy_options::overwrite_existing);

            files_to_compile.push_back(new_file_path);
        }

    }
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
    for (auto &&f : files_to_compile) {
        file_stream << std::filesystem::relative(f, temp_path).string() << "\n";
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

        // If local add the file to project.
        if(!is_web_link && std::filesystem::exists(root_path / url)) {
            files_to_compile.push_back(root_path / url);
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
            std::cout << "Downloading: \"" << url << "\"" << std::endl;

            std::filesystem::path target_filepath = temp_path / link_match[2].str();
            std::filesystem::create_directories(std::filesystem::absolute(target_filepath).remove_filename());

            FILE* file = std::fopen(target_filepath.c_str(), "wb");

            if(!file) {
                goto defer;
            }
            // std::ofstream file(temp_path / target_filename);
            CURL* curl = curl_easy_init();

            if(!curl) {
                goto defer;
            }

            CURLcode res;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
            // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            res = curl_easy_perform(curl);

            if(res) {
                std::cout << res << std::endl;
            }

            std::fclose(file);

            curl_easy_cleanup(curl);

            std::string temp = "<img src=";
            temp += std::filesystem::relative(target_filepath, temp_path);
            html.replace(i + match.position(), match.length(), temp);

            files_to_compile.push_back(target_filepath);

            i -= match.length();
            i += temp.length();
        }

        defer:
        i += match.position() + match.length();
        temp = html.substr(i);
    }
}


void chm::project::update_html_headings(std::string& html) {
    std::vector<std::regex> heading_tag_tests = {
        std::regex("<(h1)>(.*?)<\\/(h1)>"),
        std::regex("<(h2)>(.*?)<\\/(h2)>"),
        std::regex("<(h3)>(.*?)<\\/(h3)>"),
        std::regex("<(h4)>(.*?)<\\/(h4)>"),
        std::regex("<(h5)>(.*?)<\\/(h5)>"),
        std::regex("<(h6)>(.*?)<\\/(h6)>"),
    };

    for (auto &&regex : heading_tag_tests) {
        while (true) {
            std::smatch match;
            if(!std::regex_search(html, match, regex)) {
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
            new_tag += "</";
            new_tag += match[3];
            new_tag += ">";
            html.replace(match.position(), match.length(), new_tag);
        }

        // html = std::regex_replace(html, regex, "<$1 id=\"\\L$2\">$2</$3>");
    }
}



chm::toc_item chm::project::create_toc_entries(std::filesystem::path* file, const std::string& html) {
    std::regex img_tag_test("<h[1-6] +id=\"(.+?)\">(.+?)</h[1-6]>");

    auto begin = std::sregex_iterator(html.begin(), html.end(), img_tag_test);
    auto end = std::sregex_iterator();

    toc_item toc_entry;

    toc_entry.name = file->filename().replace_extension("");
    toc_entry.file_link = file;

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string heading_id = (*i)[1];
        std::string heading_name = (*i)[2];

        toc_entry.children.push_back({.name = heading_name, .file_link = file, .target_fragment = heading_id});
    }

    if(toc_entry.children.size() == 1) {
        toc_entry.children.clear();
    }

    return toc_entry;
}