///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "Tab.hpp"

#include "libslic3r/DLPDebugLog.hpp"

#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

void TabSLAPrint::build_dlp_options_pages()
{
    auto page = add_options_page(L("Process Motion"), "cog");
    auto optgroup = page->new_optgroup(L("Stage motion"));
    for (const char *key : { "dlp_starting_position", "dlp_motion_mode", "dlp_stage_velocity",
                             "dlp_stage_acceleration", "dlp_jerk_time", "dlp_end_position_min",
                             "dlp_end_position_max" })
        optgroup->append_single_option_line(key);

    optgroup = page->new_optgroup(L("Stage pumping"));
    optgroup->append_single_option_line("dlp_stage_pumping");
    optgroup->append_single_option_line("dlp_pumping_depth");

    page = add_options_page(L("Print Script"), "note");
    optgroup = page->new_optgroup(L("Dynamic print script"));
    for (const char *key : { "dlp_dynamic_print_script", "dlp_print_script_file",
                             "dlp_print_script_directory", "dlp_image_directory",
                             "dlp_object_image_files" })
        optgroup->append_single_option_line(key);

    optgroup = page->new_optgroup(L("Script columns"));
    for (const char *key : { "dlp_script_exposure_time", "dlp_script_led_intensity",
                             "dlp_script_dark_time", "dlp_script_layer_thickness",
                             "dlp_script_stage_velocity", "dlp_script_stage_acceleration",
                             "dlp_script_pumping_depth", "dlp_script_injection_volume",
                             "dlp_script_injection_rate" })
        optgroup->append_single_option_line(key);

    page = add_options_page(L("Image Generation"), "output+page_white");
    optgroup = page->new_optgroup(L("Generated images"));
    for (const char *key : { "dlp_image_target_directory", "dlp_image_encoding", "dlp_pixel_binning",
                             "dlp_gaussian_blur", "dlp_binary_threshold" })
        optgroup->append_single_option_line(key);

    optgroup = page->new_optgroup(L("Legacy automatic slicing"));
    for (const char *key : { "dlp_legacy_auto_mode", "dlp_legacy_auto_print_speed",
                             "dlp_legacy_auto_print_height" })
        optgroup->append_single_option_line(key);

    optgroup = page->new_optgroup(L("Logging"));
    optgroup->append_single_option_line("dlp_log_directory");
    optgroup->append_single_option_line("dlp_log_name");

    page = add_options_page(L("Corkscrew"), "wrench");
    optgroup = page->new_optgroup(L("Corkscrew mode"));
    optgroup->append_single_option_line("corkscrew_enable");
    optgroup->append_single_option_line("corkscrew_box_count");

    page = add_options_page(L("Multi-box Export"), "output+page_white");
    optgroup = page->new_optgroup(L("Projection boxes"));
    for (const char *key : { "multibox_num_boxes", "multibox_angle_between_boxes",
                             "multibox_starting_angle", "multibox_box_width_px",
                             "multibox_box_height_px", "multibox_pixel_scale_um",
                             "multibox_fab_width_mm", "multibox_fab_height_mm",
                             "multibox_radius_mm" })
        optgroup->append_single_option_line(key);

    dlp::debug_log("GUI: built native print settings pages");
    BOOST_LOG_TRIVIAL(debug) << "Print GUI: native print settings pages built";
}

}} // namespace Slic3r::GUI
