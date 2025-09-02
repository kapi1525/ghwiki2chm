#include <fstream>

#include <maddy/parser.h>

#include "helpers.hpp"

using namespace RUtils;



ErrorOr<std::string> convert_markdown_file_to_html(std::filesystem::path file) {
    static maddy::Parser parser;
    std::ifstream md_file(file);
    return parser.Parse(md_file);
}