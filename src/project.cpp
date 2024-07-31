#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

// POSIX specific
#include <unistd.h>
#include <sys/wait.h>

#include "project.hpp"
#include "maddy/parser.h"
#include "chm.hpp"



void project::create_from_ghwiki(std::filesystem::path default_file) {
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



void project::convert_source_files() {
    for (auto &&file_path : files) {
        if(file_path.extension() == ".md") {
            // md to html parser
            static maddy::Parser parser;

            std::cout << "Converting: " << std::filesystem::relative(file_path) << " ---> ";
            std::cout.flush();

            // Open md file for reading.
            std::fstream file_stream(file_path, std::ios_base::in);
            std::string html_out = parser.Parse(file_stream);

            file_stream.close();

            file_path = temp_path / std::filesystem::relative(file_path, root_path).replace_extension(".html");
            std::filesystem::create_directories(std::filesystem::absolute(file_path).remove_filename());

            file_stream.open(file_path, std::ios_base::out);
            // html head body tags are required, chmcmd crashes if they are not present.
            // TODO: Custom html style templates
            file_stream << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"></head><body>\n";
            file_stream << html_out;
            file_stream << "\n</body></html>";
            file_stream.close();

            std::cout << std::filesystem::relative(file_path) << std::endl;
        }
        else {
            std::cout << "Copying: " << std::filesystem::relative(file_path) << " ---> ";
            std::cout.flush();

            auto new_path = temp_path / std::filesystem::relative(file_path);
            std::filesystem::copy_file(file_path, new_path);

            file_path = new_path;
            std::cout << file_path.filename() << std::endl;
        }
    }
}



// TODO: Scan generated html files for header tags
void project::create_default_toc() {
    for (auto &&file : files) {
        toc.push_back({file.filename().string(), &file, {}});
    }
}



void project::create_project_files() {
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



std::string project::to_hhc(toc_item& item) {
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



void project::compile_project() {
    pid_t pid = fork();
    if(pid == 0) {
        // Child process
        std::filesystem::current_path(temp_path);
        // --no-html-scan - assume maddy produces correct html
        if(execlp("chmcmd", "chmcmd", "proj.hhp", "--no-html-scan", 0) == -1) {
            perror("execlp failed");
        }
    } else if(pid > 0) {
        int status;
        if(waitpid(pid, &status, 0) != pid) {
            perror("waitpid() failed");
        }
    } else {
        perror("fork() failed");
    }
}
