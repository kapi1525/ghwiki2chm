#pragma once

#include <string>
#include <string_view>
#include <filesystem>

#include <RUtils/ErrorOr.hpp>



std::string remove_html_tags(std::string_view in);
std::string_view trim_whitespace(std::string_view in);
std::string remove_hashes(std::string_view in);
RUtils::ErrorOr<std::string> convert_markdown_file_to_html(std::filesystem::path file);