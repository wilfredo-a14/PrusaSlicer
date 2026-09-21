#include "MultiBoxExporter.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

#include <boost/log/trivial.hpp>

#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/format.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/SLA/RasterBase.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r::MultiBox {

namespace {

constexpr double kPi = 3.14159265358979323846;

ExPolygon make_box_expolygon(double min_x_mm, double min_y_mm, double max_x_mm, double max_y_mm)
{
    Points pts;
    pts.emplace_back(scaled<double>(min_x_mm), scaled<double>(min_y_mm));
    pts.emplace_back(scaled<double>(max_x_mm), scaled<double>(min_y_mm));
    pts.emplace_back(scaled<double>(max_x_mm), scaled<double>(max_y_mm));
    pts.emplace_back(scaled<double>(min_x_mm), scaled<double>(max_y_mm));
    return ExPolygon(std::move(pts));
}

ExPolygons translate_expolygons_mm(const ExPolygons &src, double dx_mm, double dy_mm)
{
    ExPolygons out = src;
    translate(out, Point(scaled<double>(dx_mm), scaled<double>(dy_mm)));
    return out;
}

BoundingBox get_layers_bbox(const SLAPrint &print)
{
    BoundingBox bbox;
    for (const SLAPrint::PrintLayer &layer : print.print_layers())
        bbox.merge(get_extents(layer.transformed_slices()));
    return bbox;
}

bool write_png(const std::filesystem::path &path, const sla::EncodedRaster &encoded)
{
    std::ofstream outfile(path, std::ios::binary);
    if (!outfile)
        return false;
    outfile.write(static_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    return outfile.good();
}

sla::EncodedRaster rasterize_box_slice(
    const ExPolygons &layer_slices_mm,
    const BoxOverlay   &box,
    const SLAPrintObjectConfig &config)
{
    const double pixel_scale_mm = config.multibox_pixel_scale_um.value / 1000.0;
    const double box_min_x      = box.center_x_mm - box.width_mm / 2.;
    const double box_min_y      = box.center_y_mm - box.height_mm / 2.;
    const double box_max_x      = box.center_x_mm + box.width_mm / 2.;
    const double box_max_y      = box.center_y_mm + box.height_mm / 2.;

    const ExPolygon box_poly = make_box_expolygon(
        box_min_x - pixel_scale_mm,
        box_min_y - pixel_scale_mm,
        box_max_x + pixel_scale_mm,
        box_max_y + pixel_scale_mm);

    ExPolygons clipped = intersection_ex(layer_slices_mm, ExPolygons{ box_poly });
    ExPolygons local_slices = translate_expolygons_mm(clipped, -box_min_x, -box_min_y);

    const size_t width_px  = static_cast<size_t>(config.multibox_box_width_px.value);
    const size_t height_px = static_cast<size_t>(config.multibox_box_height_px.value);

    sla::Resolution res(width_px, height_px);
    sla::PixelDim   pxdim(box.width_mm / width_px, box.height_mm / height_px);

    auto raster = sla::create_raster_grayscale_aa(res, pxdim, 1.0, sla::RasterBase::Trafo{});
    for (const ExPolygon &poly : local_slices)
        raster->draw(poly);

    return raster->encode(sla::PNGRasterEncoder{});
}

} // namespace

std::vector<BoxOverlay> compute_static_box_overlays(const SLAPrintObjectConfig &config)
{
    const int    num_boxes           = config.multibox_num_boxes.value;
    const double angle_between_boxes = config.multibox_angle_between_boxes.value;
    const double starting_angle      = config.multibox_starting_angle.value;
    const double pixel_scale_mm      = config.multibox_pixel_scale_um.value / 1000.0;
    const double box_width_mm        = config.multibox_box_width_px.value * pixel_scale_mm;
    const double box_height_mm       = config.multibox_box_height_px.value * pixel_scale_mm;
    const double x_center_mm         = config.multibox_fab_width_mm.value / 2.;
    const double y_center_mm         = config.multibox_fab_height_mm.value / 2.;
    const double box_radius_mm       = config.multibox_radius_mm.value;

    std::vector<BoxOverlay> overlays;
    overlays.reserve(static_cast<size_t>(num_boxes));

    for (int i = 0; i < num_boxes; ++i) {
        const double angle_deg = starting_angle + i * angle_between_boxes;
        const double angle_rad = angle_deg * kPi / 180.0;

        BoxOverlay box;
        box.center_x_mm = x_center_mm + box_radius_mm * std::cos(angle_rad);
        box.center_y_mm = y_center_mm + box_radius_mm * std::sin(angle_rad);
        box.width_mm    = box_width_mm;
        box.height_mm   = box_height_mm;
        box.angle_deg   = angle_deg;
        overlays.push_back(box);
    }

    return overlays;
}

ExportResult export_print(
    const SLAPrint   &print,
    const std::string &output_dir,
    const std::string &project_name,
    ProgressFn        progress)
{
    ExportResult result;

    if (print.print_layers().empty()) {
        result.error = "The print has no sliced layers.";
        return result;
    }

    const SLAPrintObjectConfig &config = print.default_object_config();
    const auto                  boxes  = compute_static_box_overlays(config);
    if (boxes.empty()) {
        result.error = "No projection boxes configured.";
        return result;
    }

    const BoundingBox model_bbox = get_layers_bbox(print);
    if (!model_bbox.defined) {
        result.error = "Could not determine model bounds.";
        return result;
    }

    const double model_center_x_mm = unscale<double>(0.5 * (model_bbox.min.x() + model_bbox.max.x()));
    const double model_center_y_mm = unscale<double>(0.5 * (model_bbox.min.y() + model_bbox.max.y()));
    const double fab_center_x_mm   = config.multibox_fab_width_mm.value / 2.;
    const double fab_center_y_mm   = config.multibox_fab_height_mm.value / 2.;
    const double translate_x_mm    = fab_center_x_mm - model_center_x_mm;
    const double translate_y_mm    = fab_center_y_mm - model_center_y_mm;

    std::error_code ec;
    const std::filesystem::path out_dir(output_dir);
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        result.error = "Could not create output directory: " + ec.message();
        return result;
    }

    result.boxes_per_layer = boxes.size();

    const size_t total_layers = print.print_layers().size();
    size_t layer_index = 0;
    for (const SLAPrint::PrintLayer &layer : print.print_layers()) {
        ++layer_index;
        if (progress && !progress(layer_index, total_layers)) {
            result.error = "Export canceled.";
            return result;
        }

        ExPolygons centered_slices = translate_expolygons_mm(layer.transformed_slices(), translate_x_mm, translate_y_mm);

        size_t box_index = 0;
        for (const BoxOverlay &box : boxes) {
            ++box_index;
            const std::string filename = project_name + "_"
                + Slic3r::format("%04d_%04d.png", layer_index, box_index);
            const std::filesystem::path filepath = out_dir / filename;

            try {
                sla::EncodedRaster png = rasterize_box_slice(centered_slices, box, config);
                if (!write_png(filepath, png)) {
                    result.error = "Failed to write " + filepath.string();
                    return result;
                }
                ++result.files_written;
            } catch (const std::exception &ex) {
                result.error = std::string("Rasterization failed: ") + ex.what();
                return result;
            }
        }
    }

    result.layers_exported = layer_index;
    BOOST_LOG_TRIVIAL(info) << "Multi-box export wrote " << result.files_written
        << " PNG files (" << result.layers_exported << " layers x "
        << result.boxes_per_layer << " boxes) to " << output_dir;

    return result;
}

} // namespace Slic3r::MultiBox
