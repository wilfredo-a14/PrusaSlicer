///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPPlater_hpp_
#define slic3r_DLPPlater_hpp_

class wxWindow;

namespace Slic3r { class SLAPrint; }

namespace Slic3r { namespace GUI {

// Prompt for a PNG layer export directory before SLA reslicing.
void prompt_png_export_dir(wxWindow *parent, SLAPrint &sla_print);

}} // namespace Slic3r::GUI

#endif // slic3r_DLPPlater_hpp_
