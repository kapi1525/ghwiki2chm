#include <vector>
#include <fstream>

#define NOMINMAX // Maybe a bug in curl.wrap: on windows min max macros are added and collide with std::min/std::max
                 // why microsoft didn't make this the default already??? no one uses those.
#include "curl/curl.h"

#include "project.hpp"



// for curl
static size_t write_callback(char *ptr, std::size_t size, std::size_t nmemb, std::ofstream *file) {
    size_t bytes_to_write = size * nmemb;

    file->write(ptr, bytes_to_write);

    return bytes_to_write;
}

// Download remote dependencies
void chm::download_dependencies(const ProjectConfig &config, ProjectData &data) {
    // Nothing to download.
    if(data.remote_dependencies.size() == 0) {
        return;
    }

    std::printf("Will download %zu remote dependencies...\n", data.remote_dependencies.size());


    size_t max_downloads = std::min((size_t)config.max_downloads, data.remote_dependencies.size());
    size_t next_dep_to_download_index = 0;  // index to remote_dependencies[]

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURLM* multi_handle = curl_multi_init();


    struct DownloaderState {
        CURL* handle;
        RemoteDependency* dep_ptr; // assigned file or nullptr
        std::ofstream* file_ptr;    // target file output stream or nullptr
    };

    std::vector<DownloaderState> downloaders(max_downloads);

    for (auto& download : downloaders) {
        CURL* handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, !config.dep_download_ignore_ssl);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, !config.dep_download_ignore_ssl);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, config.dep_download_curl_verbose);
        download.handle = handle;
    }


    int running_handles = 0;
    size_t downloaded_count = 0;

    do {
        if((size_t)running_handles < max_downloads && next_dep_to_download_index < data.remote_dependencies.size()) {
            for (auto& download : downloaders) {
                if (download.dep_ptr == nullptr && download.file_ptr == nullptr) {
                    download.dep_ptr = &data.remote_dependencies[next_dep_to_download_index];
                    next_dep_to_download_index++;

                    std::filesystem::create_directories(std::filesystem::absolute(download.dep_ptr->target).remove_filename());
                    download.file_ptr = new std::ofstream(download.dep_ptr->target);

                    curl_easy_setopt(download.handle, CURLOPT_URL, download.dep_ptr->link.c_str());
                    curl_easy_setopt(download.handle, CURLOPT_WRITEDATA, download.file_ptr);
                    curl_multi_add_handle(multi_handle, download.handle);
                }

                if(next_dep_to_download_index >= data.remote_dependencies.size()) {
                    break;
                }
            }
        }

        curl_multi_perform(multi_handle, &running_handles);

        int msgs_in_queue = 0;
        while (CURLMsg* msg = curl_multi_info_read(multi_handle, &msgs_in_queue)) {
            if(msg->msg == CURLMSG_DONE) {
                for (auto& download : downloaders) {
                    if(download.handle == msg->easy_handle) {
                        curl_multi_remove_handle(multi_handle, download.handle);
                        std::printf("%s\n", download.dep_ptr->link.c_str());
                        download.dep_ptr = nullptr;
                        delete download.file_ptr;
                        download.file_ptr = nullptr;
                        downloaded_count++;
                        break;
                    }
                }
            }
        };

        int fds;
        curl_multi_poll(multi_handle, nullptr, 0, 1000, &fds);
    } while (running_handles || next_dep_to_download_index < data.remote_dependencies.size());


    for (auto& download : downloaders) {
        curl_easy_cleanup(download.handle);
        download.handle = nullptr;
    }

    curl_multi_cleanup(multi_handle);

    std::printf("%zu/%zu.\n", downloaded_count, data.remote_dependencies.size());
}
