///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#ifndef slic3r_DLPPrintPanel_hpp_
#define slic3r_DLPPrintPanel_hpp_

#include <memory>
#include <string>
#include <vector>

#include <wx/panel.h>

namespace Slic3r {

class SLAPrint;

namespace GUI {

// Uses the same effective settings and motion model as the native Print panel.
std::vector<double> estimate_native_dlp_layer_completion_seconds(const SLAPrint &print);
double estimate_native_dlp_print_seconds(const SLAPrint &print);
// Store the layer height selected by an explicit slice/reslice operation.
// Merely reopening the Print panel must not replace this value.
void remember_native_dlp_sliced_layer_height(double layer_height_mm);

// In-plater AirPrint monitor: settings, plan validation, log, layer preview, motion graph.
// Start Print begins hardware motion. STOP pauses; Continue resumes; Quit homes and returns to the 3D plater.
// The DLP framebuffer is a separate borderless window on the projector display.
class DLPPrintPanel : public wxPanel
{
public:
    explicit DLPPrintPanel(wxWindow *parent);
    ~DLPPrintPanel() override;

    DLPPrintPanel(const DLPPrintPanel &) = delete;
    DLPPrintPanel &operator=(const DLPPrintPanel &) = delete;

    void start_print(const std::string &image_directory, bool sliced_or_resliced = false);
    bool is_running() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}} // namespace Slic3r::GUI

#endif // slic3r_DLPPrintPanel_hpp_
