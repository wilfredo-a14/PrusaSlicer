///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPPlater_hpp_
#define slic3r_DLPPlater_hpp_

class wxWindow;

namespace Slic3r { class SLAPrint; }

namespace Slic3r { namespace GUI {

// Select the PNG layer export directory. Closing the picker with OK allows
// slicing and PNG export to begin immediately.
bool prepare_dlp_print(wxWindow *parent, SLAPrint &sla_print);

// Review the active settings after slicing and PNG export have completed.
// This dialog confirms only the print, not the preceding slice/export.
bool confirm_dlp_print(wxWindow *parent, const SLAPrint &sla_print);

}} // namespace Slic3r::GUI

#endif // slic3r_DLPPlater_hpp_
