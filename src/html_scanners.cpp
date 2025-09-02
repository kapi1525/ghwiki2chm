#include <regex>

#include "project.hpp"



void chm::scan_html_for_local_dependencies(const ProjectConfig &config, ProjectData &data, const std::string& html) {
    std::regex img_tag_test("<img +src=\"(.*?)\" *(alt=\"(.*?)\")?\\/>"); // 1 group - image url, 3 group alt text.
    std::regex web_link_test("(http|https)://.*\\..*");

    auto begin = std::sregex_iterator(html.begin(), html.end(), img_tag_test);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i) {
        std::string url = (*i)[1];

        bool is_web_link = std::regex_match(url, web_link_test);

        auto file_path = config.root / url;

        // If local add the file to project.
        if(!is_web_link && std::filesystem::exists(file_path)) {
            // If already was added to dependencies skip it.
            for (auto &&dep : data.local_dependencies) {
                if(dep.original == file_path) {
                    continue;
                }
            }

            data.local_dependencies.push_back({.original = file_path});
        }
    }
}


// TODO: This is ugly as f...
void chm::scan_html_for_remote_dependencies(const ProjectConfig &config, ProjectData &data, std::string& html) {
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
            for (auto &&dep : data.remote_dependencies) {
                if(dep.link == url) {
                    already_added = true;
                    break;
                }
            }
            if(!already_added) {
                data.remote_dependencies.push_back({.link = url, .target = config.temp / link_match[2].str()});
                // std::cout << "Found dependency: \"" << url << "\"" << std::endl;
            }

            std::string temp = "<img src=";
            temp += std::filesystem::relative(data.remote_dependencies.back().target, config.temp).string();
            html.replace(i + match.position(), match.length(), temp);

            i -= match.length();
            i += temp.length();
        }

        i += match.position() + match.length();
        temp = html.substr(i);
    }
}