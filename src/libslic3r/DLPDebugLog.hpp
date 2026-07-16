///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPDebugLog_hpp_
#define slic3r_DLPDebugLog_hpp_

#include <string>

namespace Slic3r::dlp {

// Append a timestamped line to logs/dlp_corkscrew.log (gitignored).
void debug_log(const std::string &message);

// When enabled, debug_log() also writes to stdout.
// Off by default so headless CLI stays quiet; details stay in the log file.
void set_debug_log_mirror_stdout(bool enable);
bool debug_log_mirror_stdout();

// Path to the DLP debug log file (relative to cwd unless absolute).
std::string debug_log_path();

// Print the last N lines of the DLP debug log to stdout (for CLI summaries).
void print_debug_log_tail(size_t line_count = 10);

} // namespace Slic3r::dlp

#endif // slic3r_DLPDebugLog_hpp_
