///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPPlater.hpp"

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/DLPDebugLog.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/Utils.hpp"
#include "slic3r/Utils/Serial.hpp"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/dir.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <boost/log/trivial.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>

#include "libslic3r/format.hpp"

namespace Slic3r { namespace GUI {

namespace {

using PrinterConfig = std::map<std::string, std::string>;

std::string trim(std::string value)
{
    const std::string whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos)
        return {};
    const size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

std::string display_value(std::string value)
{
    value = trim(std::move(value));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);
    if (value == "True")
        return "Yes";
    if (value == "False")
        return "No";
    if (value == "None")
        return "Auto";
    return value;
}

PrinterConfig load_printer_config()
{
    // These fallbacks mirror printer/config.py and printer/smc100cc.py. The
    // source file is read at runtime so edits there appear without rebuilding.
    PrinterConfig config {
        { "DISPLAY_INDEX", "Auto" },
        { "DISPLAY_RESOLUTION", "(2560, 1600)" },
        { "FULLSCREEN", "Yes" },
        { "DISPLAY_DETECTION_SECONDS", "30.0" },
        { "VIDEO_LOCK_SECONDS", "10.0" },
        { "SCALE_TO_DISPLAY", "No" },
        { "CONFIGURE_DLPC900_HDMI", "Yes" },
        { "DLPC900_USB_VENDOR_ID", "0x0451" },
        { "DLPC900_USB_PRODUCT_ID", "0xC900" },
        { "DLPC900_FLIP_LONG_AXIS", "No" },
        { "DLPC900_FLIP_SHORT_AXIS", "No" },
        { "DLPC900_DEGAMMA_ENABLED", "No" },
        { "DLPC900_DEGAMMA_TABLE", "0" },
        { "STAGE_PORT", "/dev/cu.usbserial-FT55U47V" },
        { "CONTROLLER_ADDRESS", "1" },
        { "STAGE_BAUD_RATE", "57600" },
        { "STAGE_XON_XOFF", "Yes" },
        { "HOME_POSITION_MM", "0.0" },
        { "STAGE_MAX_HEIGHT_MM", "59.0" },
        { "START_POSITION_MM", "58.2" },
        { "LAYER_HEIGHT_MM", "6" },
        { "MOVE_DIRECTION", "-1" },
        { "STAGE_MAX_VELOCITY_MM_S", "10.0" },
        { "STAGE_ACCELERATION_MM_S2", "5.0" },
        { "SETTLE_SECONDS", "0.25" },
        { "STAGE_POSITION_TOLERANCE_MM", "0.005" },
        { "STAGE_CORRECTION_ATTEMPTS", "2" },
        { "STAGE_MAX_CORRECTION_MM", "0.050" },
        { "PUMPING_ENABLED", "Yes" },
        { "PUMP_HEIGHT_MM", "0.6" },
        { "DEBUG_STAGE_POSITION", "Yes" },
        { "EXPOSURE_SECONDS", "2.0" },
        { "INITIAL_EXPOSURE_SECONDS", "Auto" },
    };

    std::ifstream input("printer/config.py");
    if (!input)
        return config;

    const std::regex assignment(
        R"(^\s*([A-Z][A-Z0-9_]*)(?:\s*:[^=]+)?\s*=\s*([^#]+?)\s*$)");
    std::string line;
    std::smatch match;
    while (std::getline(input, line)) {
        if (std::regex_match(line, match, assignment))
            config[match[1].str()] = display_value(match[2].str());
    }

    std::ifstream serial_input("printer/smc100cc.py");
    const std::regex serial_assignment(R"REGEX(^\s*"(baudrate|xonxoff)"\s*:\s*([^,#]+).*$)REGEX");
    while (std::getline(serial_input, line)) {
        if (!std::regex_match(line, match, serial_assignment))
            continue;
        const std::string key = match[1].str() == "baudrate" ? "STAGE_BAUD_RATE" : "STAGE_XON_XOFF";
        config[key] = display_value(match[2].str());
    }
    return config;
}

wxString value_with_unit(const PrinterConfig &config, const char *key, const char *unit = nullptr)
{
    const auto found = config.find(key);
    wxString value = found == config.end() ? _L("Not configured") : from_u8(found->second);
    if (unit != nullptr && !value.empty() && value != _L("Auto"))
        value += " " + from_u8(unit);
    return value;
}

wxString resolution_value(const PrinterConfig &config)
{
    wxString value = value_with_unit(config, "DISPLAY_RESOLUTION");
    value.Replace("(", "");
    value.Replace(")", "");
    value.Replace(", ", " × ");
    return value + " px";
}

double number_value(const PrinterConfig &config, const char *key, double fallback)
{
    const auto found = config.find(key);
    if (found == config.end())
        return fallback;
    try {
        return std::stod(found->second);
    } catch (...) {
        return fallback;
    }
}

bool bool_value(const PrinterConfig &config, const char *key, bool fallback)
{
    const auto found = config.find(key);
    if (found == config.end())
        return fallback;
    return found->second == "Yes" || found->second == "True" || found->second == "1";
}

std::string python_number(double value)
{
    std::ostringstream output;
    output << std::setprecision(12) << value;
    return output.str();
}

std::string python_string(const std::string &value)
{
    std::ostringstream output;
    output << std::quoted(value);
    return output.str();
}

unsigned unsigned_value(const std::string &value, unsigned fallback)
{
    try {
        size_t consumed = 0;
        const unsigned long parsed = std::stoul(value, &consumed, 0);
        return consumed == value.size() ? static_cast<unsigned>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

struct ConnectionStatus {
    bool        stage_connected { false };
    bool        light_engine_connected { false };
    std::string stage_port;
    std::string stage_description;
    std::string light_engine_description;
};

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

ConnectionStatus detect_connections(const PrinterConfig &config, const SLAPrinterConfig &printer_config)
{
    ConnectionStatus status;
    const std::vector<Utils::SerialPortInfo> serial_ports = Utils::scan_serial_ports_extended();
    const std::string configured_stage_port = config.count("STAGE_PORT") ? config.at("STAGE_PORT") : std::string();
    const std::string preset_stage_port = printer_config.dlp_stage_serial_port.value;

    auto select_stage = [&status](const Utils::SerialPortInfo &port) {
        status.stage_connected = true;
        status.stage_port = port.port;
        status.stage_description = port.friendly_name.empty() ? port.port : port.friendly_name;
    };
    for (const std::string &candidate : { preset_stage_port, configured_stage_port }) {
        if (candidate.empty())
            continue;
        const auto found = std::find_if(serial_ports.begin(), serial_ports.end(), [&candidate](const auto &port) {
            return port.port == candidate;
        });
        if (found != serial_ports.end()) {
            select_stage(*found);
            break;
        }
    }
    if (!status.stage_connected) {
        const std::string selected_stage = lowercase(printer_config.dlp_stage_hardware.value);
        const auto found = std::find_if(serial_ports.begin(), serial_ports.end(), [&selected_stage](const auto &port) {
            const std::string description = lowercase(port.friendly_name + " " + port.port);
            if (selected_stage.find("newport") != std::string::npos || selected_stage.find("smc100") != std::string::npos)
                return description.find("newport") != std::string::npos ||
                       description.find("smc100") != std::string::npos ||
                       description.find("gts70") != std::string::npos;
            if (selected_stage.find("thorlabs") != std::string::npos)
                return description.find("thorlabs") != std::string::npos || description.find("kvs30") != std::string::npos;
            return description.find("stage") != std::string::npos || description.find("motion") != std::string::npos;
        });
        if (found != serial_ports.end())
            select_stage(*found);
    }

    const unsigned vendor_id = unsigned_value(
        printer_config.dlp_usb_vid.value.empty() ? config.at("DLPC900_USB_VENDOR_ID") : printer_config.dlp_usb_vid.value,
        0x0451);
    const unsigned product_id = unsigned_value(
        printer_config.dlp_usb_pid.value.empty() ? config.at("DLPC900_USB_PRODUCT_ID") : printer_config.dlp_usb_pid.value,
        0xC900);
    const std::vector<Utils::USBDeviceInfo> usb_devices = Utils::scan_usb_devices();
    const auto light_engine = std::find_if(usb_devices.begin(), usb_devices.end(), [vendor_id, product_id](const auto &device) {
        return device.id_vendor == vendor_id && device.id_product == product_id;
    });
    if (light_engine != usb_devices.end()) {
        status.light_engine_connected = true;
        status.light_engine_description = light_engine->friendly_name;
        if (status.light_engine_description.empty())
            status.light_engine_description = light_engine->manufacturer;
        if (status.light_engine_description.empty())
            status.light_engine_description = "USB device detected";
    }
    return status;
}

wxString connection_error(const ConnectionStatus &connections, const SLAPrinterConfig &printer_config,
                          const PrinterConfig &config)
{
    wxString message = _L("Cannot confirm the print because these required connections were not detected:");
    if (!connections.stage_connected) {
        message += "\n\n- " + from_u8(printer_config.dlp_stage_hardware.value);
        const auto configured = config.find("STAGE_PORT");
        if (configured != config.end() && !configured->second.empty())
            message += " (expected on " + from_u8(configured->second) + ")";
    }
    if (!connections.light_engine_connected)
        message += "\n\n- " + from_u8(printer_config.dlp_light_engine.value) + _L(" USB connection");
    message += "\n\n" + _L("Connect the missing device(s), or run Scan connected devices in Printer > Manual Control, then try again.");
    return message;
}

bool save_printer_config(const PrinterConfig &updates, std::string &error)
{
    const std::string path = "printer/config.py";
    std::ifstream input(path);
    if (!input) {
        error = "Could not open " + path + ". Start the application with linux/run so the printer configuration can be located.";
        return false;
    }

    const std::regex assignment(
        R"(^([ \t]*)([A-Z][A-Z0-9_]*)([ \t]*:[^=]+)?([ \t]*=[ \t]*)([^#]*?)([ \t]*(?:#.*)?)$)");
    std::ostringstream contents;
    std::string line;
    std::smatch match;
    while (std::getline(input, line)) {
        if (std::regex_match(line, match, assignment)) {
            const auto update = updates.find(match[2].str());
            if (update != updates.end())
                line = match[1].str() + match[2].str() + match[3].str() + match[4].str() +
                       update->second + match[6].str();
        }
        contents << line << '\n';
    }
    input.close();

    const std::string temporary_path = path + ".tmp";
    std::ofstream output(temporary_path, std::ios::trunc);
    if (!output) {
        error = "Could not create " + temporary_path + ".";
        return false;
    }
    output << contents.str();
    output.close();
    if (!output) {
        std::remove(temporary_path.c_str());
        error = "Could not finish writing " + temporary_path + ".";
        return false;
    }
    if (std::rename(temporary_path.c_str(), path.c_str()) != 0) {
        std::remove(temporary_path.c_str());
        error = "Could not replace " + path + " with the selected print settings.";
        return false;
    }
    return true;
}

class SettingsFormPage final : public wxScrolledWindow
{
public:
    explicit SettingsFormPage(wxNotebook *parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL)
        , m_em(wxGetApp().em_unit())
        , m_sizer(new wxBoxSizer(wxVERTICAL))
    {
        wxGetApp().UpdateDarkUI(this);
        SetScrollRate(0, m_em);
        SetSizer(m_sizer);
    }

    wxFlexGridSizer *add_group(const wxString &title)
    {
        auto *box = new wxStaticBoxSizer(wxVERTICAL, this, title);
        auto *grid = new wxFlexGridSizer(3, m_em / 2, m_em);
        grid->AddGrowableCol(1, 1);
        box->Add(grid, 1, wxEXPAND | wxALL, m_em / 2);
        m_sizer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, m_em);
        return grid;
    }

    wxStaticText *add_readonly(wxFlexGridSizer *grid, const wxString &label, const wxString &value,
                               const wxString &unit = {})
    {
        grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *text = new wxStaticText(this, wxID_ANY, value);
        grid->Add(text, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(this, wxID_ANY, unit), 0, wxALIGN_CENTER_VERTICAL);
        return text;
    }

    wxSpinCtrlDouble *add_number(wxFlexGridSizer *grid, const wxString &label, double value,
                                 double minimum, double maximum, double increment,
                                 unsigned digits, const wxString &unit)
    {
        grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *control = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                             wxDefaultSize, wxSP_ARROW_KEYS,
                                             minimum, maximum, value, increment);
        control->SetDigits(digits);
        wxGetApp().UpdateDarkUI(control);
        grid->Add(control, 1, wxEXPAND);
        grid->Add(new wxStaticText(this, wxID_ANY, unit), 0, wxALIGN_CENTER_VERTICAL);
        return control;
    }

    wxCheckBox *add_checkbox(wxFlexGridSizer *grid, const wxString &label, bool checked)
    {
        grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *control = new wxCheckBox(this, wxID_ANY, _L("Enabled"));
        control->SetValue(checked);
        wxGetApp().UpdateDarkUI(control);
        grid->Add(control, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        grid->AddSpacer(1);
        return control;
    }

private:
    int         m_em;
    wxBoxSizer *m_sizer;
};

bool show_print_confirmation(wxWindow *parent, const SLAPrint &sla_print)
{
    const wxString output_directory = from_u8(sla_print.png_export_dir());
    const PrinterConfig config = load_printer_config();
    const SLAPrinterConfig &printer_config = sla_print.printer_config();
    ConnectionStatus detected_connections = detect_connections(config, printer_config);
    const double sliced_layer_height = sla_print.default_object_config().layer_height.value;

    wxDialog dialog(parent, wxID_ANY, _L("Review print configuration"), wxDefaultPosition, wxDefaultSize,
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxGetApp().UpdateDarkUI(&dialog);
    dialog.SetFont(wxGetApp().normal_font());

    const int em = wxGetApp().em_unit();
    const int border = em;
    auto *main_sizer = new wxBoxSizer(wxVERTICAL);

    auto *heading = new wxStaticText(&dialog, wxID_ANY, _L("Confirm print"));
    heading->SetFont(wxGetApp().bold_font());
    main_sizer->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, border);
    main_sizer->Add(new wxStaticText(&dialog, wxID_ANY,
        _L("Slicing and PNG export are complete. Review the print settings below. "
           "Editable values are saved when you confirm the print.")),
        0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, border);

    main_sizer->Add(new wxStaticText(&dialog, wxID_ANY, _L("Export folder:")),
                    0, wxLEFT | wxRIGHT | wxTOP, border);
    auto *directory = new wxTextCtrl(&dialog, wxID_ANY, output_directory, wxDefaultPosition, wxDefaultSize,
                                     wxTE_READONLY);
    wxGetApp().UpdateDarkUI(directory);
    main_sizer->Add(directory, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, border);

    auto *notebook = new wxNotebook(&dialog, wxID_ANY);

    auto *projector = new SettingsFormPage(notebook);
    auto *light_engine_group = projector->add_group(_L("Light engine connection"));
    projector->add_readonly(light_engine_group, _L("Light engine"), from_u8(printer_config.dlp_light_engine.value));
    projector->add_readonly(light_engine_group, _L("Controller"), "Texas Instruments DLPC900");
    auto *light_engine_status = projector->add_readonly(
        light_engine_group, _L("USB connection"),
        detected_connections.light_engine_connected
            ? _L("Detected — ") + from_u8(detected_connections.light_engine_description)
            : _L("Not detected"));
    auto *display_group = projector->add_group(_L("Display"));
    projector->add_readonly(display_group, _L("Layer image directory"), output_directory);
    projector->add_readonly(display_group, _L("Display selection"), value_with_unit(config, "DISPLAY_INDEX"));
    projector->add_readonly(display_group, _L("Resolution"), resolution_value(config));
    auto *fullscreen = projector->add_checkbox(display_group, _L("Fullscreen projection"),
                                                bool_value(config, "FULLSCREEN", true));
    auto *projector_group = projector->add_group(_L("Projector behavior"));
    auto *configure_hdmi = projector->add_checkbox(projector_group, _L("Configure controller for HDMI"),
                                                    bool_value(config, "CONFIGURE_DLPC900_HDMI", true));
    auto *flip_long = projector->add_checkbox(projector_group, _L("Flip long axis"),
                                               bool_value(config, "DLPC900_FLIP_LONG_AXIS", false));
    auto *flip_short = projector->add_checkbox(projector_group, _L("Flip short axis"),
                                                bool_value(config, "DLPC900_FLIP_SHORT_AXIS", false));
    projector->add_readonly(projector_group, _L("Display detection timeout"),
                            value_with_unit(config, "DISPLAY_DETECTION_SECONDS", "s"));
    projector->add_readonly(projector_group, _L("Video lock timeout"),
                            value_with_unit(config, "VIDEO_LOCK_SECONDS", "s"));
    notebook->AddPage(projector, _L("Projector"));

    auto *motion = new SettingsFormPage(notebook);
    auto *connection_group = motion->add_group(_L("Stage connection"));
    motion->add_readonly(connection_group, _L("Controller"), from_u8(printer_config.dlp_stage_hardware.value));
    auto *stage_port = motion->add_readonly(
        connection_group, _L("Serial port"),
        detected_connections.stage_connected ? from_u8(detected_connections.stage_port)
                                             : value_with_unit(config, "STAGE_PORT"));
    auto *stage_status = motion->add_readonly(
        connection_group, _L("Connection"),
        detected_connections.stage_connected ? _L("Detected") : _L("Not detected"));
    motion->add_readonly(connection_group, _L("Address"), value_with_unit(config, "CONTROLLER_ADDRESS"));
    motion->add_readonly(connection_group, _L("Baud rate"), value_with_unit(config, "STAGE_BAUD_RATE"), _L("baud"));

    auto *travel_group = motion->add_group(_L("Layer motion"));
    motion->add_readonly(travel_group, _L("Layer height"),
                         wxString::Format("%.6g", sliced_layer_height), _L("mm · from slices"));
    auto *return_position = motion->add_number(travel_group, _L("Return position"),
                                                number_value(config, "HOME_POSITION_MM", 0.0),
                                                0.0, number_value(config, "STAGE_MAX_HEIGHT_MM", 59.0),
                                                0.1, 3, _L("mm"));
    motion->add_readonly(travel_group, _L("Maximum stage height"), value_with_unit(config, "STAGE_MAX_HEIGHT_MM"), _L("mm"));
    auto *start_position = motion->add_number(travel_group, _L("Start position"),
                                               number_value(config, "START_POSITION_MM", 58.2),
                                               0.0, number_value(config, "STAGE_MAX_HEIGHT_MM", 59.0),
                                               0.1, 3, _L("mm"));
    auto *velocity = motion->add_number(travel_group, _L("Maximum velocity"),
                                        number_value(config, "STAGE_MAX_VELOCITY_MM_S", 10.0),
                                        0.01, 100.0, 0.1, 2, _L("mm/s"));
    auto *acceleration = motion->add_number(travel_group, _L("Acceleration"),
                                            number_value(config, "STAGE_ACCELERATION_MM_S2", 5.0),
                                            0.01, 100.0, 0.1, 2, _L("mm/s²"));
    auto *settle_time = motion->add_number(travel_group, _L("Settle time"),
                                           number_value(config, "SETTLE_SECONDS", 0.25),
                                           0.0, 60.0, 0.05, 2, _L("s"));

    auto *pumping_group = motion->add_group(_L("Pumping"));
    auto *pumping = motion->add_checkbox(pumping_group, _L("Pump between exposures"),
                                         bool_value(config, "PUMPING_ENABLED", true));
    auto *pump_height = motion->add_number(pumping_group, _L("Pump height"),
                                           number_value(config, "PUMP_HEIGHT_MM", 0.6),
                                           sliced_layer_height + 0.001,
                                           number_value(config, "STAGE_MAX_HEIGHT_MM", 59.0),
                                           0.1, 3, _L("mm"));
    const auto update_pumping_controls = [pumping, pump_height](wxCommandEvent &) {
        pump_height->Enable(pumping->GetValue());
    };
    pump_height->Enable(pumping->GetValue());
    pumping->Bind(wxEVT_CHECKBOX, update_pumping_controls);
    notebook->AddPage(motion, _L("Motion"));

    auto *exposure = new SettingsFormPage(notebook);
    auto *exposure_group = exposure->add_group(_L("Exposure timing"));
    auto *standard_exposure = exposure->add_number(exposure_group, _L("Standard exposure"),
                                                    number_value(config, "EXPOSURE_SECONDS", 2.0),
                                                    0.01, 3600.0, 0.1, 2, _L("s"));
    wxString initial_exposure = value_with_unit(config, "INITIAL_EXPOSURE_SECONDS");
    if (initial_exposure == _L("Auto"))
        initial_exposure = _L("Same as standard exposure");
    exposure->add_readonly(exposure_group, _L("Initial exposure"), initial_exposure);
    auto *diagnostics_group = exposure->add_group(_L("Diagnostics"));
    auto *position_logging = exposure->add_checkbox(diagnostics_group, _L("Stage position logging"),
                                                     bool_value(config, "DEBUG_STAGE_POSITION", true));
    notebook->AddPage(exposure, _L("Exposure"));
    main_sizer->Add(notebook, 1, wxEXPAND | wxALL, border);

    wxStdDialogButtonSizer *buttons = dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    if (auto *confirm = dynamic_cast<wxButton *>(dialog.FindWindow(wxID_OK))) {
        confirm->SetLabel(_L("Confirm print"));
        confirm->SetDefault();
        confirm->Bind(wxEVT_BUTTON, [&dialog, &config, &printer_config, &detected_connections,
                                     light_engine_status, stage_port, stage_status](wxCommandEvent &) {
            detected_connections = detect_connections(config, printer_config);
            light_engine_status->SetLabel(detected_connections.light_engine_connected
                ? _L("Detected — ") + from_u8(detected_connections.light_engine_description)
                : _L("Not detected"));
            stage_port->SetLabel(detected_connections.stage_connected
                ? from_u8(detected_connections.stage_port) : value_with_unit(config, "STAGE_PORT"));
            stage_status->SetLabel(detected_connections.stage_connected ? _L("Detected") : _L("Not detected"));
            dialog.Layout();
            if (!detected_connections.stage_connected || !detected_connections.light_engine_connected) {
                show_error(&dialog, connection_error(detected_connections, printer_config, config));
                return;
            }
            dialog.EndModal(wxID_OK);
        });
    }
    main_sizer->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);

    dialog.SetSizer(main_sizer);
    dialog.SetMinSize(wxSize(64 * em, 38 * em));
    dialog.SetSize(wxSize(78 * em, 52 * em));
    dialog.CentreOnParent();

    if (dialog.ShowModal() != wxID_OK)
        return false;

    PrinterConfig updates {
        { "LAYER_HEIGHT_MM", python_number(sliced_layer_height) },
        { "FULLSCREEN", fullscreen->GetValue() ? "True" : "False" },
        { "CONFIGURE_DLPC900_HDMI", configure_hdmi->GetValue() ? "True" : "False" },
        { "DLPC900_FLIP_LONG_AXIS", flip_long->GetValue() ? "True" : "False" },
        { "DLPC900_FLIP_SHORT_AXIS", flip_short->GetValue() ? "True" : "False" },
        { "STAGE_PORT", python_string(detected_connections.stage_port) },
        { "HOME_POSITION_MM", python_number(return_position->GetValue()) },
        { "START_POSITION_MM", python_number(start_position->GetValue()) },
        { "STAGE_MAX_VELOCITY_MM_S", python_number(velocity->GetValue()) },
        { "STAGE_ACCELERATION_MM_S2", python_number(acceleration->GetValue()) },
        { "SETTLE_SECONDS", python_number(settle_time->GetValue()) },
        { "PUMPING_ENABLED", pumping->GetValue() ? "True" : "False" },
        { "PUMP_HEIGHT_MM", python_number(pump_height->GetValue()) },
        { "EXPOSURE_SECONDS", python_number(standard_exposure->GetValue()) },
        { "DEBUG_STAGE_POSITION", position_logging->GetValue() ? "True" : "False" },
    };
    std::string save_error;
    if (!save_printer_config(updates, save_error)) {
        show_error(parent, from_u8(save_error));
        return false;
    }
    return true;
}

} // namespace

bool prepare_dlp_print(wxWindow *parent, SLAPrint &sla_print)
{
    dlp::debug_log("GUI PNG export: showing directory picker dialog");
    BOOST_LOG_TRIVIAL(debug) << "DLP print: showing PNG layer export directory picker";
    const std::string remembered_path = wxGetApp().app_config->get("dlp_last_slice_directory");
    wxString previous_directory = from_u8(remembered_path);
    if (previous_directory.empty() || !wxDir::Exists(previous_directory))
        previous_directory = from_u8(sla_print.png_export_dir());
    wxDirDialog dlg(parent, _L("Choose PNG layer export directory:"),
                    previous_directory,
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) {
        sla_print.set_png_export_dir("");
        dlp::debug_log("GUI PNG export: directory picker cancelled — export disabled");
        BOOST_LOG_TRIVIAL(debug) << "DLP print: PNG export directory picker cancelled";
        return false;
    }

    const wxString selected_directory = dlg.GetPath();
    const std::string path = into_u8(selected_directory);
    dlp::debug_log(Slic3r::format("GUI PNG export: directory accepted path=%1%", path));

    wxGetApp().app_config->set("dlp_last_slice_directory", path);
    wxGetApp().app_config->save();
    sla_print.set_png_export_dir(path);
    dlp::debug_log("GUI DLP print: export directory selected; starting slice and PNG export");
    BOOST_LOG_TRIVIAL(info) << "DLP print: PNG export directory set to " << path;
    return true;
}

bool confirm_dlp_print(wxWindow *parent, const SLAPrint &sla_print)
{
    const bool confirmed = show_print_confirmation(parent, sla_print);
    dlp::debug_log(confirmed ? "GUI DLP print: print confirmed"
                             : "GUI DLP print: print confirmation cancelled");
    BOOST_LOG_TRIVIAL(info) << "DLP print: print " << (confirmed ? "confirmed" : "cancelled")
                            << " after PNG export to " << sla_print.png_export_dir();
    return confirmed;
}

}} // namespace Slic3r::GUI
