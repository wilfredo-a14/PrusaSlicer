///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPAirPrintLaunch_hpp_
#define slic3r_DLPAirPrintLaunch_hpp_

#include <string>
#include <vector>

namespace Slic3r::dlp {

// Command line used to start printer/airprint.py after a confirmed DLP print.
struct AirPrintCommand {
    std::string              python_executable;
    std::string              script_path;
    std::vector<std::string> arguments;
    std::string              working_directory;
    std::string              log_path;
    std::string              error;
};

// Build the AirPrint argv. Does not spawn a process or touch hardware.
AirPrintCommand plan_airprint_command(
    const std::string &printer_directory,
    const std::string &image_directory,
    const std::string &log_directory);

} // namespace Slic3r::dlp

#endif // slic3r_DLPAirPrintLaunch_hpp_
