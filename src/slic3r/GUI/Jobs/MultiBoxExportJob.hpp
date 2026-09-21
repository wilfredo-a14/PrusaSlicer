///|/ Copyright (c) Prusa Research 2026
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef MULTIBOXEXPORTJOB_HPP
#define MULTIBOXEXPORTJOB_HPP

#include "Job.hpp"

#include "libslic3r/MultiBox/MultiBoxExporter.hpp"

namespace Slic3r {

class SLAPrint;

namespace GUI {

class Plater;

class MultiBoxExportJob : public Job
{
    SLAPrint           *m_print;
    Plater             *m_plater;
    std::string         m_output_dir;
    std::string         m_project_name;
    MultiBox::ExportResult m_result;

public:
    MultiBoxExportJob(SLAPrint *print, Plater *plater, std::string output_dir, std::string project_name);

    void process(Ctl &ctl) override;
    void finalize(bool canceled, std::exception_ptr &eptr) override;
};

} // namespace GUI
} // namespace Slic3r

#endif // MULTIBOXEXPORTJOB_HPP
