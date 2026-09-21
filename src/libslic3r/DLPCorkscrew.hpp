///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPCorkscrew_hpp_
#define slic3r_DLPCorkscrew_hpp_

#include "BoundingBox.hpp"
#include "DLPConfig.hpp"
#include "ExPolygon.hpp"

#include <vector>

namespace Slic3r {

class SLAPrint;

namespace dlp {

struct CorkscrewBoxSlice {
    int          index;
    BoundingBox  region;
    ExPolygons   polygons;
    double       area;
};

struct CorkscrewVerificationResult {
    bool        ran         { false };
    bool        ok          { true };
    size_t      layer_count { 0 };
    size_t      failed_layers { 0 };
};

// Split layer polygons into equal-width vertical strips.
std::vector<CorkscrewBoxSlice> partition_layer_into_boxes(
    const ExPolygons &layer_slices,
    int               box_count);

// Verify per-layer box partitioning. Returns a summary; logs when corkscrew mode is enabled.
CorkscrewVerificationResult verify_corkscrew_box_slices(SLAPrint &print);

// Result from the most recent verify_corkscrew_box_slices() call.
const CorkscrewVerificationResult &last_corkscrew_verification();

// When corkscrew mode is on, verify per-layer box partitioning and write to logs/dlp_corkscrew.log.
void debug_corkscrew_box_slices(SLAPrint &print);

} // namespace dlp
} // namespace Slic3r

#endif // slic3r_DLPCorkscrew_hpp_
