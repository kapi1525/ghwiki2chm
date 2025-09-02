#include <cstring>
#include <regex>
#include <format>

#include <RUtils/Error.hpp>
#include <RUtils/Defer.hpp>

#define NOMINMAX // Maybe a bug in curl.wrap: on windows min max macros are added and collide with std::min/std::max
                 // why microsoft didn't make this the default already??? no one uses those.
#include "curl/curl.h"

#include "project.hpp"



void chm::update_html_headings_to_include_id(std::string &html) {
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
void chm::update_html_links_to_pages(const ProjectConfig &config, ProjectData &data, std::string &html) {
    std::regex link_tag_test("(<a +href=\")(.*?)(\">)");

    std::match_results<std::string_view::const_iterator> match;
    for (size_t i = 0; i < html.length(); i += match.position() + match.length()) {
        std::string_view html_sv(html);
        html_sv = html_sv.substr(i);

        if(!std::regex_search(html_sv.begin(), html_sv.end(), match, link_tag_test)) {
            break;
        }

        std::string url_str = match[2];
        ProjectFile* url_target = find_local_file_pointed_by_url(config, data, url_str);

        if(!url_target) {
            // std::printf("  Unknown link: \"%s\", it will be broken inside the compiled .chm file.\n", url_str.c_str());
            continue;
        }

        std::string new_link_tag = match[1];
        new_link_tag += std::filesystem::relative(url_target->target, config.temp).string();
        new_link_tag += match[3];

        html.replace(i + match.position(), match.length(), new_link_tag);

        i += new_link_tag.length() - match.size();
    }
}


// If url has host add `target="_blank"`
void chm::update_html_remote_links_to_open_in_new_broser_window(const ProjectData &data, std::string &html) {
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

        if (url_host_sz == 0 || (url_host_sz == 1 && url_host[0] == '.')) {
            continue;
        }

        constexpr std::string_view to_insert = " target=\"_blank\"";
        html.insert(i + match.position(4), to_insert);
        i += to_insert.size();
    }
}



chm::ProjectFile* chm::find_local_file_pointed_by_url(const ProjectConfig &config, ProjectData &data, const std::string &url) {
    CURLU *url_handle = curl_url();
    RUtils::Defer( curl_url_cleanup(url_handle); );

    if (CURLUcode err = curl_url_set(url_handle, CURLUPART_URL, url.c_str(), CURLU_DEFAULT_SCHEME | CURLU_NO_AUTHORITY | CURLU_ALLOW_SPACE); err != CURLUE_OK) {
        std::puts(std::format("Failed to parse link: \"{}\": {}.", url, curl_url_strerror(err)).c_str());
        return nullptr;
    }


    char *url_host, *url_path;
    if (CURLUcode err = curl_url_get(url_handle, CURLUPART_HOST, &url_host, 0); err != CURLUE_OK) {
        RUtils::Error(curl_url_strerror(err), RUtils::ErrorType::library).print();
        return nullptr;
    }
    RUtils::Defer( curl_free(url_host); );

    if (CURLUcode err = curl_url_get(url_handle, CURLUPART_PATH, &url_path, 0); err != CURLUE_OK) {
        RUtils::Error(curl_url_strerror(err), RUtils::ErrorType::library).print();
        return nullptr;
    }
    RUtils::Defer( curl_free(url_path); );


    if (std::strlen(url_host) > 1 || (std::strlen(url_host) == 1 && url_host[0] != '.')) {
        // not a local link
        return nullptr;
    }

    if (std::strlen(url_path) <= 1) {
        // no path
        return nullptr;
    }



    // paths always start with '/', skip it to get correct result.
    std::filesystem::path path_to_search = &url_path[1];

    for (auto& f : data.files) {
        auto original = std::filesystem::relative(f.original, config.root);

        // links to files
        if (path_to_search == original) {
            return &f;
        }

        // github wiki page links
        // no file extension
        // dashes instead of spaces
        // all lower case
        original = original.replace_extension("");

        auto link_normalize = [](std::string& inout) {
            for (auto &c : inout) {
                if (std::isspace(c)) {
                    c = '-';
                }
                else if (std::isalnum(c)) {
                    c = std::tolower(c);
                }
            }
        };

        std::string path_normalized = path_to_search.string();
        std::string original_normalized = original.string();

        link_normalize(path_normalized);
        link_normalize(original_normalized);

        if (path_normalized == original_normalized) {
            return &f;
        }
    }

    RUtils::Error(std::format("Failed to find a file that the link was pointing to, it's either a bug or the link is wrong. Link: \"{}\"", url)).print();

    return nullptr;
}