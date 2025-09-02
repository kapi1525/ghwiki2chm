#pragma once

#include <cstdint>
#include <filesystem>



namespace chm {
    // github supports more markup formats so potentialy others can be added in the future.
    enum class ConversionType : std::uint32_t {
        none,
        copy,
        from_markdown,
    };

    struct ProjectFile {
        std::filesystem::path original;                     // Original file
        std::filesystem::path target;                       // File in temp path, copied or converted from supported format to html. Will be included inside chm.
        ConversionType converter = ConversionType::none;    // What converter should be used.
    };
}