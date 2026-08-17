///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPConfig_hpp_
#define slic3r_DLPConfig_hpp_

namespace Slic3r::dlp {

// Default DLP display resolution (hard-coded project default).
constexpr int    DISPLAY_PIXELS_X  = 2560;
constexpr int    DISPLAY_PIXELS_Y  = 1600;
constexpr double DISPLAY_WIDTH_MM  = 120.0;
constexpr double DISPLAY_HEIGHT_MM = DISPLAY_WIDTH_MM * DISPLAY_PIXELS_Y / DISPLAY_PIXELS_X;

} // namespace Slic3r::dlp

// DLP process options stored in the SLA print preset.
#define DLP_PRINT_OBJECT_CONFIG_OPTIONS \
    ((ConfigOptionBool, corkscrew_enable)) \
    ((ConfigOptionInt,  corkscrew_box_count)) \
    ((ConfigOptionFloat, multibox_angle_between_boxes)) \
    ((ConfigOptionInt, multibox_box_height_px)) \
    ((ConfigOptionInt, multibox_box_width_px)) \
    ((ConfigOptionFloat, multibox_fab_height_mm)) \
    ((ConfigOptionFloat, multibox_fab_width_mm)) \
    ((ConfigOptionInt, multibox_num_boxes)) \
    ((ConfigOptionFloat, multibox_pixel_scale_um)) \
    ((ConfigOptionFloat, multibox_radius_mm)) \
    ((ConfigOptionFloat, multibox_starting_angle)) \
    ((ConfigOptionFloat, dlp_starting_position)) \
    ((ConfigOptionString, dlp_motion_mode)) \
    ((ConfigOptionFloat, dlp_stage_velocity)) \
    ((ConfigOptionFloat, dlp_stage_acceleration)) \
    ((ConfigOptionFloat, dlp_jerk_time)) \
    ((ConfigOptionFloat, dlp_end_position_min)) \
    ((ConfigOptionFloat, dlp_end_position_max)) \
    ((ConfigOptionBool, dlp_stage_pumping)) \
    ((ConfigOptionFloat, dlp_pumping_depth)) \
    ((ConfigOptionBool, dlp_dynamic_print_script)) \
    ((ConfigOptionString, dlp_print_script_file)) \
    ((ConfigOptionString, dlp_print_script_directory)) \
    ((ConfigOptionString, dlp_image_directory)) \
    ((ConfigOptionString, dlp_object_image_files)) \
    ((ConfigOptionBool, dlp_script_exposure_time)) \
    ((ConfigOptionBool, dlp_script_led_intensity)) \
    ((ConfigOptionBool, dlp_script_dark_time)) \
    ((ConfigOptionBool, dlp_script_layer_thickness)) \
    ((ConfigOptionBool, dlp_script_stage_velocity)) \
    ((ConfigOptionBool, dlp_script_stage_acceleration)) \
    ((ConfigOptionBool, dlp_script_pumping_depth)) \
    ((ConfigOptionBool, dlp_script_injection_volume)) \
    ((ConfigOptionBool, dlp_script_injection_rate)) \
    ((ConfigOptionString, dlp_image_target_directory)) \
    ((ConfigOptionString, dlp_image_encoding)) \
    ((ConfigOptionInt, dlp_pixel_binning)) \
    ((ConfigOptionBool, dlp_gaussian_blur)) \
    ((ConfigOptionInt, dlp_binary_threshold)) \
    ((ConfigOptionBool, dlp_legacy_auto_mode)) \
    ((ConfigOptionFloat, dlp_legacy_auto_print_speed)) \
    ((ConfigOptionFloat, dlp_legacy_auto_print_height)) \
    ((ConfigOptionString, dlp_log_directory)) \
    ((ConfigOptionString, dlp_log_name))

// Resin-dependent DLP options stored in the SLA material preset.
#define DLP_MATERIAL_CONFIG_OPTIONS \
    ((ConfigOptionFloat, dlp_initial_exposure_delay)) \
    ((ConfigOptionInt, dlp_initial_exposure_intensity)) \
    ((ConfigOptionInt, dlp_uv_intensity)) \
    ((ConfigOptionFloat, dlp_dark_time)) \
    ((ConfigOptionFloat, dlp_post_exposure_delay)) \
    ((ConfigOptionFloat, dlp_injection_rate)) \
    ((ConfigOptionFloat, dlp_volume_per_layer)) \
    ((ConfigOptionFloat, dlp_initial_injection_volume)) \
    ((ConfigOptionFloat, dlp_base_infusion_rate)) \
    ((ConfigOptionString, dlp_continuous_injection)) \
    ((ConfigOptionString, dlp_injection_delay_placement)) \
    ((ConfigOptionFloat, dlp_injection_delay))

// Machine-specific DLP options stored in the SLA printer preset.
#define DLP_PRINTER_CONFIG_OPTIONS \
    ((ConfigOptionString, dlp_printer_type)) \
    ((ConfigOptionString, dlp_projection_mode)) \
    ((ConfigOptionString, dlp_display_cable)) \
    ((ConfigOptionInt, dlp_bit_depth)) \
    ((ConfigOptionInt, dlp_max_image_upload)) \
    ((ConfigOptionInt, dlp_vp_resync_rate)) \
    ((ConfigOptionBool, dlp_dual_asic)) \
    ((ConfigOptionString, dlp_usb_vid)) \
    ((ConfigOptionString, dlp_usb_pid)) \
    ((ConfigOptionString, dlp_stage_hardware)) \
    ((ConfigOptionString, dlp_pump_hardware)) \
    ((ConfigOptionString, dlp_light_engine)) \
    ((ConfigOptionString, dlp_roll_to_roll)) \
    ((ConfigOptionString, dlp_stage_serial_port)) \
    ((ConfigOptionString, dlp_pump_serial_port)) \
    ((ConfigOptionString, dlp_pic_serial_port)) \
    ((ConfigOptionInt, dlp_smc_baud)) \
    ((ConfigOptionInt, dlp_stage_baud)) \
    ((ConfigOptionInt, dlp_pump_baud)) \
    ((ConfigOptionString, dlp_smc_address)) \
    ((ConfigOptionString, dlp_pump_address)) \
    ((ConfigOptionFloat, dlp_kvs_position_scale)) \
    ((ConfigOptionFloat, dlp_kvs_velocity_scale)) \
    ((ConfigOptionFloat, dlp_kvs_acceleration_scale)) \
    ((ConfigOptionString, dlp_manual_stage_type)) \
    ((ConfigOptionFloat, dlp_manual_relative_move)) \
    ((ConfigOptionFloat, dlp_manual_absolute_move)) \
    ((ConfigOptionFloat, dlp_manual_set_position)) \
    ((ConfigOptionFloat, dlp_manual_min_limit)) \
    ((ConfigOptionFloat, dlp_manual_max_limit)) \
    ((ConfigOptionFloat, dlp_manual_velocity)) \
    ((ConfigOptionFloat, dlp_manual_acceleration)) \
    ((ConfigOptionString, dlp_gcode_endstops)) \
    ((ConfigOptionString, dlp_custom_stage_command)) \
    ((ConfigOptionString, dlp_pump_target_mode)) \
    ((ConfigOptionFloat, dlp_manual_target_time)) \
    ((ConfigOptionFloat, dlp_manual_target_volume)) \
    ((ConfigOptionFloat, dlp_manual_infuse_rate)) \
    ((ConfigOptionFloat, dlp_manual_withdraw_rate)) \
    ((ConfigOptionFloat, dlp_syringe_volume)) \
    ((ConfigOptionString, dlp_custom_pump_command)) \
    ((ConfigOptionString, dlp_focus_calibration_mode)) \
    ((ConfigOptionFloat, dlp_focus_starting_step)) \
    ((ConfigOptionFloat, dlp_focus_minimum_step)) \
    ((ConfigOptionInt, dlp_camera_exposure)) \
    ((ConfigOptionFloat, dlp_camera_gain))

// DLP SLA print preset option keys (inserted into s_Preset_sla_print_options).
#define DLP_SLA_PRINT_PRESET_OPTION_ENTRIES \
    "corkscrew_enable", \
    "corkscrew_box_count", \
    "multibox_angle_between_boxes", "multibox_box_height_px", \
    "multibox_box_width_px", "multibox_fab_height_mm", "multibox_fab_width_mm", \
    "multibox_num_boxes", "multibox_pixel_scale_um", "multibox_radius_mm", \
    "multibox_starting_angle", \
    "dlp_starting_position", "dlp_motion_mode", "dlp_stage_velocity", \
    "dlp_stage_acceleration", "dlp_jerk_time", "dlp_end_position_min", \
    "dlp_end_position_max", "dlp_stage_pumping", "dlp_pumping_depth", \
    "dlp_dynamic_print_script", "dlp_print_script_file", "dlp_print_script_directory", \
    "dlp_image_directory", "dlp_object_image_files", \
    "dlp_script_exposure_time", "dlp_script_led_intensity", "dlp_script_dark_time", \
    "dlp_script_layer_thickness", "dlp_script_stage_velocity", \
    "dlp_script_stage_acceleration", "dlp_script_pumping_depth", \
    "dlp_script_injection_volume", "dlp_script_injection_rate", \
    "dlp_image_target_directory", "dlp_image_encoding", "dlp_pixel_binning", \
    "dlp_gaussian_blur", "dlp_binary_threshold", "dlp_legacy_auto_mode", \
    "dlp_legacy_auto_print_speed", "dlp_legacy_auto_print_height", \
    "dlp_log_directory", "dlp_log_name",

#define DLP_SLA_MATERIAL_PRESET_OPTION_ENTRIES \
    "dlp_initial_exposure_delay", "dlp_initial_exposure_intensity", "dlp_uv_intensity", \
    "dlp_dark_time", "dlp_post_exposure_delay", "dlp_injection_rate", \
    "dlp_volume_per_layer", "dlp_initial_injection_volume", "dlp_base_infusion_rate", \
    "dlp_continuous_injection", "dlp_injection_delay_placement", "dlp_injection_delay",

#define DLP_SLA_PRINTER_PRESET_OPTION_ENTRIES \
    "dlp_printer_type", "dlp_projection_mode", "dlp_display_cable", "dlp_bit_depth", \
    "dlp_max_image_upload", "dlp_vp_resync_rate", "dlp_dual_asic", "dlp_usb_vid", \
    "dlp_usb_pid", "dlp_stage_hardware", "dlp_pump_hardware", "dlp_light_engine", \
    "dlp_roll_to_roll", "dlp_stage_serial_port", "dlp_pump_serial_port", \
    "dlp_pic_serial_port", "dlp_smc_baud", "dlp_stage_baud", "dlp_pump_baud", \
    "dlp_smc_address", "dlp_pump_address", "dlp_kvs_position_scale", \
    "dlp_kvs_velocity_scale", "dlp_kvs_acceleration_scale", "dlp_manual_stage_type", \
    "dlp_manual_relative_move", "dlp_manual_absolute_move", "dlp_manual_set_position", \
    "dlp_manual_min_limit", "dlp_manual_max_limit", "dlp_manual_velocity", \
    "dlp_manual_acceleration", "dlp_gcode_endstops", "dlp_custom_stage_command", \
    "dlp_pump_target_mode", "dlp_manual_target_time", "dlp_manual_target_volume", \
    "dlp_manual_infuse_rate", "dlp_manual_withdraw_rate", "dlp_syringe_volume", \
    "dlp_custom_pump_command", "dlp_focus_calibration_mode", "dlp_focus_starting_step", \
    "dlp_focus_minimum_step", "dlp_camera_exposure", "dlp_camera_gain",

#endif // slic3r_DLPConfig_hpp_
