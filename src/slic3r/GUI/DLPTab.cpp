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
    auto page = add_options_page(L("Corkscrew"), "wrench");
    auto optgroup = page->new_optgroup(L("Corkscrew mode"));
    optgroup->append_single_option_line("corkscrew_enable");
    optgroup->append_single_option_line("corkscrew_box_count");

    dlp::debug_log("GUI: built Corkscrew options page");
    BOOST_LOG_TRIVIAL(debug) << "DLP GUI: Corkscrew options page built";
}

}} // namespace Slic3r::GUI
