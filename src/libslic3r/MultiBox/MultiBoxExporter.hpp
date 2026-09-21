#ifndef slic3r_MultiBoxExporter_hpp_
#define slic3r_MultiBoxExporter_hpp_

#include <string>
#include <vector>
#include <functional>

#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {

class SLAPrint;

namespace MultiBox {

struct BoxOverlay
{
    double center_x_mm { 0. };
    double center_y_mm { 0. };
    double width_mm    { 0. };
    double height_mm   { 0. };
    double angle_deg   { 0. };
};

struct ExportResult
{
    size_t layers_exported { 0 };
    size_t boxes_per_layer { 0 };
    size_t files_written   { 0 };
    std::string error;
};

using ProgressFn = std::function<bool(size_t done, size_t total)>;

std::vector<BoxOverlay> compute_static_box_overlays(const SLAPrintObjectConfig &config);

ExportResult export_print(
    const SLAPrint   &print,
    const std::string &output_dir,
    const std::string &project_name,
    ProgressFn        progress = nullptr);

} // namespace MultiBox
} // namespace Slic3r

#endif // slic3r_MultiBoxExporter_hpp_
