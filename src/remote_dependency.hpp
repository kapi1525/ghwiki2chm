#pragma once

#include <filesystem>
#include <string>



namespace chm {
    enum class DownloadState : uint8_t {
        NotStarted,
        InProgress,
        Finished,
        Failed,
    };

    struct RemoteDependency {
        std::string link;
        std::filesystem::path target;
        DownloadState state = DownloadState::NotStarted;
    };
}