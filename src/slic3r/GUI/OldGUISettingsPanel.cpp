///|/ Copyright (c) Prusa Research 2018 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/

#include "OldGUISettingsPanel.hpp"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <initializer_list>
#include <limits>

namespace Slic3r { namespace GUI {

namespace {

constexpr int LABEL_WIDTH = 215;
constexpr int CONTROL_WIDTH = 180;

wxString u8(const char* text)
{
    return wxString::FromUTF8(text);
}

wxArrayString choices(std::initializer_list<const char*> values)
{
    wxArrayString result;
    for (const char* value : values)
        result.Add(u8(value));
    return result;
}

wxFlexGridSizer* make_grid()
{
    auto* grid = new wxFlexGridSizer(2, 8, 12);
    grid->AddGrowableCol(1, 1);
    return grid;
}

void add_label(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label)
{
    auto* text = new wxStaticText(parent, wxID_ANY, label, wxDefaultPosition, wxSize(LABEL_WIDTH, -1));
    text->Wrap(LABEL_WIDTH);
    grid->Add(text, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);
}

wxTextCtrl* add_text(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label,
                     const wxString& value = wxEmptyString, long style = 0)
{
    add_label(parent, grid, label);
    auto* input = new wxTextCtrl(parent, wxID_ANY, value, wxDefaultPosition,
                                 wxSize(CONTROL_WIDTH, style & wxTE_MULTILINE ? 70 : -1), style);
    grid->Add(input, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    return input;
}

wxSpinCtrlDouble* add_number(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label,
                             double value, double minimum, double maximum, double increment = 1.0,
                             unsigned digits = 2)
{
    add_label(parent, grid, label);
    auto* input = new wxSpinCtrlDouble(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                       wxSize(CONTROL_WIDTH, -1), wxSP_ARROW_KEYS,
                                       minimum, maximum, value, increment);
    input->SetDigits(digits);
    grid->Add(input, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    return input;
}

wxChoice* add_choice(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label,
                     const wxArrayString& values, int selection = 0)
{
    add_label(parent, grid, label);
    auto* input = new wxChoice(parent, wxID_ANY, wxDefaultPosition, wxSize(CONTROL_WIDTH, -1), values);
    if (!values.empty())
        input->SetSelection(std::clamp(selection, 0, static_cast<int>(values.size()) - 1));
    grid->Add(input, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    return input;
}

wxCheckBox* add_check(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label,
                      const wxString& checkbox_label, bool checked = false)
{
    add_label(parent, grid, label);
    auto* input = new wxCheckBox(parent, wxID_ANY, checkbox_label);
    input->SetValue(checked);
    grid->Add(input, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    return input;
}

wxComboBox* add_port(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label)
{
    add_label(parent, grid, label);
    const wxArrayString ports = choices({"COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "COM10"});
    auto* input = new wxComboBox(parent, wxID_ANY, ports.front(), wxDefaultPosition,
                                 wxSize(CONTROL_WIDTH, -1), ports, wxCB_DROPDOWN);
    grid->Add(input, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    return input;
}

wxStaticBoxSizer* add_section(wxWindow* parent, wxBoxSizer* column, const wxString& title,
                              wxFlexGridSizer*& grid)
{
    auto* section = new wxStaticBoxSizer(wxVERTICAL, parent, title);
    grid = make_grid();
    section->Add(grid, 1, wxEXPAND | wxALL, 10);
    column->Add(section, 0, wxEXPAND | wxBOTTOM, 12);
    return section;
}

} // namespace

OldGUISettingsPanel::OldGUISettingsPanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL | wxTAB_TRAVERSAL)
{
    SetScrollRate(0, 12);

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* title = new wxStaticText(this, wxID_ANY, u8("Legacy CLIP3DPrinterGUI settings"));
    wxFont title_font = title->GetFont();
    title_font.SetPointSize(title_font.GetPointSize() + 3);
    title_font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(title_font);
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 16);

    auto* note = new wxStaticText(this, wxID_ANY,
        u8("Compatibility input surface only. These values are not connected to slicing, export, or hardware yet."));
    note->Wrap(900);
    root->Add(note, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);

    auto* columns = new wxBoxSizer(wxHORIZONTAL);
    auto* left = new wxBoxSizer(wxVERTICAL);
    auto* right = new wxBoxSizer(wxVERTICAL);
    columns->Add(left, 1, wxEXPAND | wxRIGHT, 6);
    columns->Add(right, 1, wxEXPAND | wxLEFT, 6);
    root->Add(columns, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    wxFlexGridSizer* grid = nullptr;

    add_section(this, left, u8("Display / DLP"), grid);
    add_choice(this, grid, u8("Printer type"), choices({"CLIP 30 µm", "iCLIP"}));
    add_choice(this, grid, u8("Projection mode"), choices({"Pattern On The Fly (POTF)", "Video Pattern (VP)", "Video"}));
    add_choice(this, grid, u8("Display cable"), choices({"None", "HDMI", "DisplayPort"}));
    add_number(this, grid, u8("Bit depth (bits)"), 1, 1, 8, 1, 0);
    add_number(this, grid, u8("Max image upload (patterns)"), 50, 0, 399, 1, 0);
    add_choice(this, grid, u8("VP resync rate (patterns)"),
               choices({"24", "48", "72", "96", "120", "144", "168", "192", "216", "240"}));
    add_number(this, grid, u8("Pattern width (px)"), 2560, 1, 16384, 1, 0);
    add_number(this, grid, u8("Pattern height (px)"), 1600, 1, 16384, 1, 0);
    add_check(this, grid, u8("DLP architecture"), u8("Dual ASIC"), true);
    add_text(this, grid, u8("DLP USB VID"), "0x0451");
    add_text(this, grid, u8("DLP USB PID"), "0xC900");

    add_section(this, left, u8("Exposure / Light"), grid);
    add_number(this, grid, u8("Initial exposure (s)"), 10, 0, 400, 0.1, 2);
    add_number(this, grid, u8("Initial exposure delay (s)"), 0, 0, 99, 1, 0);
    add_number(this, grid, u8("Initial exposure intensity (0–255)"), 10, 0, 255, 1, 0);
    add_number(this, grid, u8("UV intensity (0–255)"), 12, 0, 255, 1, 0);
    add_number(this, grid, u8("Exposure time (ms)"), 1, 0, 100000000, 0.1, 3);
    add_number(this, grid, u8("Dark time (ms)"), 1, 0, 100000000, 0.1, 3);
    add_number(this, grid, u8("Post-exposure delay (ms)"), 0, 0, 10000, 0.1, 2);

    add_section(this, left, u8("Layer / Motion / Stage"), grid);
    add_text(this, grid, u8("Resin"), u8("No resin selected"));
    add_number(this, grid, u8("Layer thickness (µm)"), 1000, 0, 1000000, 1, 2);
    add_number(this, grid, u8("Starting position (mm)"), 5, 0, 99.99, 0.01, 3);
    add_choice(this, grid, u8("Motion mode"), choices({"Stepped", "Continuous"}));
    add_number(this, grid, u8("Stage velocity (mm/s)"), 10, 0, 10, 0.1, 3);
    add_number(this, grid, u8("Stage acceleration (mm/s²)"), 5, 0, 10, 0.1, 3);
    add_number(this, grid, u8("Jerk time (ms)"), 40, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Minimum end of run (mm)"), 0, -5, 65, 0.01, 3);
    add_number(this, grid, u8("Maximum end of run (mm)"), 60, -5, 65, 0.01, 3);
    add_check(this, grid, u8("Stage pumping"), u8("Enable pumping"));
    add_number(this, grid, u8("Pumping depth (µm)"), 0, 0, 5000, 1, 2);

    add_section(this, left, u8("Pump / Resin Injection"), grid);
    add_number(this, grid, u8("Injection rate (µL/s)"), 5, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Volume per layer (µL)"), 5, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Initial injection volume (µL)"), 0, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Base infusion rate (µL/s)"), 0, 0, 99.99, 0.01, 2);
    add_choice(this, grid, u8("Continuous injection"), choices({"Disabled", "Constant", "Stepped"}));
    add_choice(this, grid, u8("Injection delay placement"), choices({"Off", "Pre-movement", "Post-movement"}));
    add_number(this, grid, u8("Injection delay (ms)"), 0, 0, 9999, 0.1, 2);

    add_section(this, right, u8("Print Script / Images"), grid);
    add_check(this, grid, u8("Dynamic print script"), u8("Enable script"));
    add_text(this, grid, u8("Print script file (.csv/.txt)"));
    add_text(this, grid, u8("Print script directory"), "C://");
    add_text(this, grid, u8("Image file directory"), "C://");
    add_text(this, grid, u8("Object image files"), wxEmptyString, wxTE_MULTILINE);
    add_check(this, grid, u8("Script column 1"), u8("Exposure time (ms)"), true);
    add_check(this, grid, u8("Script column 2"), u8("LED intensity"));
    add_check(this, grid, u8("Script column 3"), u8("Dark time (ms)"));
    add_check(this, grid, u8("Script column 4"), u8("Layer thickness (µm)"));
    add_check(this, grid, u8("Script column 5"), u8("Stage velocity (mm/s)"));
    add_check(this, grid, u8("Script column 6"), u8("Stage acceleration (mm/s²)"));
    add_check(this, grid, u8("Script column 7"), u8("Pumping depth (µm)"));
    add_check(this, grid, u8("Script column 8"), u8("Injection volume (µL)"));
    add_check(this, grid, u8("Script column 9"), u8("Injection rate (µL/s)"));

    add_section(this, right, u8("Image Processing"), grid);
    add_text(this, grid, u8("Target directory"), "C://");
    add_choice(this, grid, u8("Encoding type"), choices({"24 × 1-bit images", "3 × 8-bit grayscale images"}));
    wxArrayString binning;
    for (int i = 1; i <= 20; ++i)
        binning.Add(wxString::Format("%dx%d", i, i));
    add_choice(this, grid, u8("Pixel binning"), binning);
    add_check(this, grid, u8("Fixed image operation"), u8("Apply 3×3 Gaussian blur"));
    add_number(this, grid, u8("Binary threshold (legacy fixed)"), 10, 0, 255, 1, 0);

    add_section(this, right, u8("Hardware Ports / Devices"), grid);
    add_choice(this, grid, u8("Stage hardware"),
               choices({"Newport GTS70V (SMC100CC)", "Thorlabs KVS30/M", "G-code lead screw", "Debugging dummy"}));
    add_choice(this, grid, u8("Pump hardware"), choices({"None", "Harvard Apparatus", "Debugging dummy"}));
    add_choice(this, grid, u8("Light engine"), choices({"DLi 3DLP9000", "In-Vision Firebird", "Debugging dummy"}));
    add_choice(this, grid, u8("Roll-to-roll"), choices({"None", "Arduino R2R v1"}));
    add_port(this, grid, u8("Stage serial port"));
    add_port(this, grid, u8("Pump serial port"));
    add_port(this, grid, u8("PIC serial port"));
    add_number(this, grid, u8("SMC baud"), 57600, 1, 1000000, 1, 0);
    add_number(this, grid, u8("G-code / KVS baud"), 115200, 1, 1000000, 1, 0);
    add_number(this, grid, u8("Pump baud"), 9600, 1, 1000000, 1, 0);
    add_text(this, grid, u8("SMC controller address"), "1");
    add_text(this, grid, u8("Pump address"), "0");
    add_number(this, grid, u8("KVS position scale (counts/mm)"), 20000, 0, 10000000, 1, 2);
    add_number(this, grid, u8("KVS velocity scale"), 447392.43, 0, 10000000, 0.01, 2);
    add_number(this, grid, u8("KVS acceleration scale"), 152.71, 0, 10000000, 0.01, 2);

    add_section(this, right, u8("Manual Stage"), grid);
    add_choice(this, grid, u8("Manual stage type"), choices({"SMC100CC", "G-code"}));
    add_number(this, grid, u8("Relative move (mm)"), 0, -1000, 1000, 0.00001, 5);
    add_number(this, grid, u8("Absolute move (mm)"), 0, -500, 500, 0.00001, 5);
    add_number(this, grid, u8("Set current position (mm)"), 0, 0, 1000, 0.001, 3);
    add_number(this, grid, u8("Manual minimum limit (mm)"), 0, -5, 65, 0.001, 3);
    add_number(this, grid, u8("Manual maximum limit (mm)"), 0, -5, 65, 0.001, 3);
    add_number(this, grid, u8("Manual velocity (mm/s)"), 0, 0, 99.99, 0.001, 3);
    add_number(this, grid, u8("Manual acceleration (mm/s²)"), 0, 0, 99.99, 0.001, 3);
    add_choice(this, grid, u8("G-code endstops"), choices({"Unchanged", "Enabled", "Disabled"}));
    add_text(this, grid, u8("Custom stage command"));

    add_section(this, right, u8("Manual Pump / Focus / Other"), grid);
    add_choice(this, grid, u8("Pump target mode"), choices({"None", "Volume", "Time"}));
    add_number(this, grid, u8("Manual target time (s)"), 0, 0, 5000, 0.00001, 5);
    add_number(this, grid, u8("Manual target volume (µL)"), 0, 0, 100000, 0.00001, 5);
    add_number(this, grid, u8("Manual infuse rate (µL/s)"), 0, 0, 5000, 0.00001, 5);
    add_number(this, grid, u8("Manual withdraw rate (µL/s)"), 0, 0, 5000, 0.00001, 5);
    add_number(this, grid, u8("Syringe volume (µL)"), 0, 0, 1009.9, 0.00001, 5);
    add_text(this, grid, u8("Custom pump command"));
    add_choice(this, grid, u8("Focus calibration mode"), choices({"Unselected", "Auto calibrate", "Manual calibration"}));
    add_number(this, grid, u8("Focus starting step (µm)"), 0, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Focus minimum step (µm)"), 0, 0, 99.99, 0.01, 2);
    add_number(this, grid, u8("Camera exposure"), 1000, 0, 100000000, 1, 0);
    add_number(this, grid, u8("Camera gain (dB)"), 33, 0, 100, 0.1, 1);
    add_check(this, grid, u8("Legacy model slicer"), u8("Auto mode"));
    add_number(this, grid, u8("Deprecated auto print speed (µm/s)"), 40, 0, 10000, 1, 2);
    add_number(this, grid, u8("Deprecated auto print height (µm)"), 5000, 0, 70000, 1, 0);
    add_text(this, grid, u8("Log destination"), "C://");
    add_text(this, grid, u8("Log name"), "CLIPGUITEST");

    SetSizer(root);
    FitInside();
}

}} // namespace Slic3r::GUI
