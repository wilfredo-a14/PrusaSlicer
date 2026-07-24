///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "PrintConfig.hpp"
#include "I18N.hpp"
#include "DLPDebugLog.hpp"

#include "format.hpp"

namespace Slic3r {

namespace {

constexpr const char *DLP_TOOLTIP = "DLP printer setting migrated from CLIP3DPrinterGUI.";

ConfigOptionDef *add_dlp_bool(PrintConfigDef &config, const char *key, const char *label, bool value)
{
    ConfigOptionDef *def = config.add_dlp_option(key, coBool);
    def->label = label;
    def->category = L("DLP");
    def->tooltip = DLP_TOOLTIP;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(value));
    return def;
}

ConfigOptionDef *add_dlp_int(PrintConfigDef &config, const char *key, const char *label, int value,
                             int minimum, int maximum, const char *unit = nullptr)
{
    ConfigOptionDef *def = config.add_dlp_option(key, coInt);
    def->label = label;
    def->category = L("DLP");
    def->tooltip = DLP_TOOLTIP;
    def->mode = comSimple;
    def->min = minimum;
    def->max = maximum;
    if (unit != nullptr)
        def->sidetext = unit;
    def->set_default_value(new ConfigOptionInt(value));
    return def;
}

ConfigOptionDef *add_dlp_float(PrintConfigDef &config, const char *key, const char *label, double value,
                               double minimum, double maximum, const char *unit = nullptr)
{
    ConfigOptionDef *def = config.add_dlp_option(key, coFloat);
    def->label = label;
    def->category = L("DLP");
    def->tooltip = DLP_TOOLTIP;
    def->mode = comSimple;
    def->min = minimum;
    def->max = maximum;
    if (unit != nullptr)
        def->sidetext = unit;
    def->set_default_value(new ConfigOptionFloat(value));
    return def;
}

ConfigOptionDef *add_dlp_string(PrintConfigDef &config, const char *key, const char *label,
                                const char *value = "")
{
    ConfigOptionDef *def = config.add_dlp_option(key, coString);
    def->label = label;
    def->category = L("DLP");
    def->tooltip = DLP_TOOLTIP;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionString(value));
    return def;
}

ConfigOptionDef *add_dlp_choice(PrintConfigDef &config, const char *key, const char *label,
                                const char *value, std::initializer_list<std::string_view> values)
{
    ConfigOptionDef *def = add_dlp_string(config, key, label, value);
    def->set_enum_values(ConfigOptionDef::GUIType::select_close, values);
    return def;
}

} // namespace

void PrintConfigDef::init_dlp_params()
{
    ConfigOptionDef* def;

    dlp::debug_log(Slic3r::format(
        "DLP config: registering corkscrew options "
        "(display defaults %1%x%2% px, %3%x%4% mm)",
        dlp::DISPLAY_PIXELS_X,
        dlp::DISPLAY_PIXELS_Y,
        dlp::DISPLAY_WIDTH_MM,
        dlp::DISPLAY_HEIGHT_MM));

    def = this->add("corkscrew_enable", coBool);
    def->label = L("Corkscrew mode");
    def->category = L("Corkscrew");
    def->tooltip = L("Enable corkscrew exposure mode.");
    def->mode = comSimple;
    {
        ConfigOption *default_opt = new ConfigOptionBool(false);
        def->set_default_value(default_opt);
    }

    def = this->add("corkscrew_box_count", coInt);
    def->label = L("Number of boxes");
    def->category = L("Corkscrew");
    def->tooltip = L("Number of exposure boxes to divide each layer into when corkscrew mode is enabled.");
    def->min = 1;
    def->max = 64;
    def->mode = comSimple;
    {
        ConfigOption *default_opt = new ConfigOptionInt(4);
        def->set_default_value(default_opt);
    }

    // SLA print preset: settings which describe how a particular job is run.
    add_dlp_float(*this, "dlp_starting_position", "Starting position", 5., 0., 99.99, "mm");
    add_dlp_choice(*this, "dlp_motion_mode", "Motion mode", "Stepped", { "Stepped", "Continuous" });
    add_dlp_float(*this, "dlp_stage_velocity", "Stage velocity", 10., 0., 10., "mm/s");
    add_dlp_float(*this, "dlp_stage_acceleration", "Stage acceleration", 5., 0., 10., "mm/s²");
    add_dlp_float(*this, "dlp_jerk_time", "Jerk time", 40., 0., 99.99, "ms");
    add_dlp_float(*this, "dlp_end_position_min", "Minimum end position", 0., -5., 65., "mm");
    add_dlp_float(*this, "dlp_end_position_max", "Maximum end position", 60., -5., 65., "mm");
    add_dlp_bool(*this, "dlp_stage_pumping", "Enable stage pumping", false);
    add_dlp_float(*this, "dlp_pumping_depth", "Pumping depth", 0., 0., 5000., "µm");

    add_dlp_bool(*this, "dlp_dynamic_print_script", "Enable dynamic print script", false);
    add_dlp_string(*this, "dlp_print_script_file", "Print script file");
    add_dlp_string(*this, "dlp_print_script_directory", "Print script directory");
    add_dlp_string(*this, "dlp_image_directory", "Source image directory");
    add_dlp_string(*this, "dlp_object_image_files", "Object image files");
    add_dlp_bool(*this, "dlp_script_exposure_time", "Script exposure time", true);
    add_dlp_bool(*this, "dlp_script_led_intensity", "Script LED intensity", false);
    add_dlp_bool(*this, "dlp_script_dark_time", "Script dark time", false);
    add_dlp_bool(*this, "dlp_script_layer_thickness", "Script layer thickness", false);
    add_dlp_bool(*this, "dlp_script_stage_velocity", "Script stage velocity", false);
    add_dlp_bool(*this, "dlp_script_stage_acceleration", "Script stage acceleration", false);
    add_dlp_bool(*this, "dlp_script_pumping_depth", "Script pumping depth", false);
    add_dlp_bool(*this, "dlp_script_injection_volume", "Script injection volume", false);
    add_dlp_bool(*this, "dlp_script_injection_rate", "Script injection rate", false);

    add_dlp_string(*this, "dlp_image_target_directory", "Generated image directory");
    add_dlp_choice(*this, "dlp_image_encoding", "Image encoding", "24 x 1-bit images",
                   { "24 x 1-bit images", "3 x 8-bit grayscale images" });
    add_dlp_int(*this, "dlp_pixel_binning", "Pixel binning", 1, 1, 20);
    add_dlp_bool(*this, "dlp_gaussian_blur", "Apply 3x3 Gaussian blur", false);
    add_dlp_int(*this, "dlp_binary_threshold", "Binary threshold", 10, 0, 255);
    add_dlp_bool(*this, "dlp_legacy_auto_mode", "Legacy automatic slicing", false);
    add_dlp_float(*this, "dlp_legacy_auto_print_speed", "Automatic print speed", 40., 0., 10000., "µm/s");
    add_dlp_float(*this, "dlp_legacy_auto_print_height", "Automatic print height", 5000., 0., 70000., "µm");
    add_dlp_string(*this, "dlp_log_directory", "Log directory");
    add_dlp_string(*this, "dlp_log_name", "Log name", "CLIPGUITEST");

    // SLA material preset: exposure and delivery values which vary with resin.
    add_dlp_float(*this, "dlp_initial_exposure_delay", "Initial exposure delay", 0., 0., 99., "s");
    add_dlp_int(*this, "dlp_initial_exposure_intensity", "Initial exposure intensity", 10, 0, 255);
    add_dlp_int(*this, "dlp_uv_intensity", "UV intensity", 12, 0, 255);
    add_dlp_float(*this, "dlp_dark_time", "Dark time", 1., 0., 100000000., "ms");
    add_dlp_float(*this, "dlp_post_exposure_delay", "Post-exposure delay", 0., 0., 10000., "ms");
    add_dlp_float(*this, "dlp_injection_rate", "Injection rate", 5., 0., 99.99, "µL/s");
    add_dlp_float(*this, "dlp_volume_per_layer", "Volume per layer", 5., 0., 99.99, "µL");
    add_dlp_float(*this, "dlp_initial_injection_volume", "Initial injection volume", 0., 0., 99.99, "µL");
    add_dlp_float(*this, "dlp_base_infusion_rate", "Base infusion rate", 0., 0., 99.99, "µL/s");
    add_dlp_choice(*this, "dlp_continuous_injection", "Continuous injection", "Disabled",
                   { "Disabled", "Constant", "Stepped" });
    add_dlp_choice(*this, "dlp_injection_delay_placement", "Injection delay placement", "Off",
                   { "Off", "Pre-movement", "Post-movement" });
    add_dlp_float(*this, "dlp_injection_delay", "Injection delay", 0., 0., 9999., "ms");

    // SLA printer preset: projector, controller, port, and calibration values.
    add_dlp_choice(*this, "dlp_printer_type", "DLP printer type", "CLIP 30 µm", { "CLIP 30 µm", "iCLIP" });
    add_dlp_choice(*this, "dlp_projection_mode", "Projection mode", "Pattern On The Fly (POTF)",
                   { "Pattern On The Fly (POTF)", "Video Pattern (VP)", "Video" });
    add_dlp_choice(*this, "dlp_display_cable", "Display cable", "None", { "None", "HDMI", "DisplayPort" });
    add_dlp_int(*this, "dlp_bit_depth", "Bit depth", 1, 1, 8, "bits");
    add_dlp_int(*this, "dlp_max_image_upload", "Maximum image upload", 50, 0, 399, "patterns");
    add_dlp_int(*this, "dlp_vp_resync_rate", "VP resync rate", 24, 1, 240, "patterns");
    add_dlp_bool(*this, "dlp_dual_asic", "Dual ASIC", true);
    add_dlp_string(*this, "dlp_usb_vid", "DLP USB VID", "0x0451");
    add_dlp_string(*this, "dlp_usb_pid", "DLP USB PID", "0xC900");

    add_dlp_choice(*this, "dlp_stage_hardware", "Stage hardware", "Newport GTS70V (SMC100CC)",
                   { "Newport GTS70V (SMC100CC)", "Thorlabs KVS30/M", "G-code lead screw", "Debugging dummy" });
    add_dlp_choice(*this, "dlp_pump_hardware", "Pump hardware", "None",
                   { "None", "Harvard Apparatus", "Debugging dummy" });
    add_dlp_choice(*this, "dlp_light_engine", "Light engine", "DLi 3DLP9000",
                   { "DLi 3DLP9000", "In-Vision Firebird", "Debugging dummy" });
    add_dlp_choice(*this, "dlp_roll_to_roll", "Roll-to-roll hardware", "None", { "None", "Arduino R2R v1" });
    add_dlp_string(*this, "dlp_stage_serial_port", "Stage serial port");
    add_dlp_string(*this, "dlp_pump_serial_port", "Pump serial port");
    add_dlp_string(*this, "dlp_pic_serial_port", "PIC serial port");
    add_dlp_int(*this, "dlp_smc_baud", "SMC baud rate", 57600, 1, 1000000, "baud");
    add_dlp_int(*this, "dlp_stage_baud", "G-code / KVS baud rate", 115200, 1, 1000000, "baud");
    add_dlp_int(*this, "dlp_pump_baud", "Pump baud rate", 9600, 1, 1000000, "baud");
    add_dlp_string(*this, "dlp_smc_address", "SMC controller address", "1");
    add_dlp_string(*this, "dlp_pump_address", "Pump address", "0");
    add_dlp_float(*this, "dlp_kvs_position_scale", "KVS position scale", 20000., 0., 10000000., "counts/mm");
    add_dlp_float(*this, "dlp_kvs_velocity_scale", "KVS velocity scale", 447392.43, 0., 10000000.);
    add_dlp_float(*this, "dlp_kvs_acceleration_scale", "KVS acceleration scale", 152.71, 0., 10000000.);

    add_dlp_choice(*this, "dlp_manual_stage_type", "Manual stage type", "SMC100CC", { "SMC100CC", "G-code" });
    add_dlp_float(*this, "dlp_manual_relative_move", "Relative move", 0., -1000., 1000., "mm");
    add_dlp_float(*this, "dlp_manual_absolute_move", "Absolute move", 0., -500., 500., "mm");
    add_dlp_float(*this, "dlp_manual_set_position", "Set current position", 0., 0., 1000., "mm");
    add_dlp_float(*this, "dlp_manual_min_limit", "Manual minimum limit", 0., -5., 65., "mm");
    add_dlp_float(*this, "dlp_manual_max_limit", "Manual maximum limit", 0., -5., 65., "mm");
    add_dlp_float(*this, "dlp_manual_velocity", "Manual velocity", 0., 0., 99.99, "mm/s");
    add_dlp_float(*this, "dlp_manual_acceleration", "Manual acceleration", 0., 0., 99.99, "mm/s²");
    add_dlp_choice(*this, "dlp_gcode_endstops", "G-code endstops", "Unchanged",
                   { "Unchanged", "Enabled", "Disabled" });
    add_dlp_string(*this, "dlp_custom_stage_command", "Custom stage command");

    add_dlp_choice(*this, "dlp_pump_target_mode", "Pump target mode", "None", { "None", "Volume", "Time" });
    add_dlp_float(*this, "dlp_manual_target_time", "Manual target time", 0., 0., 5000., "s");
    add_dlp_float(*this, "dlp_manual_target_volume", "Manual target volume", 0., 0., 100000., "µL");
    add_dlp_float(*this, "dlp_manual_infuse_rate", "Manual infuse rate", 0., 0., 5000., "µL/s");
    add_dlp_float(*this, "dlp_manual_withdraw_rate", "Manual withdraw rate", 0., 0., 5000., "µL/s");
    add_dlp_float(*this, "dlp_syringe_volume", "Syringe volume", 0., 0., 1009.9, "µL");
    add_dlp_string(*this, "dlp_custom_pump_command", "Custom pump command");
    add_dlp_choice(*this, "dlp_focus_calibration_mode", "Focus calibration mode", "Unselected",
                   { "Unselected", "Auto calibrate", "Manual calibration" });
    add_dlp_float(*this, "dlp_focus_starting_step", "Focus starting step", 0., 0., 99.99, "µm");
    add_dlp_float(*this, "dlp_focus_minimum_step", "Focus minimum step", 0., 0., 99.99, "µm");
    add_dlp_int(*this, "dlp_camera_exposure", "Camera exposure", 1000, 0, 100000000);
    add_dlp_float(*this, "dlp_camera_gain", "Camera gain", 33., 0., 100., "dB");

    dlp::debug_log("DLP config: registered native print, material, and printer options");
}

} // namespace Slic3r
