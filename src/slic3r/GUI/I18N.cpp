///|/ Copyright (c) Prusa Research 2018 Vojtěch Bubník @bubnikv
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "libslic3r/libslic3r.h"
#include "I18N.hpp"

namespace Slic3r { namespace GUI { 

wxString I18N::apply_app_branding(wxString text)
{
	text.Replace("PrusaSlicer", SLIC3R_APP_NAME, true);
	return text;
}

wxString L_str(const std::string &str)
{
	//! Explicitly specify that the source string is already in UTF-8 encoding
	return I18N::translate(str);
}

} }
