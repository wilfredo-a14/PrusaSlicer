///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPExport.hpp"

#include "DLPConfig.hpp"
#include "DLPCorkscrew.hpp"
#include "DLPDebugLog.hpp"
#include "SLAPrint.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/Execution/Execution.hpp"
#include "libslic3r/Execution/ExecutionTBB.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/SLA/RasterBase.hpp"

#include <boost/log/trivial.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>

#include "format.hpp"

namespace Slic3r::dlp {

void export_png_layers(SLAPrint &print,
                       const ExecutionTBB &ex_tbb,
                       const std::function<bool()> &canceled)
{
    if (print.png_export_dir().empty()) {
        debug_log("PNG export: skipped — png_export_dir is empty");
        return;
    }

    const bool corkscrew_enabled = print.default_object_config().corkscrew_enable.getBool();
    const int  box_count         = print.default_object_config().corkscrew_box_count.getInt();

    debug_log(Slic3r::format(
        "PNG export: begin dir=%1% corkscrew_enable=%2% corkscrew_box_count=%3% "
        "hardcoded_resolution=%4%x%5%",
        print.png_export_dir(),
        corkscrew_enabled,
        box_count,
        DISPLAY_PIXELS_X,
        DISPLAY_PIXELS_Y));

    BOOST_LOG_TRIVIAL(debug) << "SLA rasterize: PNG export requested to "
        << print.png_export_dir();
    try {
        namespace fs = std::filesystem;
        fs::path output_dir = print.png_export_dir();
        fs::create_directories(output_dir);

        const auto &pcfg = print.printer_config();
        double disp_w = pcfg.display_width.getFloat();   // mm
        double disp_h = pcfg.display_height.getFloat();   // mm

        const size_t target_w = static_cast<size_t>(DISPLAY_PIXELS_X);
        const size_t target_h = static_cast<size_t>(DISPLAY_PIXELS_Y);

        // Fixed pixel dimensions from display size (scale doesn't change with model)
        sla::Resolution res{target_w, target_h};
        sla::PixelDim   pxdim{disp_w / target_w, disp_h / target_h};

        debug_log(Slic3r::format(
            "PNG export: display_mm=%1%x%2% pixel_pitch_mm=%3%x%4%",
            disp_w,
            disp_h,
            pxdim.w_mm,
            pxdim.h_mm));

        // Compute model bounding box to find where it actually is in coord space
        BoundingBox bbox;
        for (const SLAPrint::PrintLayer& pl : print.print_layers()) {
            for (const ExPolygon& poly : pl.transformed_slices())
                bbox.merge(get_extents(poly));
        }

        BOOST_LOG_TRIVIAL(debug) << "SLA rasterize: PNG bbox "
            << (bbox.defined ? "defined" : "undefined")
            << ", resolution " << target_w << "x" << target_h
            << ", corkscrew_enable=" << corkscrew_enabled
            << ", corkscrew_box_count=" << box_count;

        debug_log(Slic3r::format(
            "PNG export: bbox %1%, resolution %2%x%3%, corkscrew_enable=%4%, box_count=%5%",
            bbox.defined ? "defined" : "undefined",
            target_w,
            target_h,
            corkscrew_enabled,
            box_count));

        if (!bbox.defined) {
            debug_log("PNG export: abort — model bounding box undefined");
            BOOST_LOG_TRIVIAL(warning) << "SLA rasterize: skipping PNG export — model bounding box is undefined";
            return;
        }

        debug_log(Slic3r::format(
            "PNG export: model_bbox_mm=[%1%,%2%]x[%3%,%4%]",
            unscale<double>(bbox.min.x()),
            unscale<double>(bbox.max.x()),
            unscale<double>(bbox.min.y()),
            unscale<double>(bbox.max.y())));

        // Center the model in the image:
        // We want model_center to map to image_center (target_w/2, target_h/2)
        // pixel = (model_coord + center) * SCALING_FACTOR / pxdim
        // Solving: center = scaled(disp/2) - model_center
        coord_t model_cx = (bbox.min.x() + bbox.max.x()) / 2;
        coord_t model_cy = (bbox.min.y() + bbox.max.y()) / 2;

        sla::RasterBase::Trafo tr;
        tr.center_x = scaled(disp_w / 2.0) - model_cx;
        tr.center_y = scaled(disp_h / 2.0) - model_cy;

        debug_log(Slic3r::format(
            "PNG export: transform center_x=%1% center_y=%2% model_center_mm=(%3%,%4%)",
            tr.center_x,
            tr.center_y,
            unscale<double>(model_cx),
            unscale<double>(model_cy)));

        const size_t num_layers = print.print_layers().size();
        const size_t png_count  = corkscrew_enabled && box_count > 0 ?
            num_layers * static_cast<size_t>(box_count) : num_layers;

        debug_log(Slic3r::format(
            "PNG export: rasterizing layers=%1% png_files=%2% mode=%3%",
            num_layers,
            png_count,
            corkscrew_enabled && box_count > 0 ? "corkscrew_boxes" : "full_layer"));

        std::vector<sla::EncodedRaster> png_layers(png_count);

        if (corkscrew_enabled && box_count > 0) {
            execution::for_each(
                ex_tbb, size_t(0), png_count,
                [&print, &png_layers, &res, &pxdim, &tr, box_count, &canceled](size_t idx) {
                    if (canceled()) return;

                    const size_t layer_idx = idx / static_cast<size_t>(box_count);
                    const size_t box_idx   = idx % static_cast<size_t>(box_count);

                    auto raster = sla::create_raster_grayscale_aa(res, pxdim, 1.0, tr);

                    const SLAPrint::PrintLayer& pl = print.print_layers()[layer_idx];
                    const auto boxes = partition_layer_into_boxes(pl.transformed_slices(), box_count);
                    if (box_idx < boxes.size())
                        for (const ExPolygon& poly : boxes[box_idx].polygons)
                            raster->draw(poly);

                    png_layers[idx] = raster->encode(sla::PNGRasterEncoder{});
                },
                execution::max_concurrency(ex_tbb));
        } else {
            execution::for_each(
                ex_tbb, size_t(0), num_layers,
                [&print, &png_layers, &res, &pxdim, &tr, &canceled](size_t idx) {
                    if (canceled()) return;

                    auto raster = sla::create_raster_grayscale_aa(res, pxdim, 1.0, tr);

                    const SLAPrint::PrintLayer& pl = print.print_layers()[idx];
                    for (const ExPolygon& poly : pl.transformed_slices())
                        raster->draw(poly);

                    png_layers[idx] = raster->encode(sla::PNGRasterEncoder{});
                },
                execution::max_concurrency(ex_tbb));
        }

        // Write PNG files to disk.
        size_t wrote = 0;
        size_t failed = 0;
        for (size_t i = 0; i < png_count; ++i) {
            std::string filename;
            if (corkscrew_enabled && box_count > 0) {
                const size_t layer_idx = i / static_cast<size_t>(box_count);
                const size_t box_idx   = i % static_cast<size_t>(box_count);
                filename = Slic3r::format("layer_%04d_box_%02d.png", layer_idx, box_idx);
            } else {
                filename = Slic3r::format("layer_%04d.png", i);
            }
            fs::path filepath = output_dir / filename;
            std::ofstream outfile(filepath, std::ios::binary);
            if (outfile) {
                const auto& enc = png_layers[i];
                outfile.write(static_cast<const char*>(enc.data()), enc.size());
                ++wrote;
            } else {
                ++failed;
                debug_log(Slic3r::format(
                    "PNG export: FAILED write index=%1% path=%2%",
                    i,
                    filepath.string()));
                BOOST_LOG_TRIVIAL(warning) << "SLA rasterize: failed to write PNG layer "
                    << i << " to " << filepath.string();
            }
        }

        debug_log(Slic3r::format(
            "PNG export: wrote %1% files (%2% failed) for %3% layers%4% dir=%5%",
            wrote,
            failed,
            num_layers,
            corkscrew_enabled && box_count > 0 ?
                Slic3r::format(" using %1% corkscrew boxes", box_count) : "",
            output_dir.string()));

        BOOST_LOG_TRIVIAL(info) << "Exported " << wrote
            << " PNG files for " << num_layers << " layers (" << target_w << "x" << target_h
            << ", display " << disp_w << "x" << disp_h << "mm"
            << ") to: " << output_dir.string();
    } catch (const std::exception& e) {
        debug_log(Slic3r::format("PNG export: exception — %1%", e.what()));
        BOOST_LOG_TRIVIAL(warning) << "PNG layer export failed: " << e.what();
    }
}

} // namespace Slic3r::dlp
