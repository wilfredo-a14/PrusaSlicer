#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "libslic3r/DLPConfig.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>

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

TEST_CASE("Legacy DLP settings are backed by native SLA preset configs", "[dlp][config][gui]")
{
    SLAPrintObjectConfig print;
    REQUIRE(print.has("dlp_stage_velocity"));
    REQUIRE(print.has("dlp_dynamic_print_script"));
    CHECK(print.dlp_stage_velocity.getFloat() == Approx(10.));
    CHECK_FALSE(print.dlp_dynamic_print_script.getBool());

    SLAMaterialConfig material;
    REQUIRE(material.has("dlp_uv_intensity"));
    REQUIRE(material.has("dlp_injection_rate"));
    CHECK(material.dlp_uv_intensity.getInt() == 12);
    CHECK(material.dlp_injection_rate.getFloat() == Approx(5.));

    SLAPrinterConfig printer;
    REQUIRE(printer.has("dlp_projection_mode"));
    REQUIRE(printer.has("dlp_stage_serial_port"));
    CHECK(printer.dlp_projection_mode.value == "Pattern On The Fly (POTF)");
    CHECK(printer.dlp_stage_serial_port.value.empty());
    CHECK(printer.dlp_pump_serial_port.value.empty());
    CHECK(printer.dlp_pic_serial_port.value.empty());

    const ConfigOptionDef *motion_mode = print_config_def.get("dlp_motion_mode");
    REQUIRE(motion_mode != nullptr);
    CHECK(motion_mode->type == coString);
    CHECK(motion_mode->gui_type == ConfigOptionDef::GUIType::select_close);
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

TEST_CASE("DLP application replaces a saved FFF printer selection", "[dlp][config][preset]")
{
    const std::string original_data_dir = data_dir();
    const boost::filesystem::path test_data_dir = boost::filesystem::temp_directory_path() /
                                                   boost::filesystem::unique_path("prusaslicer-dlp-presets-%%%%-%%%%");
    for (const char *directory : { "vendor", "print", "sla_print", "filament", "sla_material", "printer", "physical_printer" })
        boost::filesystem::create_directories(test_data_dir / directory);
    set_data_dir(test_data_dir.string());

    AppConfig config(AppConfig::EAppMode::Editor);
    config.set("presets", "printer", "- default FFF -");

    PresetBundle bundle;
    bundle.load_presets(config, ForwardCompatibilitySubstitutionRule::EnableSilent);

    CHECK(bundle.printers.get_selected_preset_name() == "- default DLP -");
    CHECK(bundle.printers.get_selected_preset().printer_technology() == ptSLA);
    CHECK(config.get("presets", "printer") == "- default DLP -");

    set_data_dir(original_data_dir);
    boost::filesystem::remove_all(test_data_dir);
}
