///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPExport_hpp_
#define slic3r_DLPExport_hpp_

#include <functional>

namespace Slic3r {

class SLAPrint;
struct ExecutionTBB;

namespace dlp {

// Export rasterized SLA layers as PNG files when a directory has been set on the print.
void export_png_layers(SLAPrint &print,
                       const ExecutionTBB &ex_tbb,
                       const std::function<bool()> &canceled);

} // namespace dlp
} // namespace Slic3r

#endif // slic3r_DLPExport_hpp_
