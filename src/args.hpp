#pragma once

#include <functional>
#include <vector>
#include <string>



struct cmd_parser_flag {
    char short_flag;                            // eg 'h' = "-h"
    // const char* long_flag;                      // eg "help" = "--help"
    std::function<void()> callback;             // called when short or long flag is present
};

struct cmd_parser_arg {
    char short_arg;                             // eg 'o' = "-o test.txt" string "test.txt" is passed to callback
    // const char* long_arg;                       // eg "out" = "--out test.txt" string "test.txt" is passed to callback
    std::function<void(std::string)> callback;  // called when short or long arg is present argument value is passed
};

class cmd_parser {
public:
    std::vector<cmd_parser_flag> flags;
    std::vector<cmd_parser_arg> args;

    // Returns false on invalid command line input, can be used to print help message.
    bool parse(int argc, const char* argv[]);
};

