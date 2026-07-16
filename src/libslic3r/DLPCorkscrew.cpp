///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPCorkscrew.hpp"

#include "ClipperUtils.hpp"
#include "DLPDebugLog.hpp"
#include "SLAPrint.hpp"

#include <cmath>
#include <sstream>

#include "format.hpp"

namespace Slic3r::dlp {

std::vector<CorkscrewBoxSlice> partition_layer_into_boxes(
    const ExPolygons &layer_slices,
    int               box_count)
{
    std::vector<CorkscrewBoxSlice> boxes;
    if (box_count < 1) {
        debug_log(Slic3r::format(
            "corkscrew partition: abort — invalid box_count=%1% (expected >= 1)",
            box_count));
        return boxes;
    }
    if (layer_slices.empty()) {
        debug_log("corkscrew partition: abort — empty layer_slices");
        return boxes;
    }

    BoundingBox layer_bbox = get_extents(layer_slices);
    if (!layer_bbox.defined) {
        debug_log("corkscrew partition: abort — layer bounding box undefined");
        return boxes;
    }

    const coord_t total_width = layer_bbox.max.x() - layer_bbox.min.x();
    if (total_width <= 0) {
        debug_log(Slic3r::format(
            "corkscrew partition: abort — non-positive width=%1% (bbox x=%2%..%3%)",
            total_width,
            unscale<double>(layer_bbox.min.x()),
            unscale<double>(layer_bbox.max.x())));
        return {};
    }

    const double total_area = area(layer_slices);
    debug_log(Slic3r::format(
        "corkscrew partition: start box_count=%1% polys=%2% area=%3% "
        "bbox=[%4%,%5%]x[%6%,%7%] width_mm=%8%",
        box_count,
        layer_slices.size(),
        total_area,
        unscale<double>(layer_bbox.min.x()),
        unscale<double>(layer_bbox.max.x()),
        unscale<double>(layer_bbox.min.y()),
        unscale<double>(layer_bbox.max.y()),
        unscale<double>(total_width)));

    boxes.resize(box_count);

    for (int i = 0; i < box_count; ++i) {
        CorkscrewBoxSlice &box = boxes[i];
        box.index = i;

        const coord_t x0 = layer_bbox.min.x() + (total_width * i) / box_count;
        const coord_t x1 = layer_bbox.min.x() + (total_width * (i + 1)) / box_count;

        box.region.min.x() = x0;
        box.region.max.x() = (i == box_count - 1) ? layer_bbox.max.x() : x1;
        box.region.min.y() = layer_bbox.min.y();
        box.region.max.y() = layer_bbox.max.y();

        const Polygons clip{box.region.polygon()};
        box.polygons = intersection_ex(layer_slices, clip);
        box.area     = area(box.polygons);

        debug_log(Slic3r::format(
            "corkscrew partition: box %1%/%2% x=[%3%,%4%] width_mm=%5% polys=%6% area=%7%",
            i + 1,
            box_count,
            unscale<double>(box.region.min.x()),
            unscale<double>(box.region.max.x()),
            unscale<double>(box.region.max.x() - box.region.min.x()),
            box.polygons.size(),
            box.area));
    }

    double partitioned_area = 0.;
    for (const CorkscrewBoxSlice &box : boxes)
        partitioned_area += box.area;

    const double area_delta = std::abs(partitioned_area - total_area);
    const double rel_error  = total_area > 0. ? area_delta / total_area : 0.;
    debug_log(Slic3r::format(
        "corkscrew partition: done total_area=%1% partitioned_area=%2% "
        "abs_delta=%3% rel_error_pct=%4%",
        total_area,
        partitioned_area,
        area_delta,
        rel_error * 100.));

    return boxes;
}

namespace {

CorkscrewVerificationResult &last_result()
{
    static CorkscrewVerificationResult result;
    return result;
}

} // namespace

const CorkscrewVerificationResult &last_corkscrew_verification()
{
    return last_result();
}

CorkscrewVerificationResult verify_corkscrew_box_slices(SLAPrint &print)
{
    CorkscrewVerificationResult result;
    last_result() = result;

    const auto &cfg = print.default_object_config();
    if (!cfg.corkscrew_enable.getBool()) {
        debug_log("corkscrew verify: skipped — corkscrew_enable=false");
        return result;
    }

    result.ran = true;
    const int box_count = cfg.corkscrew_box_count.getInt();
    debug_log(Slic3r::format(
        "corkscrew verify: starting box_count=%1% layers=%2% "
        "display_defaults=%3%x%4%",
        box_count,
        print.print_layers().size(),
        DISPLAY_PIXELS_X,
        DISPLAY_PIXELS_Y));

    if (box_count < 1) {
        result.ok = false;
        debug_log(Slic3r::format(
            "corkscrew verify: FAIL invalid box_count=%1% (expected >= 1)",
            box_count));
        last_result() = result;
        return result;
    }

    size_t layer_idx = 0;
    for (const SLAPrint::PrintLayer &layer : print.print_layers()) {
        const ExPolygons &slices = layer.transformed_slices();
        if (slices.empty()) {
            debug_log(Slic3r::format(
                "corkscrew verify: layer %1% empty slice — skipping",
                layer_idx));
            ++layer_idx;
            continue;
        }

        const double total_area = area(slices);
        const auto   boxes      = partition_layer_into_boxes(slices, box_count);

        if (static_cast<int>(boxes.size()) != box_count) {
            ++result.failed_layers;
            debug_log(Slic3r::format(
                "corkscrew verify: layer %1% FAIL expected %2% boxes, got %3%",
                layer_idx,
                box_count,
                boxes.size()));
            ++layer_idx;
            continue;
        }

        std::ostringstream box_details;
        double             partitioned_area = 0.;
        for (const CorkscrewBoxSlice &box : boxes) {
            partitioned_area += box.area;
            box_details << Slic3r::format(
                " box%1%[x=%2%-%3% polys=%4% area=%5%]",
                box.index,
                unscale<double>(box.region.min.x()),
                unscale<double>(box.region.max.x()),
                box.polygons.size(),
                box.area);
        }

        const double area_delta = std::abs(partitioned_area - total_area);
        const double rel_error  = total_area > 0. ? area_delta / total_area : 0.;
        const bool   layer_ok   = rel_error < 0.01;

        if (!layer_ok)
            ++result.failed_layers;

        debug_log(Slic3r::format(
            "corkscrew verify: layer %1% total_area=%2% partitioned_area=%3% "
            "rel_error_pct=%4% ok=%5%",
            layer_idx,
            total_area,
            partitioned_area,
            rel_error * 100.,
            layer_ok ? "yes" : "NO"));
        debug_log(Slic3r::format(
            "corkscrew verify: layer %1% boxes:%2%",
            layer_idx,
            box_details.str()));

        ++layer_idx;
    }

    result.layer_count = layer_idx;
    result.ok          = result.failed_layers == 0;

    debug_log(Slic3r::format(
        "corkscrew verify: finished layers=%1% failed=%2% result=%3%",
        result.layer_count,
        result.failed_layers,
        result.ok ? "PASSED" : "FAILED"));

    last_result() = result;
    return result;
}

void debug_corkscrew_box_slices(SLAPrint &print)
{
    debug_log("corkscrew verify: debug_corkscrew_box_slices() entry");
    verify_corkscrew_box_slices(print);
}

} // namespace Slic3r::dlp
