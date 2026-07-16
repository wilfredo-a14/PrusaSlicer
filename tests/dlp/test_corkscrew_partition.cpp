#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/DLPCorkscrew.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/ExPolygon.hpp"

#include <cmath>
#include <numeric>
#include <vector>

using namespace Slic3r;
using namespace Slic3r::dlp;
using Catch::Approx;

namespace {

ExPolygon make_rect_mm(double x0, double y0, double x1, double y1)
{
    Polygon contour{
        { scaled(x0), scaled(y0) },
        { scaled(x1), scaled(y0) },
        { scaled(x1), scaled(y1) },
        { scaled(x0), scaled(y1) },
    };
    return ExPolygon{contour};
}

ExPolygons make_square_layer_mm(double size_mm)
{
    return ExPolygons{make_rect_mm(0., 0., size_mm, size_mm)};
}

double sum_box_areas(const std::vector<CorkscrewBoxSlice> &boxes)
{
    double sum = 0.;
    for (const CorkscrewBoxSlice &box : boxes)
        sum += box.area;
    return sum;
}

} // namespace

TEST_CASE("Corkscrew partition rejects invalid inputs", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);

    SECTION("box_count < 1 returns empty") {
        CHECK(partition_layer_into_boxes(layer, 0).empty());
        CHECK(partition_layer_into_boxes(layer, -3).empty());
    }

    SECTION("empty layer returns empty") {
        CHECK(partition_layer_into_boxes({}, 4).empty());
    }
}

TEST_CASE("Corkscrew partition produces expected box count", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);

    for (int box_count : {1, 2, 3, 4, 8, 16, 32, 64}) {
        INFO("box_count=" << box_count);
        const auto boxes = partition_layer_into_boxes(layer, box_count);
        REQUIRE(static_cast<int>(boxes.size()) == box_count);
        for (int i = 0; i < box_count; ++i)
            CHECK(boxes[i].index == i);
    }
}

TEST_CASE("Corkscrew partition preserves total area within tolerance", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);
    const double total_area = area(layer);

    for (int box_count : {1, 2, 4, 5, 7, 8, 16}) {
        INFO("box_count=" << box_count);
        const auto boxes = partition_layer_into_boxes(layer, box_count);
        REQUIRE_FALSE(boxes.empty());

        const double partitioned = sum_box_areas(boxes);
        const double rel_error = std::abs(partitioned - total_area) / total_area;
        CHECK(rel_error < 0.01);
    }
}

TEST_CASE("Corkscrew boxes are contiguous vertical strips covering the bbox", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);
    const BoundingBox bbox = get_extents(layer);
    constexpr int box_count = 4;

    const auto boxes = partition_layer_into_boxes(layer, box_count);
    REQUIRE(boxes.size() == box_count);

    CHECK(boxes.front().region.min.x() == bbox.min.x());
    CHECK(boxes.back().region.max.x() == bbox.max.x());

    for (int i = 0; i < box_count; ++i) {
        CHECK(boxes[i].region.min.y() == bbox.min.y());
        CHECK(boxes[i].region.max.y() == bbox.max.y());
        if (i + 1 < box_count)
            CHECK(boxes[i].region.max.x() == boxes[i + 1].region.min.x());
    }
}

TEST_CASE("Corkscrew boxes have approximately equal widths", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);
    constexpr int box_count = 8;
    const auto boxes = partition_layer_into_boxes(layer, box_count);
    REQUIRE(boxes.size() == box_count);

    const coord_t total_width = get_extents(layer).max.x() - get_extents(layer).min.x();
    const coord_t expected = total_width / box_count;

    for (const CorkscrewBoxSlice &box : boxes) {
        const coord_t width = box.region.max.x() - box.region.min.x();
        // Integer division may leave remainder on the last box.
        CHECK(std::abs(width - expected) <= expected);
        CHECK(width > 0);
    }
}

TEST_CASE("Corkscrew partition of a rectangle is left-to-right ordered", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(100.);
    const auto boxes = partition_layer_into_boxes(layer, 5);
    REQUIRE(boxes.size() == 5);

    for (size_t i = 1; i < boxes.size(); ++i)
        CHECK(boxes[i].region.min.x() >= boxes[i - 1].region.min.x());
}

TEST_CASE("Corkscrew partition with a single box returns the full layer", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(25.);
    const auto boxes = partition_layer_into_boxes(layer, 1);
    REQUIRE(boxes.size() == 1);
    CHECK(boxes[0].index == 0);
    CHECK(boxes[0].area == Approx(area(layer)).margin(area(layer) * 0.01));
    CHECK_FALSE(boxes[0].polygons.empty());
}

TEST_CASE("Corkscrew partition handles non-square rectangle", "[dlp][corkscrew]")
{
    const ExPolygons layer{make_rect_mm(10., 0., 50., 20.)}; // 40 x 20 mm
    const double total_area = area(layer);

    const auto boxes = partition_layer_into_boxes(layer, 4);
    REQUIRE(boxes.size() == 4);

    const double partitioned = sum_box_areas(boxes);
    CHECK(std::abs(partitioned - total_area) / total_area < 0.01);

    // Each strip should be about 10 mm wide.
    for (const CorkscrewBoxSlice &box : boxes) {
        const double width_mm = unscale<double>(box.region.max.x() - box.region.min.x());
        CHECK(width_mm == Approx(10.0).margin(0.01));
    }
}

TEST_CASE("Corkscrew partition of offset rectangle keeps absolute coordinates", "[dlp][corkscrew]")
{
    const ExPolygons layer{make_rect_mm(100., 50., 140., 90.)};
    const auto boxes = partition_layer_into_boxes(layer, 2);
    REQUIRE(boxes.size() == 2);

    CHECK(unscale<double>(boxes[0].region.min.x()) == Approx(100.0));
    CHECK(unscale<double>(boxes[1].region.max.x()) == Approx(140.0));
    CHECK(unscale<double>(boxes[0].region.min.y()) == Approx(50.0));
    CHECK(unscale<double>(boxes[0].region.max.y()) == Approx(90.0));
}

TEST_CASE("Corkscrew partition of two disjoint rectangles still covers total area", "[dlp][corkscrew]")
{
    ExPolygons layer;
    layer.emplace_back(make_rect_mm(0., 0., 10., 10.));
    layer.emplace_back(make_rect_mm(20., 0., 30., 10.)); // gap in the middle

    const double total_area = area(layer);
    const auto boxes = partition_layer_into_boxes(layer, 3);
    REQUIRE(boxes.size() == 3);

    const double partitioned = sum_box_areas(boxes);
    CHECK(std::abs(partitioned - total_area) / total_area < 0.01);

    // Middle strip may be empty or nearly empty because of the gap.
    CHECK(boxes[1].area <= boxes[0].area + boxes[2].area);
}

TEST_CASE("Corkscrew partition with a hole preserves area", "[dlp][corkscrew]")
{
    Polygon outer{
        { scaled(0.), scaled(0.) },
        { scaled(40.), scaled(0.) },
        { scaled(40.), scaled(40.) },
        { scaled(0.), scaled(40.) },
    };
    // Hole must be clockwise for ExPolygon validity.
    Polygon hole{
        { scaled(15.), scaled(15.) },
        { scaled(15.), scaled(25.) },
        { scaled(25.), scaled(25.) },
        { scaled(25.), scaled(15.) },
    };
    ExPolygons layer{ExPolygon{outer, {hole}}};
    REQUIRE(layer.front().is_valid());

    const double total_area = area(layer);
    const auto boxes = partition_layer_into_boxes(layer, 4);
    REQUIRE(boxes.size() == 4);

    const double partitioned = sum_box_areas(boxes);
    CHECK(std::abs(partitioned - total_area) / total_area < 0.01);
}

TEST_CASE("Corkscrew box areas are non-negative", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(30.);
    const auto boxes = partition_layer_into_boxes(layer, 6);
    REQUIRE(boxes.size() == 6);

    for (const CorkscrewBoxSlice &box : boxes)
        CHECK(box.area >= 0.);
}

TEST_CASE("Corkscrew partition is deterministic", "[dlp][corkscrew]")
{
    const ExPolygons layer = make_square_layer_mm(40.);
    const auto a = partition_layer_into_boxes(layer, 4);
    const auto b = partition_layer_into_boxes(layer, 4);
    REQUIRE(a.size() == b.size());

    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].index == b[i].index);
        CHECK(a[i].region.min == b[i].region.min);
        CHECK(a[i].region.max == b[i].region.max);
        CHECK(a[i].area == Approx(b[i].area));
    }
}
