///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPDebugLog.hpp"

#include <boost/nowide/iostream.hpp>

#include <chrono>
#ifdef SLIC3R_DLP_FILE_LOG
#include <deque>
#include <filesystem>
#include <fstream>
#endif
#include <iomanip>
#ifdef SLIC3R_DLP_FILE_LOG
#include <mutex>
#endif
#include <sstream>

namespace Slic3r::dlp {

namespace {

bool &mirror_stdout_flag()
{
    static bool enabled = false;
    return enabled;
}

#ifdef SLIC3R_DLP_FILE_LOG
std::mutex &log_mutex()
{
    static std::mutex m;
    return m;
}

std::filesystem::path log_file_path()
{
    return std::filesystem::current_path() / "logs" / "dlp_corkscrew.log";
}
#endif

std::string timestamp()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto t   = clock::to_time_t(now);
    std::tm    tm  = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

void set_debug_log_mirror_stdout(bool enable) { mirror_stdout_flag() = enable; }

bool debug_log_mirror_stdout() { return mirror_stdout_flag(); }

std::string debug_log_path()
{
#ifdef SLIC3R_DLP_FILE_LOG
    return log_file_path().string();
#else
    return {};
#endif
}

void debug_log(const std::string &message)
{
#ifndef SLIC3R_DLP_FILE_LOG
    if (!mirror_stdout_flag())
        return;
#endif
    const std::string line = "[" + timestamp() + "] " + message;

    if (mirror_stdout_flag())
        boost::nowide::cout << line << std::endl;

#ifdef SLIC3R_DLP_FILE_LOG
    std::lock_guard<std::mutex> lock(log_mutex());

    namespace fs = std::filesystem;
    const fs::path path = log_file_path();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::app);
    if (!out)
        return;

    out << line << '\n';
#endif
}

void print_debug_log_tail(size_t line_count)
{
#ifdef SLIC3R_DLP_FILE_LOG
    if (line_count == 0)
        return;

    std::lock_guard<std::mutex> lock(log_mutex());

    std::ifstream in(log_file_path());
    if (!in)
        return;

    std::deque<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
        if (lines.size() > line_count)
            lines.pop_front();
    }

    if (lines.empty())
        return;

    boost::nowide::cout << "--- last " << lines.size() << " debug log lines ---" << std::endl;
    for (const std::string &l : lines)
        boost::nowide::cout << l << std::endl;
#else
    (void) line_count;
#endif
}

} // namespace Slic3r::dlp
