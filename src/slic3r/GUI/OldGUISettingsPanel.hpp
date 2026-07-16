///|/ Copyright (c) Prusa Research 2018 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_OldGUISettingsPanel_hpp_
#define slic3r_OldGUISettingsPanel_hpp_

#include <wx/scrolwin.h>

namespace Slic3r { namespace GUI {

// Compatibility input surface for settings exposed by CLIP3DPrinterGUI.
// The controls intentionally do not affect slicing or hardware yet.
class OldGUISettingsPanel final : public wxScrolledWindow
{
public:
    explicit OldGUISettingsPanel(wxWindow* parent);
};

}} // namespace Slic3r::GUI

#endif // slic3r_OldGUISettingsPanel_hpp_
