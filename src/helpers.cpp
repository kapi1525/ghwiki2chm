#include <cctype>

#include "helpers.hpp"



std::string remove_html_tags(std::string_view in) {
    int tags_inside = 0;
    bool tag = false;
    bool is_close_tag = false;

    std::string out;

    out.reserve(in.size());

    for (auto& c : in) {
        if (c == '<') {
            tag = true;
            continue;
        }
        if (!tag) {
            if (tags_inside == 0) {
                out += c;
            }
            continue;
        }
        if (c == '/') {
            is_close_tag = true;
        }
        if (c == '>') {
            tag = false;
            if (!is_close_tag) {
                tags_inside++;
            } else {
                tags_inside--;
                if (tags_inside < 0) {
                    tags_inside = 0;
                }
            }
            is_close_tag = false;
        }
    }

    return out;
}

std::string_view trim_whitespace(std::string_view in) {
    size_t start = 0, end = in.size();

    for (size_t i = 0; i < in.size(); i++) {
        auto c = in[i];

        if (!std::isspace(c)) {
            start = i;
            break;
        }
    }

    for (size_t i = in.size(); i > 0; i--) {
        auto c = in[i - 1];

        if (!std::isspace(c)) {
            end = i;
            break;
        }
    }

    return in.substr(start, end - start);
}

std::string remove_hashes(std::string_view in) {
    std::string out;

    out.reserve(in.size());

    for (auto c : in) {
        if(c != '#') {
            out += c;
        }
    }

    return out;
}