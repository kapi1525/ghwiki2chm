#pragma once

#include <cstdint>
#include <filesystem>



namespace chm {
    // github supports more markup formats so potentialy others can be added in the future.
    enum class conversion_type : std::uint32_t {
        copy,
        markdown,
    };

    struct project_file {
        std::filesystem::path original; // Original file
        std::filesystem::path target;   // File in temp path, copied or converted from supported format to html. Will be included inside chm.
        conversion_type converter;      // What converter should be used.
    };
}