#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <regex>

#include "chm.hpp"
#include "maddy/parser.h"
#include "chm.hpp"



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
            files.push_back(file);
        }
        else if(file.extension() == ".html") {
            files.push_back(file);
        }
    }

    if(!default_file.empty()) {
        for (auto &&f : files) {
            if(f == default_file) {
                default_file_link = &f;
            }
        }
    }

    if(files.size() == 0) {
        return;
    }

    if(!default_file_link) {
        default_file_link = &files[0];
    }
}



void chm::project::convert_source_files() {
    for (auto &&file_path : files) {
        if(file_path.extension() != ".md") {
            continue;
        }

        // md to html parser
        static maddy::Parser parser;
        std::string html_out;

        std::cout << "Converting: " << std::filesystem::relative(file_path) << std::endl;

        {
            std::ifstream md_file(file_path);
            html_out = parser.Parse(md_file);
        }

        scan_html_for_dependencies(html_out);

        // Update the path
        file_path = temp_path / std::filesystem::relative(file_path, root_path).replace_extension(".html");
        std::filesystem::create_directories(std::filesystem::absolute(file_path).remove_filename());

        {
            std::ofstream html_file(file_path);
            // html head body tags are required, chmcmd crashes if they are not present.
            // TODO: Custom html style templates
            html_file << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>";
            html_file << html_out;
            html_file << "</body></html>";
        }
    }

    for (auto &&file_path : files) {
        if(file_path.extension() == ".html") {
            continue;
        }

        std::cout << "Copying: " << std::filesystem::relative(file_path) << std::endl;

        auto new_path = temp_path / std::filesystem::relative(file_path);
        std::filesystem::copy_file(file_path, new_path);

        file_path = new_path;
    }
}



// TODO: Scan generated html files for header tags
void chm::project::create_default_toc() {
    for (auto &&file : files) {
        if(file.extension() != ".html") {
            continue;
        }
        toc.push_back({file.filename().string(), &file, {}});
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
    for (auto &&f : files) {
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
        temp += "<param name=\"Local\" value=\"" + std::filesystem::relative(*item.file_link, temp_path).string() + "\">\n";
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



void chm::project::scan_html_for_dependencies(std::string& html) {
    std::regex img_tag_test("<img *src=\"(.*?)\" *(alt=\"(.*?)\")?\\/>"); // 1 group - image url, 3 group alt text.
    std::regex web_link_test("(http|https)://.*\\..*");

    auto begin = std::sregex_iterator(html.begin(), html.end(), img_tag_test);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string url = (*i)[1];

        bool is_web_link = std::regex_match(url, web_link_test);

        // If local add the file to project.
        if(!is_web_link && std::filesystem::exists(root_path / url)) {
            std::cout << url << std::endl;
            files.push_back(root_path / url);
        }

        // TODO: Download images from the web
    }
}