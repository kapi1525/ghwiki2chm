#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <string>
#include <filesystem>
#include <variant>
#include <mutex>



namespace chm {

    // https://www.nongnu.org/chmspec/latest/INI.html#HHP_WINDOWS
    enum navigation_window_style : std::uint32_t {
        auto_hide_sidebar       = 0x00000001, // Automatically hide/show tri-pane window: when the help window has focus the navigation pane is visible, otherwise it is hidden.
        always_on_top           = 0x00000002, // Keep the help window on top.
        no_titlebar             = 0x00000004, // No title bar
        no_default_style        = 0x00000008, // No default window styles (only HH_WINTYPE.dwStyles)
        no_default_exstyle      = 0x00000010, // No default extended window styles (only HH_WINTYPE.dwExStyles)
        tri_pane_window         = 0x00000020, // Use a tri-pane window
        small_toolbar           = 0x00000040, // No text on toolbar buttons
        wm_quit_on_close        = 0x00000080, // Post WM_QUIT message when window closes
        sync_sidebar_with_topic = 0x00000100, // When the current topic changes automatically sync contents and index.
        send_tracking_messages  = 0x00000200, // Send tracking notification messages
        search_tab              = 0x00000400, // Include search tab in navigation pane
        history_tab             = 0x00000800, // Include history tab in navigation pane
        favorites_tab           = 0x00001000, // Include favorites tab in navigation pane
        html_title_in_titlebar  = 0x00002000, // Put current HTML title in title bar
        only_navigation         = 0x00004000, // Only display the navigation window
        no_toolbar              = 0x00008000, // Don't display a toolbar
        msdn                    = 0x00010000, // MSDN Menu
        fts                     = 0x00020000, // Advanced FTS UI. (???)
        resizable               = 0x00040000, // After initial creation, user controls window size/position
        custom_tab_1            = 0x00080000, // Use custom tab #1
        custom_tab_2            = 0x00100000, // Use custom tab #2
        custom_tab_3            = 0x00200000, // Use custom tab #3
        custom_tab_4            = 0x00400000, // Use custom tab #4
        custom_tab_5            = 0x00800000, // Use custom tab #5
        custom_tab_6            = 0x01000000, // Use custom tab #6
        custom_tab_7            = 0x02000000, // Use custom tab #7
        custom_tab_8            = 0x04000000, // Use custom tab #8
        custom_tab_9            = 0x08000000, // Use custom tab #9
        margin                  = 0x10000000, // The window type has a margin

        default_style = (tri_pane_window | sync_sidebar_with_topic | search_tab | html_title_in_titlebar | resizable | margin),
    };

    enum toolbar_buttons : std::uint32_t {
        hide_show               = 0x00000002, // Hide/Show button hides/shows the navigation pane.
        back                    = 0x00000004, // Back button.
        forward                 = 0x00000008, // Forward button.
        stop                    = 0x00000010, // Stop button.
        refresh                 = 0x00000020, // Refresh button.
        home                    = 0x00000040, // Home button.
        next                    = 0x00000080, // Next button. Not implemented by HH.
        previous                = 0x00000100, // Previous button. Not implemented by HH.
        notes                   = 0x00000200, // Notes button. Not implemented by HH.
        contents                = 0x00000400, // Contents button. Not implemented by HH.
        locate                  = 0x00000800, // Locate button. Jumps to the current topic in the contents pane.
        options                 = 0x00001000, // Options button.
        print                   = 0x00002000, // Print button.
        index                   = 0x00004000, // Index button. Not implemented by HH.
        search                  = 0x00008000, // Search button. Not implemented by HH.
        history                 = 0x00010000, // History button. Not implemented by HH.
        favotites               = 0x00020000, // Favorites button. Not implemented by HH.
        jump1                   = 0x00040000, // Jump 1 button. Customised using argument 7 of the [WINDOWS] entry.
        jump2                   = 0x00080000, // Jump 2 button. Customised using argument 9 of the [WINDOWS] entry.
        font                    = 0x00100000, // Font button. Changes the size of the text shown in the IE HTML display pane.
        next_topc               = 0x00200000, // Next button. Jumps to the next topic in the contents pane. Requires Binary TOC to be on.
        previous_topc           = 0x00400000, // Previous button. Jumps to the previous topic in the contents pane. Requires Binary TOC to be on.

        default_buttons = (hide_show | locate | back | forward | home | print | options),
    };



    struct toc_item {
        std::string name;
        std::filesystem::path* file_link = nullptr;
        std::string target_fragment;
        std::vector<toc_item> children;
    };

    using table_of_contents = std::deque<toc_item>;


    class project {
    public:
        project() = default;
        ~project() = default;

        std::filesystem::path root_path, temp_path, out_file;
        std::filesystem::path* default_file_link = nullptr;
        std::string title = "test";
        table_of_contents toc;
        bool auto_toc = true;

        // create a project config automaticaly from md files and _Sidebar.
        bool create_from_ghwiki(std::filesystem::path default_file);

        void convert_source_files();        // Copy or covert project source files to temp dir
        void download_dependencies();       // Download remote images that are used in the project
        void generate_project_files();      // Create .hhc .hhp

    private:
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
        std::deque<project_file> files;

        // Aditional local files that should be included into the chm, like images.
        // Images and other files should be here because files deque cant be changed during scaning for dependencies.
        std::deque<project_file> local_dependencies;

        // Files on the web that should be downloaded and included into the chm, like images.
        struct remote_dependency {
            std::string link;
            std::filesystem::path target;
            bool downloading = false;
            bool downloaded = false;
            bool download_failed = false;
        };
        std::deque<remote_dependency> remote_dependencies;

        void download_depdendencies_thread(std::mutex& dependencies_deque_mutex);

        std::string to_hhc(toc_item& item);

        void scan_html_for_local_dependencies(const std::string& html); // Looks for local dependencies like images and includes them into the project
        void scan_html_for_remote_dependencies(std::string& html);      // Same as above but looks for remote images that should be downloaded and updates the url to point to a local file
        void update_html_headings(std::string& html);                   // Update heading tags (<h1>) to include id like in github wikis
        void update_html_links(std::string& html);                      // Update link tags <a> to have correct url

        toc_item create_toc_entries(std::filesystem::path* file, const std::string& html);  // Create toc entry by looking for heading tags in generated html
    };



    // Special arguments that shoud be replaced with actual value before calling the compiler
    enum class compiler_special_arg {
        project_file_path,
    };

    struct compiler_info {
        std::string executable;
        std::vector<std::variant<std::string, compiler_special_arg>> args;
    };


    const compiler_info* find_available_compiler();
    bool is_compiler_valid(const compiler_info* compiler);
    bool compile(project* proj, const compiler_info* compiler);
}