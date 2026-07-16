#include <catch2/catch_test_macros.hpp>

#include "libslic3r/DLPDebugLog.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace Slic3r::dlp;

TEST_CASE("DLP debug log path points at logs/dlp_corkscrew.log", "[dlp][debug]")
{
    const std::string path = debug_log_path();
    REQUIRE_FALSE(path.empty());
    CHECK(path.find("dlp_corkscrew.log") != std::string::npos);
    CHECK(path.find("logs") != std::string::npos);
}

TEST_CASE("DLP debug log mirror stdout flag toggles", "[dlp][debug]")
{
    const bool previous = debug_log_mirror_stdout();

    set_debug_log_mirror_stdout(true);
    CHECK(debug_log_mirror_stdout());

    set_debug_log_mirror_stdout(false);
    CHECK_FALSE(debug_log_mirror_stdout());

    set_debug_log_mirror_stdout(previous);
}

TEST_CASE("DLP debug_log appends a timestamped line to the log file", "[dlp][debug]")
{
    namespace fs = std::filesystem;

    const fs::path path = debug_log_path();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    {
        std::ofstream truncate(path, std::ios::trunc);
        REQUIRE(truncate.good());
    }

    const std::string marker = "dlp-unit-test-marker-42";
    set_debug_log_mirror_stdout(false);
    debug_log(marker);

    std::ifstream in(path);
    REQUIRE(in.good());

    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    CHECK(contents.find(marker) != std::string::npos);
    CHECK(contents.find('[') != std::string::npos);
}

TEST_CASE("DLP print_debug_log_tail prints recent lines", "[dlp][debug]")
{
    namespace fs = std::filesystem;
    const fs::path path = debug_log_path();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    {
        std::ofstream truncate(path, std::ios::trunc);
        REQUIRE(truncate.good());
    }

    set_debug_log_mirror_stdout(false);
    for (int i = 0; i < 15; ++i)
        debug_log("tail-line-" + std::to_string(i));

    // Smoke: helper must not throw / crash.
    print_debug_log_tail(10);
}
