#include <catch2/catch_test_macros.hpp>

#include "libslic3r/DLPExport.hpp"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>

using namespace Slic3r::dlp;

namespace fs = std::filesystem;

namespace {

fs::path make_temp_dir()
{
    const fs::path root = fs::temp_directory_path() / ("clip3d-export-" + std::to_string(std::rand()));
    fs::create_directories(root);
    return root;
}

} // namespace

TEST_CASE("Empty export directory reports no contents", "[dlp][export]")
{
    const fs::path root = make_temp_dir();
    CHECK_FALSE(export_directory_has_contents(root.string()));
    fs::remove_all(root);
}

TEST_CASE("Export directory with a file reports contents", "[dlp][export]")
{
    const fs::path root = make_temp_dir();
    std::ofstream{root / "layer_0000.png"} << "png";
    CHECK(export_directory_has_contents(root.string()));
    fs::remove_all(root);
}

TEST_CASE("Clearing an export directory removes files and nested folders", "[dlp][export]")
{
    const fs::path root = make_temp_dir();
    std::ofstream{root / "layer_0000.png"} << "old";
    std::ofstream{root / "layer_0195.png"} << "stale";
    fs::create_directories(root / "nested");
    std::ofstream{root / "nested" / "keep-me-not.txt"} << "gone";

    const std::string error = clear_export_directory(root.string());
    REQUIRE(error.empty());
    CHECK(fs::is_directory(root));
    CHECK_FALSE(export_directory_has_contents(root.string()));

    fs::remove_all(root);
}

TEST_CASE("Clearing a missing path returns an error", "[dlp][export]")
{
    const std::string error = clear_export_directory("/this/path/does/not/exist-clip3d");
    CHECK_FALSE(error.empty());
}
