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

// DLP / corkscrew exposure mode options for SLAPrintObjectConfig.
#define DLP_PRINT_OBJECT_CONFIG_OPTIONS \
    ((ConfigOptionBool, corkscrew_enable)) \
    ((ConfigOptionInt,  corkscrew_box_count))

// DLP SLA print preset option keys (inserted into s_Preset_sla_print_options).
#define DLP_SLA_PRINT_PRESET_OPTION_ENTRIES \
    "corkscrew_enable", \
    "corkscrew_box_count",

#endif // slic3r_DLPConfig_hpp_
