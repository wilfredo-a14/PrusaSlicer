///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPPlater.hpp"

#include "GUI.hpp"
#include "I18N.hpp"
#include "libslic3r/DLPDebugLog.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/dir.h>

#include <boost/log/trivial.hpp>

#include "libslic3r/format.hpp"

namespace Slic3r { namespace GUI {

void prompt_png_export_dir(wxWindow *parent, SLAPrint &sla_print)
{
    dlp::debug_log("GUI PNG export: showing directory picker dialog");
    BOOST_LOG_TRIVIAL(debug) << "SLA reslice: showing PNG layer export directory picker";
    wxDirDialog dlg(parent, _L("Choose PNG layer export directory:"),
                    "C:\\3DPrinter\\output",
                    wxDD_DEFAULT_STYLE);
    if (dlg.ShowModal() == wxID_OK) {
        const std::string path = into_u8(dlg.GetPath());
        sla_print.set_png_export_dir(path);
        dlp::debug_log(Slic3r::format("GUI PNG export: directory accepted path=%1%", path));
        BOOST_LOG_TRIVIAL(info) << "SLA reslice: PNG export directory set to " << path;
    } else {
        sla_print.set_png_export_dir("");
        dlp::debug_log("GUI PNG export: directory picker cancelled — export disabled");
        BOOST_LOG_TRIVIAL(debug) << "SLA reslice: PNG export directory picker cancelled";
    }
}

}} // namespace Slic3r::GUI
