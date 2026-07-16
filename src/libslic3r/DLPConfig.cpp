///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "PrintConfig.hpp"
#include "I18N.hpp"
#include "DLPDebugLog.hpp"

#include "format.hpp"

namespace Slic3r {

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

    dlp::debug_log("DLP config: corkscrew_enable default=false, corkscrew_box_count default=4");
}

} // namespace Slic3r

