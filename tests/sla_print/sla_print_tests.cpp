#include <unordered_map>
#include <random>
#include <numeric>
#include <cstdint>

#include "sla_test_utils.hpp"

#include <libslic3r/TriangleMeshSlicer.hpp>
#include <libslic3r/SLA/SupportTreeMesher.hpp>
#include <libslic3r/BranchingTree/PointCloud.hpp>
#include <libslic3r/Model.hpp>

namespace {

const char *const BELOW_PAD_TEST_OBJECTS[] = {
    "20mm_cube.obj",
    "V.obj",
};

const char *const AROUND_PAD_TEST_OBJECTS[] = {
    "20mm_cube.obj",
    "V.obj",
    "frog_legs.obj",
    "cube_with_concave_hole_enlarged.obj",
};

const char *const SUPPORT_TEST_MODELS[] = {
    "cube_with_concave_hole_enlarged_standing.obj",
    "A_upsidedown.obj",
    "extruder_idler.obj"
};

} // namespace

TEST_CASE("SLA slice grid uses fixed increments without a partial final layer", "[sla_print][slice_grid]")
{
    constexpr double layer_height         = 0.3;
    constexpr double initial_layer_height = 0.3;
    constexpr double model_height         = 19.84;
    constexpr double expected_last_slice  = 19.8;

    Model model;
    ModelObject *object = model.add_object();
    object->name = "grounded cube";
    object->add_instance();
    object->add_volume(make_cube(1., 1., model_height));

    SLAFullPrintConfig full_config;
    full_config.printer_technology.setInt(ptSLA);
    full_config.set("supports_enable", false);
    full_config.set("pad_enable", false);
    full_config.set("layer_height", layer_height);
    full_config.set("initial_layer_height", initial_layer_height);

    DynamicPrintConfig config;
    config.apply(full_config);

    SLAPrint print;
    print.set_status_callback([](const PrintBase::SlicingStatus &) {});
    print.apply(model, config);
    print.process();

    REQUIRE(print.objects().size() == 1);
    const auto &slice_index = print.objects().front()->get_slice_index();
    REQUIRE(slice_index.size() > 1);

    CHECK(slice_index[0].print_level() == scaled(initial_layer_height));
    CHECK(slice_index[0].slice_level() == Approx(0.).margin(std::numeric_limits<float>::epsilon()));
    CHECK_FALSE(slice_index[0].get_slice(soModel).empty());
    CHECK(slice_index[1].print_level() == scaled(initial_layer_height + layer_height));
    CHECK(slice_index[1].slice_level() == Approx(initial_layer_height));
    REQUIRE(slice_index.size() == 67);
    CHECK(unscaled<double>(slice_index.back().print_level()) ==
          Approx(expected_last_slice + layer_height).margin(1e-5));
    CHECK(slice_index.back().slice_level() == Approx(expected_last_slice).margin(1e-6));
    CHECK(slice_index.back().slice_level() < model_height);
    CHECK(slice_index.back().layer_height() == Approx(layer_height).margin(1e-6));
    CHECK_FALSE(slice_index.back().get_slice(soModel).empty());

    for (size_t index = 2; index < slice_index.size(); ++index)
        CHECK(slice_index[index].slice_level() - slice_index[index - 1].slice_level() ==
              Approx(layer_height).margin(1e-6));
}

TEST_CASE("Flat pad geometry is valid", "[SLASupportGeneration]") {
    sla::PadConfig padcfg;
    
    // Disable wings
    padcfg.wall_height_mm = .0;
    
    for (auto &fname : BELOW_PAD_TEST_OBJECTS) test_pad(fname, padcfg);
}

TEST_CASE("WingedPadGeometryIsValid", "[SLASupportGeneration]") {
    sla::PadConfig padcfg;
    
    // Add some wings to the pad to test the cavity
    padcfg.wall_height_mm = 1.;
    
    for (auto &fname : BELOW_PAD_TEST_OBJECTS) test_pad(fname, padcfg);
}

TEST_CASE("FlatPadAroundObjectIsValid", "[SLASupportGeneration]") {
    sla::PadConfig padcfg;
    
    // Add some wings to the pad to test the cavity
    padcfg.wall_height_mm = 0.;
    // padcfg.embed_object.stick_stride_mm = 0.;
    padcfg.embed_object.enabled = true;
    padcfg.embed_object.everywhere = true;
    
    for (auto &fname : AROUND_PAD_TEST_OBJECTS) test_pad(fname, padcfg);
}

TEST_CASE("WingedPadAroundObjectIsValid", "[SLASupportGeneration]") {
    sla::PadConfig padcfg;
    
    // Add some wings to the pad to test the cavity
    padcfg.wall_height_mm = 1.;
    padcfg.embed_object.enabled = true;
    padcfg.embed_object.everywhere = true;
    
    for (auto &fname : AROUND_PAD_TEST_OBJECTS) test_pad(fname, padcfg);
}

TEST_CASE("DefaultSupports::ElevatedSupportGeometryIsValid", "[SLASupportGeneration]") {
    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 10.;
    
    for (auto fname : SUPPORT_TEST_MODELS) test_supports(fname, supportcfg);
}

TEST_CASE("DefaultSupports::FloorSupportGeometryIsValid", "[SLASupportGeneration]") {
    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 0;
    
    for (auto &fname: SUPPORT_TEST_MODELS) test_supports(fname, supportcfg);
}

TEST_CASE("DefaultSupports::ElevatedSupportsDoNotPierceModel", "[SLASupportGeneration]") {
    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 10.;

    for (auto fname : SUPPORT_TEST_MODELS)
        test_support_model_collision(fname, supportcfg);
}

TEST_CASE("DefaultSupports::FloorSupportsDoNotPierceModel", "[SLASupportGeneration]") {
    
    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 0;
    
    for (auto fname : SUPPORT_TEST_MODELS)
        test_support_model_collision(fname, supportcfg);
}

//TEST_CASE("BranchingSupports::ElevatedSupportGeometryIsValid", "[SLASupportGeneration][Branching]") {
//    sla::SupportTreeConfig supportcfg;
//    supportcfg.object_elevation_mm = 10.;
//    supportcfg.tree_type = sla::SupportTreeType::Branching;

//    for (auto fname : SUPPORT_TEST_MODELS) test_supports(fname, supportcfg);
//}

//TEST_CASE("BranchingSupports::FloorSupportGeometryIsValid", "[SLASupportGeneration][Branching]") {
//    sla::SupportTreeConfig supportcfg;
//    supportcfg.object_elevation_mm = 0;
//    supportcfg.tree_type = sla::SupportTreeType::Branching;

//    for (auto &fname: SUPPORT_TEST_MODELS) test_supports(fname, supportcfg);
//}


TEST_CASE("BranchingSupports::ElevatedSupportsDoNotPierceModel", "[SLASupportGeneration][Branching]") {

    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 10.;
    supportcfg.tree_type = sla::SupportTreeType::Branching;

    for (auto fname : SUPPORT_TEST_MODELS)
        test_support_model_collision(fname, supportcfg);
}

TEST_CASE("BranchingSupports::FloorSupportsDoNotPierceModel", "[SLASupportGeneration][Branching]") {

    sla::SupportTreeConfig supportcfg;
    supportcfg.object_elevation_mm = 0;
    supportcfg.tree_type = sla::SupportTreeType::Branching;

    for (auto fname : SUPPORT_TEST_MODELS)
        test_support_model_collision(fname, supportcfg);
}

TEST_CASE("InitializedRasterShouldBeNONEmpty", "[SLARasterOutput]") {
    // Default Prusa SL1 display parameters
    sla::Resolution res{2560, 1440};
    sla::PixelDim   pixdim{120. / res.width_px, 68. / res.height_px};
    
    sla::RasterGrayscaleAAGammaPower raster(res, pixdim, {}, 1.);
    REQUIRE(raster.resolution().width_px == res.width_px);
    REQUIRE(raster.resolution().height_px == res.height_px);
    REQUIRE(raster.pixel_dimensions().w_mm == Approx(pixdim.w_mm));
    REQUIRE(raster.pixel_dimensions().h_mm == Approx(pixdim.h_mm));
}

TEST_CASE("MirroringShouldBeCorrect", "[SLARasterOutput]") {
    sla::RasterBase::TMirroring mirrorings[] = {sla::RasterBase::NoMirror,
                                                sla::RasterBase::MirrorX,
                                                sla::RasterBase::MirrorY,
                                                sla::RasterBase::MirrorXY};

    sla::RasterBase::Orientation orientations[] =
        {sla::RasterBase::roLandscape, sla::RasterBase::roPortrait};
    
    for (auto orientation : orientations)
        for (auto &mirror : mirrorings)
            check_raster_transformations(orientation, mirror);
}


TEST_CASE("RasterizedPolygonAreaShouldMatch", "[SLARasterOutput]") {
    double disp_w = 120., disp_h = 68.;
    sla::Resolution res{2560, 1440};
    sla::PixelDim pixdim{disp_w / res.width_px, disp_h / res.height_px};
    
    double gamma = 1.;
    sla::RasterGrayscaleAAGammaPower raster(res, pixdim, {}, gamma);
    auto bb = BoundingBox({0, 0}, {scaled(disp_w), scaled(disp_h)});
    
    ExPolygon poly = square_with_hole(10.);
    poly.translate(bb.center().x(), bb.center().y());
    raster.draw(poly);
    
    double a = poly.area() / (scaled<double>(1.) * scaled(1.));
    double ra = raster_white_area(raster);
    double diff = std::abs(a - ra);
    
    REQUIRE(diff <= predict_error(poly, pixdim));
    
    raster.clear();
    poly = square_with_hole(60.);
    poly.translate(bb.center().x(), bb.center().y());
    raster.draw(poly);
    
    a = poly.area() / (scaled<double>(1.) * scaled(1.));
    ra = raster_white_area(raster);
    diff = std::abs(a - ra);
    
    REQUIRE(diff <= predict_error(poly, pixdim));
    
    sla::RasterGrayscaleAA raster0(res, pixdim, {}, [](double) { return 0.; });
    REQUIRE(raster_pxsum(raster0) == 0);
    
    raster0.draw(poly);
    ra = raster_white_area(raster);
    REQUIRE(raster_pxsum(raster0) == 0);
}


TEST_CASE("halfcone test", "[halfcone]") {
    sla::DiffBridge br{Vec3d{1., 1., 1.}, Vec3d{10., 10., 10.}, 0.25, 0.5};

    indexed_triangle_set m = sla::get_mesh(br, 45);

    its_merge_vertices(m);
    its_write_obj(m, "Halfcone.obj");
}

TEST_CASE("Test concurrency")
{
    std::vector<double> vals = grid(0., 100., 10.);

    double ref = std::accumulate(vals.begin(), vals.end(), 0.);

    double s = execution::accumulate(ex_tbb, vals.begin(), vals.end(), 0.);

    REQUIRE(s == Approx(ref));
}
