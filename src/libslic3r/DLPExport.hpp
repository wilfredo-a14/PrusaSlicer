///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPExport_hpp_
#define slic3r_DLPExport_hpp_

#include <functional>
#include <string>

namespace Slic3r {

class SLAPrint;
struct ExecutionTBB;

namespace dlp {

// True when `directory` exists and contains at least one file or subdirectory.
bool export_directory_has_contents(const std::string &directory);

// Delete every file and subdirectory inside `directory`, leaving the folder itself.
// Returns an empty string on success, or an error message.
std::string clear_export_directory(const std::string &directory);

// Export rasterized SLA layers as PNG files when a directory has been set on the print.
void export_png_layers(SLAPrint &print,
                       const ExecutionTBB &ex_tbb,
                       const std::function<bool()> &canceled);

} // namespace dlp
} // namespace Slic3r

#endif // slic3r_DLPExport_hpp_
