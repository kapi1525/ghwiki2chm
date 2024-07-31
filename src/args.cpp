#include <cstdio>
#include <cstring>

#include "args.hpp"



#if 0
    #define debug_print(...) std::printf(__VA_ARGS__)
#else
    #define debug_print(...)
#endif



bool is_valid_flag(const char* arg) {
    return  std::strlen(arg) == 2 && arg[0] == '-' && arg[1] != '-';
}

bool cmd_parser::parse(int argc, const char* argv[]) {
    debug_print("Parsing command line...\n");


    for (int i = 1; i < argc; i++) {
        bool handled = false;
        debug_print("%s\n", argv[i]);

        if(!is_valid_flag(argv[i])) {
            debug_print("Not a flag or argument.\n");
            return false;
        }

        for (auto &&flag : flags) {
            if(flag.short_flag != argv[i][1]) {
                continue;
            }

            flag.callback();
            handled = true;
            goto handled_arg;
        }

        for (auto &&arg : args) {
            if(arg.short_arg != argv[i][1]) {
                continue;
            }

            std::string arg_param = "";

            if(i + 1 < argc && !is_valid_flag(argv[i + 1])) {
                arg_param = argv[i + 1];
                i++;
            }

            arg.callback(arg_param);
            handled = true;
            goto handled_arg;
        }

        if(!handled) {
            debug_print("Unknown flag or argument.\n");
            return false;
        }

        handled_arg:
        continue;       // Apparently label at the end is non standard until C++23 lol
    }

    return true;
}