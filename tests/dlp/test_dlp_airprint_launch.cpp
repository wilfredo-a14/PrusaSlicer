#include <catch2/catch_test_macros.hpp>

#include "libslic3r/DLPAirPrintLaunch.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <string>
#include <vector>

using namespace Slic3r::dlp;

namespace fs = boost::filesystem;

namespace {

fs::path make_tree()
{
    const fs::path root = fs::temp_directory_path() / fs::unique_path("clip3d-airprint-%%%%-%%%%");
    fs::create_directories(root / "printer");
    fs::create_directories(root / "logs");
    fs::create_directories(root / "output" / "png");
#ifdef _WIN32
    fs::create_directories(root / ".venv" / "Scripts");
    const fs::path python = root / ".venv" / "Scripts" / "python.exe";
#else
    fs::create_directories(root / ".venv" / "bin");
    const fs::path python = root / ".venv" / "bin" / "python3";
#endif
    fs::ofstream{python} << "python";
    fs::ofstream{root / "printer" / "airprint.py"} << "print('airprint')\n";
    fs::ofstream{root / "output" / "png" / "layer_1.png"} << "png";
    return root;
}

} // namespace

TEST_CASE("AirPrint launch command uses venv python, --execute, and the image directory", "[dlp][airprint]")
{
    const fs::path root = make_tree();
    const fs::path printer = root / "printer";
    const fs::path images = root / "output" / "png";
    const fs::path logs = root / "logs";

    const AirPrintCommand command = plan_airprint_command(printer.string(), images.string(), logs.string());
    REQUIRE(command.error.empty());
#ifdef _WIN32
    CHECK(command.python_executable == (root / ".venv" / "Scripts" / "python.exe").string());
#else
    CHECK(command.python_executable == (root / ".venv" / "bin" / "python3").string());
#endif
    CHECK(command.script_path == (printer / "airprint.py").string());
    CHECK(command.working_directory == printer.string());
    CHECK(command.log_path == (logs / "airprint.log").string());

    const std::vector<std::string> &args = command.arguments;
    REQUIRE(args.size() >= 6);
    CHECK(args[0] == "-u");
    CHECK(args[1] == command.script_path);
    const auto execute = std::find(args.begin(), args.end(), "--execute");
    const auto image_dir = std::find(args.begin(), args.end(), "--image-dir");
    const auto log = std::find(args.begin(), args.end(), "--log");
    REQUIRE(execute != args.end());
    REQUIRE(image_dir != args.end());
    REQUIRE(std::next(image_dir) != args.end());
    CHECK(*std::next(image_dir) == images.string());
    REQUIRE(log != args.end());
    REQUIRE(std::next(log) != args.end());
    CHECK(*std::next(log) == command.log_path);

    fs::remove_all(root);
}

TEST_CASE("AirPrint launch command reports a missing printer script", "[dlp][airprint]")
{
    const fs::path root = make_tree();
    fs::remove(root / "printer" / "airprint.py");

    const AirPrintCommand command = plan_airprint_command((root / "printer").string(),
                                                          (root / "output" / "png").string(),
                                                          (root / "logs").string());
    CHECK_FALSE(command.error.empty());
    CHECK(command.error.find("airprint.py") != std::string::npos);

    fs::remove_all(root);
}

TEST_CASE("AirPrint launch command reports a missing Python environment", "[dlp][airprint]")
{
    const fs::path root = make_tree();
    fs::remove_all(root / ".venv");

    const AirPrintCommand command = plan_airprint_command((root / "printer").string(),
                                                          (root / "output" / "png").string(),
                                                          (root / "logs").string());
    CHECK_FALSE(command.error.empty());
    CHECK(command.error.find(".venv") != std::string::npos);

    fs::remove_all(root);
}
