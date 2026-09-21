///|/ Copyright (c) Prusa Research 2026
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "MultiBoxExportJob.hpp"

#include "libslic3r/SLAPrint.hpp"

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Plater.hpp"

namespace Slic3r::GUI {

MultiBoxExportJob::MultiBoxExportJob(
    SLAPrint   *print,
    Plater     *plater,
    std::string output_dir,
    std::string project_name)
    : m_print(print)
    , m_plater(plater)
    , m_output_dir(std::move(output_dir))
    , m_project_name(std::move(project_name))
{}

void MultiBoxExportJob::process(Ctl &ctl)
{
    ctl.update_status(0, _u8L("Exporting multi-box PNGs"));

    m_result = MultiBox::export_print(
        *m_print, m_output_dir, m_project_name,
        [&ctl](size_t done, size_t total) {
            if (total > 0)
                ctl.update_status(static_cast<int>(100 * done / total), _u8L("Exporting multi-box PNGs"));
            return !ctl.was_canceled();
        });

    ctl.update_status(100, ctl.was_canceled() ? _u8L("Multi-box export canceled.") : _u8L("Multi-box export finished."));
}

void MultiBoxExportJob::finalize(bool canceled, std::exception_ptr &eptr)
{
    if (canceled || eptr || m_plater == nullptr)
        return;

    if (!m_result.error.empty()) {
        GUI::show_error(m_plater, from_u8(m_result.error));
        return;
    }

    GUI::show_info(m_plater,
        wxString::Format(_L("Exported %zu PNG files (%zu layers x %zu boxes)."),
            m_result.files_written, m_result.layers_exported, m_result.boxes_per_layer),
        _L("Multi-box export"));
}

} // namespace Slic3r::GUI
