#include <regex>

#include "table_of_contents.hpp"
#include "helpers.hpp"
#include "project.hpp"



// Converts to hhc format. Toc item is treated as root, all members except children are not needed.
std::string chm::TableOfContentsItem::to_hhc(const std::filesystem::path& temp_path) const {
    std::string ret = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">\n"
    "<HTML>\n"
    "<HEAD>\n"
        "<meta name=\"GENERATOR\"content=\"ghwiki2chm test\">\n"
        "<!-- Sitemap 1.0 -->\n"
    "</HEAD>\n"
    "<BODY>\n"
        "<OBJECT type=\"text/site properties\">\n"
        // "<param name=\"Property Name\" value=\"Property Value\">\n";
        "</OBJECT>\n"
    "<UL>\n";

    for (auto &&i : children) {
        ret += i.to_hhc_entry(temp_path);
    }
    ret += "</UL>\n"

    "</BODY>\n"
    "</HTML>\n";

    return ret;
}

// Converts item to hhc format. Recursive.
std::string chm::TableOfContentsItem::to_hhc_entry(const std::filesystem::path& temp_path) const {
    std::string ret = "<LI> <OBJECT type=\"text/sitemap\">\n";
    ret += "<param name=\"Name\" value=\"" + name + "\">\n";

    if(file_link) {
        std::string link = std::filesystem::relative(file_link->target, temp_path).string();
        if(!fragment.empty()) {
            link += "#" + fragment;
        }
        ret += "<param name=\"Local\" value=\"" + link + "\">\n";
    }

    ret += "</OBJECT>\n";

    if(children.empty()) {
        return ret;
    }

    ret += "<UL>\n";

    for (auto &&i : children) {
        ret += i.to_hhc_entry(temp_path);
    }

    ret += "</UL>\n";

    return ret;
}