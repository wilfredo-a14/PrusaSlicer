///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPAirPrintLaunch.hpp"

#include <boost/filesystem.hpp>

namespace Slic3r::dlp {

namespace fs = boost::filesystem;

namespace {

fs::path venv_python(const fs::path &repository_root)
{
#ifdef _WIN32
    return repository_root / ".venv" / "Scripts" / "python.exe";
#else
    const fs::path python3 = repository_root / ".venv" / "bin" / "python3";
    if (fs::is_regular_file(python3))
        return python3;
    return repository_root / ".venv" / "bin" / "python";
#endif
}

} // namespace

AirPrintCommand plan_airprint_command(
    const std::string &printer_directory,
    const std::string &image_directory,
    const std::string &log_directory)
{
    AirPrintCommand command;
    const fs::path printer(printer_directory);
    const fs::path script = printer / "airprint.py";
    if (printer_directory.empty() || !fs::is_regular_file(script)) {
        command.error = "Could not locate printer/airprint.py. Start PrusaSlicer with this repository's launcher.";
        return command;
    }

    const fs::path python = venv_python(printer.parent_path());
    if (!fs::is_regular_file(python)) {
        command.error = "Could not find the printer Python environment at "
                        + (printer.parent_path() / ".venv").string()
                        + ". Create .venv and install the printer dependencies before printing.";
        return command;
    }

    const fs::path logs = log_directory.empty() ? printer.parent_path() / "logs" : fs::path(log_directory);
    command.python_executable = python.string();
    command.script_path = script.string();
    command.working_directory = printer.string();
    command.log_path = (logs / "airprint.log").string();
    command.arguments = {
        "-u",
        command.script_path,
        "--execute",
        "--image-dir",
        image_directory,
        "--log",
        command.log_path,
    };
    return command;
}

} // namespace Slic3r::dlp
