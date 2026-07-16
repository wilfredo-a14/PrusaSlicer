#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "libslic3r/DLPConfig.hpp"
#include "libslic3r/PrintConfig.hpp"

using namespace Slic3r;
using namespace Slic3r::dlp;
using Catch::Approx;

TEST_CASE("DLP display defaults are hard-coded 2560x1600", "[dlp][config]")
{
    CHECK(DISPLAY_PIXELS_X == 2560);
    CHECK(DISPLAY_PIXELS_Y == 1600);
    CHECK(DISPLAY_WIDTH_MM == Approx(120.0));
    CHECK(DISPLAY_HEIGHT_MM == Approx(120.0 * 1600.0 / 2560.0));
    CHECK(DISPLAY_HEIGHT_MM == Approx(75.0));
}

TEST_CASE("DLP display aspect ratio matches pixel aspect ratio", "[dlp][config]")
{
    const double pixel_aspect = static_cast<double>(DISPLAY_PIXELS_X) / DISPLAY_PIXELS_Y;
    const double mm_aspect    = DISPLAY_WIDTH_MM / DISPLAY_HEIGHT_MM;
    CHECK(pixel_aspect == Approx(mm_aspect).margin(1e-9));
}

TEST_CASE("SLA printer config defaults use DLP display resolution", "[dlp][config]")
{
    SLAPrinterConfig cfg;
    CHECK(cfg.display_pixels_x.getInt() == DISPLAY_PIXELS_X);
    CHECK(cfg.display_pixels_y.getInt() == DISPLAY_PIXELS_Y);
    CHECK(cfg.display_width.getFloat() == Approx(DISPLAY_WIDTH_MM));
    CHECK(cfg.display_height.getFloat() == Approx(DISPLAY_HEIGHT_MM));
}

TEST_CASE("Corkscrew options exist on SLA print object config", "[dlp][config][corkscrew]")
{
    SLAPrintObjectConfig cfg;
    REQUIRE(cfg.has("corkscrew_enable"));
    REQUIRE(cfg.has("corkscrew_box_count"));

    CHECK(cfg.corkscrew_enable.getBool() == false);
    CHECK(cfg.corkscrew_box_count.getInt() == 4);
}

TEST_CASE("Corkscrew options are present in the global print config definition", "[dlp][config]")
{
    const ConfigOptionDef *enable = print_config_def.get("corkscrew_enable");
    const ConfigOptionDef *boxes  = print_config_def.get("corkscrew_box_count");

    REQUIRE(enable != nullptr);
    REQUIRE(boxes != nullptr);

    CHECK(enable->type == coBool);
    CHECK(boxes->type == coInt);
    CHECK(boxes->min == Approx(1.));
    CHECK(boxes->max == Approx(64.));
}

TEST_CASE("Corkscrew box count can be set within allowed range", "[dlp][config][corkscrew]")
{
    SLAPrintObjectConfig cfg;

    for (int n : {1, 2, 4, 8, 16, 32, 64}) {
        cfg.corkscrew_box_count.value = n;
        CHECK(cfg.corkscrew_box_count.getInt() == n);
    }

    cfg.corkscrew_enable.value = true;
    CHECK(cfg.corkscrew_enable.getBool());
    cfg.corkscrew_enable.value = false;
    CHECK_FALSE(cfg.corkscrew_enable.getBool());
}

TEST_CASE("DynamicPrintConfig can deserialize corkscrew options", "[dlp][config]")
{
    DynamicPrintConfig cfg;
    cfg.set_deserialize_strict("corkscrew_enable", "1");
    cfg.set_deserialize_strict("corkscrew_box_count", "8");

    CHECK(cfg.opt_bool("corkscrew_enable"));
    CHECK(cfg.opt_int("corkscrew_box_count") == 8);

    cfg.set_deserialize_strict("corkscrew_enable", "0");
    CHECK_FALSE(cfg.opt_bool("corkscrew_enable"));
}

TEST_CASE("CLI misc config defines export_png_dir", "[dlp][config][cli]")
{
    const ConfigOptionDef *def = cli_misc_config_def.get("export_png_dir");
    REQUIRE(def != nullptr);
    CHECK(def->type == coString);
    CHECK(def->cli == "export-png-dir");
}
