#pragma once

#include <string>
#include <string_view>



std::string remove_html_tags(std::string_view in);
std::string_view trim_whitespace(std::string_view in);