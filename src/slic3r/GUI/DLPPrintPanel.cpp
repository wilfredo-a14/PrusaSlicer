///|/ Copyright (c) Prusa Research 2017 - 2023
///|/
///|/ PrusaSlicer is released under the terms of the AGPLv3 or higher
///|/
#include "DLPPrintPanel.hpp"

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "libslic3r/DLPDebugLog.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/format.hpp"

#include "PrinterCore.hpp"

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/colour.h>
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/event.h>
#include <wx/filepicker.h>
#include <wx/font.h>
#include <wx/frame.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <thread>
#include <vector>

namespace pp = printer::prototype;

namespace Slic3r { namespace GUI {

namespace {

constexpr int PREVIEW_WIDTH = 360;
constexpr int PREVIEW_HEIGHT = 280;
constexpr int EXPANDED_PREVIEW_WIDTH = 560;
constexpr int EXPANDED_PREVIEW_HEIGHT = 420;
constexpr int SETTING_FIELD_WIDTH = 100;
constexpr int SETTING_PATH_WIDTH = 280;
constexpr const char *PRINT_PANEL_CONFIG_SECTION = "dlp_print_panel";
constexpr const char *PUMP_HEIGHT_AUTOMATIC_KEY = "PUMP_HEIGHT_AUTOMATIC";

boost::filesystem::path printer_config_directory()
{
    if (const char *configured = std::getenv("CLIP3D_PRINTER_DIR"); configured != nullptr && *configured != '\0') {
        const boost::filesystem::path directory(configured);
        boost::system::error_code error;
        if (boost::filesystem::is_regular_file(directory / "config.py", error))
            return directory;
    }

    std::vector<boost::filesystem::path> search_roots;
    boost::system::error_code error;
    const boost::filesystem::path cwd = boost::filesystem::current_path(error);
    if (!error)
        search_roots.emplace_back(cwd);

    const boost::filesystem::path executable = into_path(wxStandardPaths::Get().GetExecutablePath());
    if (!executable.empty())
        search_roots.emplace_back(executable.parent_path());

    for (boost::filesystem::path root : search_roots) {
        while (!root.empty()) {
            const boost::filesystem::path directory = root / "printer";
            error.clear();
            if (boost::filesystem::is_regular_file(directory / "config.py", error))
                return directory;
            const boost::filesystem::path parent = root.parent_path();
            if (parent == root)
                break;
            root = parent;
        }
    }
    return {};
}

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

using PrinterConfig = std::map<std::string, std::string>;

PrinterConfig load_printer_config()
{
    PrinterConfig config;
    const boost::filesystem::path config_directory = printer_config_directory();
    std::ifstream input((config_directory / "config.py").string());
    const std::regex assignment(R"(^\s*([A-Z][A-Z0-9_]*)(?:\s*:[^=]+)?\s*=\s*([^#]+?)\s*$)");
    std::string line;
    std::smatch match;
    if (input) {
        while (std::getline(input, line)) {
            if (std::regex_match(line, match, assignment))
                config[match[1].str()] = display_value(match[2].str());
        }
    }

    std::ifstream serial_input((config_directory / "smc100cc.py").string());
    const std::regex serial_assignment(R"REGEX(^\s*"(baudrate|xonxoff)"\s*:\s*([^,#]+).*$)REGEX");
    while (std::getline(serial_input, line)) {
        if (!std::regex_match(line, match, serial_assignment))
            continue;
        const std::string key = match[1].str() == "baudrate" ? "STAGE_BAUD_RATE" : "STAGE_XON_XOFF";
        config[key] = display_value(match[2].str());
    }

    // Values last accepted by the live Print panel take precedence over the
    // source defaults. AppConfig is user data, so these survive both returning
    // to the plater and restarting PrusaSlicer without editing the repository.
    if (wxTheApp != nullptr && wxGetApp().app_config != nullptr &&
        wxGetApp().app_config->has_section(PRINT_PANEL_CONFIG_SECTION)) {
        for (const auto &[key, value] : wxGetApp().app_config->get_section(PRINT_PANEL_CONFIG_SECTION))
            config[key] = value;
    }
    return config;
}

bool has_persisted_print_panel_settings()
{
    return wxTheApp != nullptr && wxGetApp().app_config != nullptr &&
           wxGetApp().app_config->has(PRINT_PANEL_CONFIG_SECTION, "EXPOSURE_SECONDS");
}

bool config_bool(const PrinterConfig &config, const char *key, bool fallback)
{
    const auto found = config.find(key);
    if (found == config.end())
        return fallback;
    return found->second == "Yes" || found->second == "True" || found->second == "1";
}

bool pump_height_is_automatic(const PrinterConfig &config)
{
    // Older saved settings have no mode marker and may contain the historical
    // fixed 0.600 mm default. Treat those as automatic so they migrate to the
    // sliced layer height instead of looking like an intentional override.
    return config_bool(config, PUMP_HEIGHT_AUTOMATIC_KEY, true);
}

bool saved_pump_height_is_automatic()
{
    return pump_height_is_automatic(load_printer_config());
}

double config_number(const PrinterConfig &config, const char *key, double fallback)
{
    const auto found = config.find(key);
    if (found == config.end() || found->second == "Auto")
        return fallback;
    try {
        return std::stod(found->second);
    } catch (...) {
        return fallback;
    }
}

int config_int(const PrinterConfig &config, const char *key, int fallback)
{
    const auto found = config.find(key);
    if (found == config.end() || found->second == "Auto")
        return fallback;
    try {
        return static_cast<int>(std::stoul(found->second, nullptr, 0));
    } catch (...) {
        return fallback;
    }
}

unsigned config_unsigned(const PrinterConfig &config, const char *key, unsigned fallback)
{
    const auto found = config.find(key);
    if (found == config.end())
        return fallback;
    try {
        return static_cast<unsigned>(std::stoul(found->second, nullptr, 0));
    } catch (...) {
        return fallback;
    }
}

pp::Settings settings_from_saved_config(const std::string &image_directory)
{
    pp::Settings settings = pp::default_settings();
    if (!image_directory.empty())
        settings.image_directory = image_directory;

    const PrinterConfig config = load_printer_config();
    if (config.empty())
        return settings;

    const auto text = [&](const char *key, const std::string &fallback) {
        const auto found = config.find(key);
        return found == config.end() ? fallback : found->second;
    };

    settings.stage_port = text("STAGE_PORT", settings.stage_port);
    settings.controller_address = config_int(config, "CONTROLLER_ADDRESS", settings.controller_address);
    settings.baud_rate = config_int(config, "STAGE_BAUD_RATE", settings.baud_rate);
    settings.xon_xoff = config_bool(config, "STAGE_XON_XOFF", settings.xon_xoff);
    settings.display_index = config_int(config, "DISPLAY_INDEX", -1);
    settings.fullscreen = config_bool(config, "FULLSCREEN", settings.fullscreen);
    settings.scale_to_display = config_bool(config, "SCALE_TO_DISPLAY", settings.scale_to_display);
    settings.display_detection_seconds = config_number(config, "DISPLAY_DETECTION_SECONDS", settings.display_detection_seconds);
    settings.configure_dlpc900 = config_bool(config, "CONFIGURE_DLPC900_HDMI", settings.configure_dlpc900);
    settings.projector_vid = config_unsigned(config, "DLPC900_USB_VENDOR_ID", settings.projector_vid);
    settings.projector_pid = config_unsigned(config, "DLPC900_USB_PRODUCT_ID", settings.projector_pid);
    settings.flip_long_axis = config_bool(config, "DLPC900_FLIP_LONG_AXIS", settings.flip_long_axis);
    settings.flip_short_axis = config_bool(config, "DLPC900_FLIP_SHORT_AXIS", settings.flip_short_axis);
    settings.degamma_enabled = config_bool(config, "DLPC900_DEGAMMA_ENABLED", settings.degamma_enabled);
    settings.degamma_table = config_int(config, "DLPC900_DEGAMMA_TABLE", settings.degamma_table);
    settings.uv_intensity = config_int(config, "UV_INTENSITY", settings.uv_intensity);
    settings.initial_uv_intensity = config_int(
        config, "INITIAL_UV_INTENSITY", settings.initial_uv_intensity);
    settings.video_lock_seconds = config_number(config, "VIDEO_LOCK_SECONDS", settings.video_lock_seconds);
    settings.home_position_mm = config_number(config, "HOME_POSITION_MM", settings.home_position_mm);
    settings.max_height_mm = config_number(config, "STAGE_MAX_HEIGHT_MM", settings.max_height_mm);
    settings.deadzone_thickness_mm = config_number(
        config, "DEADZONE_THICKNESS_MM", settings.deadzone_thickness_mm);
    settings.layer_height_mm = config_number(config, "LAYER_HEIGHT_MM", settings.layer_height_mm);
    settings.move_direction = config_int(config, "MOVE_DIRECTION", settings.move_direction);
    settings.stage_max_velocity_mm_s = config_number(config, "STAGE_MAX_VELOCITY_MM_S", settings.stage_max_velocity_mm_s);
    settings.stage_acceleration_mm_s2 = config_number(config, "STAGE_ACCELERATION_MM_S2", settings.stage_acceleration_mm_s2);
    settings.settle_seconds = config_number(config, "SETTLE_SECONDS", settings.settle_seconds);
    settings.position_tolerance_mm = config_number(config, "STAGE_POSITION_TOLERANCE_MM", settings.position_tolerance_mm);
    settings.correction_attempts = config_int(config, "STAGE_CORRECTION_ATTEMPTS", settings.correction_attempts);
    settings.max_correction_mm = config_number(config, "STAGE_MAX_CORRECTION_MM", settings.max_correction_mm);
    settings.pumping_enabled = config_bool(config, "PUMPING_ENABLED", settings.pumping_enabled);
    settings.pump_height_mm = pump_height_is_automatic(config)
        ? 2.0 * settings.layer_height_mm
        : config_number(config, "PUMP_HEIGHT_MM", settings.pump_height_mm);
    settings.pump_acceleration_mm_s2 = config_number(
        config, "PUMP_ACCELERATION_MM_S2", settings.pump_acceleration_mm_s2);
    settings.dark_time_seconds = config_number(config, "DARK_TIME_SECONDS", settings.dark_time_seconds);
    settings.exposure_seconds = config_number(config, "EXPOSURE_SECONDS", settings.exposure_seconds);
    settings.initial_exposure_seconds = config_number(config, "INITIAL_EXPOSURE_SECONDS", settings.exposure_seconds);

    const auto resolution = config.find("DISPLAY_RESOLUTION");
    if (resolution != config.end()) {
        int width = 0;
        int height = 0;
        if (std::sscanf(resolution->second.c_str(), "(%d, %d)", &width, &height) == 2) {
            settings.display_width = width;
            settings.display_height = height;
        }
    }
    return settings;
}

wxString format_duration(double seconds)
{
    const int total = std::max(0, int(std::lround(seconds)));
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    if (hours > 0)
        return wxString::Format("%d:%02d:%02d", hours, minutes, secs);
    return wxString::Format("%d:%02d", minutes, secs);
}

class ProgressBarPanel final : public wxPanel
{
public:
    explicit ProgressBarPanel(wxWindow *parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 56))
    {
        SetMinSize(wxSize(-1, 56));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ProgressBarPanel::on_paint, this);
        Bind(wxEVT_SIZE, [this](wxSizeEvent &event) { Refresh(false); event.Skip(); });
    }

    void set_fraction(double fraction)
    {
        fraction_ = std::clamp(fraction, 0.0, 1.0);
        Refresh(false);
    }

    void set_paused(bool paused)
    {
        if (paused_ == paused)
            return;
        paused_ = paused;
        Refresh(false);
    }

private:
    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(16, 16, 20)));
        dc.Clear();
        const wxSize size = GetClientSize();
        const double pad = 2.0;
        const double width = std::max(1.0, double(size.x) - 2.0 * pad);
        const double height = std::max(18.0, double(size.y) - 2.0 * pad);
        const double radius = height / 2.0;
        const double fill = fraction_ * width;
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc == nullptr)
            return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        gc->SetBrush(wxBrush(wxColour(28, 30, 36)));
        gc->SetPen(wxPen(wxColour(58, 62, 74), 1));
        gc->DrawRoundedRectangle(pad, pad, width, height, radius);
        gc->SetPen(*wxTRANSPARENT_PEN);
        for (int tick = 1; tick < 10; ++tick) {
            const double x = pad + width * tick / 10.0;
            gc->SetPen(wxPen(wxColour(48, 52, 62, 160), 1));
            gc->StrokeLine(x, pad + 6.0, x, pad + height - 6.0);
        }
        if (fill > 0.5) {
            const wxColour start = paused_ ? wxColour(90, 150, 190) : wxColour(40, 230, 255);
            const wxColour end = paused_ ? wxColour(50, 90, 140) : wxColour(40, 110, 255);
            gc->SetBrush(gc->CreateLinearGradientBrush(pad, pad, pad + fill, pad, start, end));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->DrawRoundedRectangle(pad, pad, fill, height, radius);
            gc->SetPen(wxPen(wxColour(220, 250, 255, 90), 1));
            gc->StrokeLine(pad + radius, pad + 3.0, pad + std::max(radius, fill - radius), pad + 3.0);
        }
        gc->SetPen(wxPen(wxColour(255, 255, 255, 28), 1));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(pad, pad, width, height, radius);
    }

    double fraction_ { 0.0 };
    bool paused_ { false };
};

class MotionGraphPanel final : public wxPanel
{
public:
    explicit MotionGraphPanel(wxWindow *parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(520, -1))
    {
        SetMinSize(wxSize(520, -1));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &MotionGraphPanel::on_paint, this);
        Bind(wxEVT_SIZE, [this](wxSizeEvent &event) { Refresh(false); event.Skip(); });
        Bind(wxEVT_LEFT_DOWN, &MotionGraphPanel::on_mouse, this);
        Bind(wxEVT_LEFT_UP, &MotionGraphPanel::on_mouse, this);
        Bind(wxEVT_MOTION, &MotionGraphPanel::on_mouse, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { dragging_ = false; });
        SetCursor(wxCURSOR_CROSS);
    }

    void reset(double minimum, double maximum, double layer_height)
    {
        minimum_ = minimum;
        maximum_ = maximum;
        layer_height_ = layer_height;
        samples_.clear();
        paused_ = false;
        inspect_.reset();
        dragging_ = false;
        Refresh(false);
    }

    void add_sample(double seconds, double height, pp::MotionPhase phase, std::size_t layer)
    {
        samples_.push_back({ seconds, height, phase, layer });
        Refresh(false);
    }

    void set_paused(bool paused)
    {
        if (paused_ == paused)
            return;
        paused_ = paused;
        Refresh(false);
    }

    void set_layer_count(std::size_t total)
    {
        layer_count_ = total;
        Refresh(false);
    }

private:
    struct Sample {
        double seconds;
        double height;
        pp::MotionPhase phase;
        std::size_t layer;
    };
    struct Inspect {
        double seconds;
        double height;
        std::size_t layer;
        pp::MotionPhase phase;
    };
    struct Plot {
        int left { 48 };
        int right { 12 };
        int top { 40 };
        int bottom { 30 };
        int width { 0 };
        int height { 0 };
        double time_span { 10.0 };
        double y_span { 1.0 };
    };

    static int tick_count(int pixels, int spacing, int min_ticks, int max_ticks)
    {
        return std::clamp(pixels / spacing, min_ticks, max_ticks);
    }

    Plot plot_metrics() const
    {
        Plot plot;
        const wxSize size = GetClientSize();
        plot.width = size.x - plot.left - plot.right;
        plot.height = size.y - plot.top - plot.bottom;
        plot.y_span = std::max(0.001, maximum_ - minimum_);
        plot.time_span = std::max(10.0, samples_.empty() ? 0.0 : samples_.back().seconds);
        return plot;
    }

    double x_for(const Plot &plot, double seconds) const
    {
        return plot.left + std::clamp(seconds / plot.time_span, 0.0, 1.0) * plot.width;
    }

    double y_for(const Plot &plot, double value) const
    {
        return plot.top + (1.0 - std::clamp((value - minimum_) / plot.y_span, 0.0, 1.0)) * plot.height;
    }

    bool in_plot(const Plot &plot, const wxPoint &point) const
    {
        return plot.width > 1 && plot.height > 1 &&
            point.x >= plot.left && point.x <= plot.left + plot.width &&
            point.y >= plot.top && point.y <= plot.top + plot.height;
    }

    std::optional<Inspect> inspect_at(double seconds) const
    {
        if (samples_.empty())
            return std::nullopt;
        seconds = std::clamp(seconds, samples_.front().seconds, samples_.back().seconds);
        const auto later = std::lower_bound(samples_.begin(), samples_.end(), seconds,
            [](const Sample &sample, double value) { return sample.seconds < value; });
        if (later == samples_.begin())
            return Inspect { samples_.front().seconds, samples_.front().height,
                             samples_.front().layer, samples_.front().phase };
        if (later == samples_.end())
            return Inspect { samples_.back().seconds, samples_.back().height,
                             samples_.back().layer, samples_.back().phase };
        const Sample &from = *(later - 1);
        const Sample &to = *later;
        const double span = to.seconds - from.seconds;
        const double t = span <= 1e-9 ? 1.0 : (seconds - from.seconds) / span;
        return Inspect { seconds, from.height + t * (to.height - from.height),
                         t < 0.5 ? from.layer : to.layer,
                         t < 0.5 ? from.phase : to.phase };
    }

    void on_mouse(wxMouseEvent &event)
    {
        const Plot plot = plot_metrics();
        if (event.LeftDown() && in_plot(plot, event.GetPosition())) {
            dragging_ = true;
            CaptureMouse();
        }
        if (event.LeftUp() && HasCapture()) {
            dragging_ = false;
            ReleaseMouse();
        }
        if ((dragging_ || event.LeftIsDown()) && plot.width > 1) {
            const double seconds = std::clamp(double(event.GetX() - plot.left) / plot.width, 0.0, 1.0) *
                plot.time_span;
            inspect_ = inspect_at(seconds);
            Refresh(false);
        }
        event.Skip();
    }

    std::tuple<wxColour, wxColour, wxColour, wxColour> colors_for(pp::MotionPhase phase) const
    {
        if (phase == pp::MotionPhase::Exposure) {
            if (paused_)
                return { wxColour(210, 130, 130), wxColour(180, 60, 70, 50),
                         wxColour(200, 70, 80, 70), wxColour(140, 30, 40, 0) };
            return { wxColour(255, 72, 88), wxColour(255, 40, 70, 70),
                     wxColour(255, 60, 80, 95), wxColour(255, 40, 70, 0) };
        }
        if (phase == pp::MotionPhase::Pumping) {
            if (paused_)
                return { wxColour(120, 180, 120), wxColour(50, 140, 70, 50),
                         wxColour(70, 170, 90, 70), wxColour(30, 90, 50, 0) };
            return { wxColour(80, 255, 130), wxColour(40, 220, 90, 70),
                     wxColour(50, 230, 110, 95), wxColour(30, 160, 70, 0) };
        }
        if (paused_)
            return { wxColour(140, 190, 220), wxColour(80, 140, 180, 50),
                     wxColour(70, 140, 180, 70), wxColour(40, 80, 110, 0) };
        return { wxColour(90, 230, 255), wxColour(40, 210, 255, 60),
                 wxColour(40, 210, 255, 90), wxColour(40, 120, 255, 0) };
    }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(14, 16, 20)));
        dc.Clear();
        const Plot plot = plot_metrics();
        if (plot.width <= 1 || plot.height <= 1)
            return;

        const int y_ticks = tick_count(plot.height, 28, 8, 16);
        const int x_ticks = tick_count(plot.width, 44, 8, 18);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc == nullptr)
            return;
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        gc->SetBrush(wxBrush(wxColour(10, 12, 16)));
        gc->SetPen(wxPen(wxColour(42, 48, 60), 1));
        gc->DrawRoundedRectangle(plot.left - 1, plot.top - 1, plot.width + 2, plot.height + 2, 6);

        gc->SetPen(wxPen(wxColour(32, 38, 48), 1));
        for (int index = 0; index < y_ticks; ++index) {
            const double y = y_for(plot, maximum_ - plot.y_span * (index + 0.5) / y_ticks);
            gc->StrokeLine(plot.left, y, plot.left + plot.width, y);
        }
        for (int index = 0; index < x_ticks; ++index) {
            const double x = x_for(plot, plot.time_span * (index + 0.5) / x_ticks);
            gc->StrokeLine(x, plot.top, x, plot.top + plot.height);
        }
        gc->SetPen(wxPen(wxColour(52, 62, 78), 1));
        for (int index = 0; index <= y_ticks; ++index) {
            const double y = y_for(plot, maximum_ - plot.y_span * index / y_ticks);
            gc->StrokeLine(plot.left, y, plot.left + plot.width, y);
        }
        for (int index = 0; index <= x_ticks; ++index) {
            const double x = x_for(plot, plot.time_span * index / x_ticks);
            gc->StrokeLine(x, plot.top, x, plot.top + plot.height);
        }
        gc->SetPen(wxPen(wxColour(80, 96, 118), 1));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->DrawRoundedRectangle(plot.left - 1, plot.top - 1, plot.width + 2, plot.height + 2, 6);

        dc.SetFont(GetFont().Smaller());
        dc.SetTextForeground(wxColour(150, 158, 172));
        for (int index = 0; index <= y_ticks; ++index) {
            const double value = maximum_ - plot.y_span * index / y_ticks;
            dc.DrawText(wxString::Format("%.1f", value), 4, int(std::lround(y_for(plot, value))) - 7);
        }
        for (int index = 0; index <= x_ticks; ++index) {
            const double seconds = plot.time_span * index / x_ticks;
            const int x = int(std::lround(x_for(plot, seconds)));
            dc.DrawText(wxString::Format("%.0fs", seconds), x - 8, plot.top + plot.height + 8);
        }

        if (samples_.empty()) {
            dc.SetTextForeground(wxColour(120, 128, 140));
            dc.DrawText("Waiting for stage", plot.left + 10, plot.top + 10);
            return;
        }

        for (std::size_t index = 1; index < samples_.size(); ++index) {
            const Sample &from = samples_[index - 1];
            const Sample &to = samples_[index];
            const auto [line, glow, fill_top, fill_bottom] = colors_for(to.phase);
            const double x0 = x_for(plot, from.seconds), y0 = y_for(plot, from.height);
            const double x1 = x_for(plot, to.seconds), y1 = y_for(plot, to.height);
            wxGraphicsPath fill = gc->CreatePath();
            fill.MoveToPoint(x0, plot.top + plot.height);
            fill.AddLineToPoint(x0, y0);
            fill.AddLineToPoint(x1, y1);
            fill.AddLineToPoint(x1, plot.top + plot.height);
            fill.CloseSubpath();
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(gc->CreateLinearGradientBrush(x0, plot.top, x0, plot.top + plot.height, fill_top, fill_bottom));
            gc->FillPath(fill);
            wxGraphicsPath stroke = gc->CreatePath();
            stroke.MoveToPoint(x0, y0);
            stroke.AddLineToPoint(x1, y1);
            gc->SetPen(wxPen(glow, 8));
            gc->StrokePath(stroke);
            wxGraphicsPath core = gc->CreatePath();
            core.MoveToPoint(x0, y0);
            core.AddLineToPoint(x1, y1);
            gc->SetPen(wxPen(line, 2));
            gc->StrokePath(core);
        }

        const Sample &latest = samples_.back();
        const auto [live_dot, live_glow, live_fill_top, live_fill_bottom] = colors_for(latest.phase);
        static_cast<void>(live_glow);
        static_cast<void>(live_fill_top);
        static_cast<void>(live_fill_bottom);
        const double last_x = x_for(plot, latest.seconds);
        const double last_y = y_for(plot, latest.height);
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(wxColour(live_dot.Red(), live_dot.Green(), live_dot.Blue(), 50)));
        gc->DrawEllipse(last_x - 8, last_y - 8, 16, 16);
        gc->SetBrush(wxBrush(live_dot));
        gc->DrawEllipse(last_x - 3.5, last_y - 3.5, 7, 7);

        wxFont value_font = GetFont();
        value_font.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(value_font);
        dc.SetTextForeground(wxColour(230, 244, 255));
        dc.DrawText(wxString::Format("%.2f mm", latest.height), plot.left + 8, 8);
        dc.SetFont(GetFont().Smaller());
        dc.SetTextForeground(wxColour(120, 170, 190));
        dc.DrawText(wxString::Format("%.1f s", latest.seconds), plot.left + 8, 24);

        if (inspect_) {
            const auto [dot, glow, fill_top, fill_bottom] = colors_for(inspect_->phase);
            static_cast<void>(glow);
            static_cast<void>(fill_top);
            static_cast<void>(fill_bottom);
            const double ix = x_for(plot, inspect_->seconds);
            const double iy = y_for(plot, inspect_->height);
            gc->SetPen(wxPen(wxColour(255, 255, 255, 80), 1, wxPENSTYLE_SHORT_DASH));
            gc->StrokeLine(ix, plot.top, ix, plot.top + plot.height);
            gc->StrokeLine(plot.left, iy, plot.left + plot.width, iy);
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(wxColour(dot.Red(), dot.Green(), dot.Blue(), 50)));
            gc->DrawEllipse(ix - 9, iy - 9, 18, 18);
            gc->SetBrush(wxBrush(dot));
            gc->DrawEllipse(ix - 4, iy - 4, 8, 8);

            const wxString height_text = wxString::Format("%.2f mm", inspect_->height);
            const wxString layer_text = inspect_->layer == 0 ? wxString("Layer -")
                : layer_count_ > 0 ? wxString::Format("Layer %zu / %zu", inspect_->layer, layer_count_)
                : wxString::Format("Layer %zu", inspect_->layer);
            const wxString time_text = wxString::Format("%.1f s", inspect_->seconds);
            wxFont card_font = GetFont().Smaller();
            dc.SetFont(card_font);
            const int line_h = dc.GetCharHeight() + 2;
            const int pad_x = 8;
            const int pad_y = 6;
            const int card_w = std::max({ dc.GetTextExtent(height_text).GetWidth(),
                                          dc.GetTextExtent(layer_text).GetWidth(),
                                          dc.GetTextExtent(time_text).GetWidth() }) + 2 * pad_x;
            const int card_h = 3 * line_h + 2 * pad_y;
            double card_x = ix + 12;
            double card_y = iy - card_h - 10;
            if (card_x + card_w > plot.left + plot.width - 4)
                card_x = ix - card_w - 12;
            if (card_y < plot.top + 4)
                card_y = iy + 12;
            gc->SetBrush(wxBrush(wxColour(12, 14, 20, 230)));
            gc->SetPen(wxPen(wxColour(dot.Red(), dot.Green(), dot.Blue(), 180), 1));
            gc->DrawRoundedRectangle(card_x, card_y, card_w, card_h, 6);
            dc.SetFont(value_font);
            dc.SetTextForeground(wxColour(230, 244, 255));
            dc.DrawText(height_text, int(std::lround(card_x)) + pad_x, int(std::lround(card_y)) + pad_y);
            dc.SetFont(card_font);
            dc.SetTextForeground(wxColour(255, 214, 102));
            dc.DrawText(layer_text, int(std::lround(card_x)) + pad_x,
                        int(std::lround(card_y)) + pad_y + line_h);
            dc.DrawText(time_text, int(std::lround(card_x)) + pad_x,
                        int(std::lround(card_y)) + pad_y + 2 * line_h);
        }

        const int legend_x = plot.left + plot.width - 214;
        dc.SetTextForeground(wxColour(255, 72, 88));
        dc.DrawText("Exposure", legend_x, 8);
        dc.SetTextForeground(wxColour(80, 255, 130));
        dc.DrawText("Pump", legend_x + 72, 8);
        dc.SetTextForeground(wxColour(90, 230, 255));
        dc.DrawText("Motion", legend_x + 122, 8);
        if (paused_) {
            dc.SetTextForeground(wxColour(255, 196, 92));
            dc.DrawText("PAUSED", legend_x, 24);
        }
    }

    double minimum_ { 0.0 };
    double maximum_ { pp::DEFAULT_PRINTER_HEIGHT_MM };
    double layer_height_ { 1.0 };
    std::size_t layer_count_ { 0 };
    bool paused_ { false };
    bool dragging_ { false };
    std::optional<Inspect> inspect_;
    std::vector<Sample> samples_;
};

class ProjectionFrame final : public wxFrame
{
public:
    ProjectionFrame(wxWindow *parent, double display_scale, std::function<void()> cancel)
        : wxFrame(parent, wxID_ANY, "AirPrint Projection", wxDefaultPosition, wxDefaultSize,
                  wxFRAME_NO_TASKBAR | wxBORDER_NONE),
          display_scale_(display_scale), cancel_(std::move(cancel))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ProjectionFrame::on_paint, this);
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &event) {
            if (event.GetKeyCode() == WXK_ESCAPE)
                cancel_();
            else
                event.Skip();
        });
    }

    void black()
    {
        bitmap_ = wxNullBitmap;
        Refresh(false);
        Update();
    }

    void show(wxImage image, bool scale, bool flip_horizontal, bool flip_vertical)
    {
        if (flip_horizontal)
            image = image.Mirror(true);
        if (flip_vertical)
            image = image.Mirror(false);
        const wxSize target = framebuffer_size();
        if (image.GetWidth() != target.x || image.GetHeight() != target.y) {
            if (!scale)
                throw std::runtime_error(
                    "Layer is " + std::to_string(image.GetWidth()) + "x" +
                    std::to_string(image.GetHeight()) + ", but the projection window is " +
                    std::to_string(target.x) + "x" + std::to_string(target.y));
            const double ratio = std::min(double(target.x) / image.GetWidth(), double(target.y) / image.GetHeight());
            image.Rescale(std::max(1, int(std::lround(image.GetWidth() * ratio))),
                          std::max(1, int(std::lround(image.GetHeight() * ratio))), wxIMAGE_QUALITY_NEAREST);
        }
        bitmap_ = wxBitmap(image, wxBITMAP_SCREEN_DEPTH, display_scale_);
        Refresh(false);
        Update();
    }

    wxSize framebuffer_size() const
    {
        const wxSize logical = GetClientSize();
        return { std::max(1, int(std::lround(logical.x * display_scale_))),
                 std::max(1, int(std::lround(logical.y * display_scale_))) };
    }

private:
    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(*wxBLACK_BRUSH);
        dc.Clear();
        if (bitmap_.IsOk()) {
            const wxSize size = GetClientSize();
            const int width = int(std::lround(bitmap_.GetLogicalWidth()));
            const int height = int(std::lround(bitmap_.GetLogicalHeight()));
            dc.DrawBitmap(bitmap_, (size.x - width) / 2, (size.y - height) / 2, false);
        }
    }

    wxBitmap bitmap_;
    double display_scale_ { 1.0 };
    std::function<void()> cancel_;
};

class GuiProjection final : public pp::Projection
{
public:
    GuiProjection(std::function<void()> cancel, pp::Logger logger,
                  std::function<bool()> cancelled, std::function<bool()> paused)
        : cancel_(std::move(cancel)), logger_(std::move(logger)),
          cancelled_(std::move(cancelled)), paused_(std::move(paused)) {}

    void open(int display_index, int requested_width, int requested_height,
              double detection_timeout_seconds, bool fullscreen) override
    {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::duration<double>(detection_timeout_seconds);
        int selected = display_index;
        wxRect geometry;
        double selected_scale = 1.0;
        std::string last_snapshot;
        bool waiting_logged = false;
        while (true) {
            if (paused_ && paused_()) {
                const auto paused_at = std::chrono::steady_clock::now();
                while (paused_() && !(cancelled_ && cancelled_()))
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                deadline += std::chrono::steady_clock::now() - paused_at;
            }
            if (cancelled_ && cancelled_())
                throw std::runtime_error("Print cancelled while waiting for the projector display");
            bool found = false;
            std::string snapshot;
            invoke([&] {
                const int count = static_cast<int>(wxDisplay::GetCount());
                std::vector<int> matches;
                std::vector<int> external_matches;
                std::ostringstream detected;
                for (int index = 0; index < count; ++index) {
                    const wxDisplay display(index);
                    const wxRect candidate = display.GetGeometry();
                    const double scale = display.GetScaleFactor();
                    const int physical_width = int(std::lround(candidate.width * scale));
                    const int physical_height = int(std::lround(candidate.height * scale));
                    if (index)
                        detected << "; ";
                    detected << index << "=" << candidate.width << "x" << candidate.height
                             << " logical @" << scale << "x (" << physical_width << "x"
                             << physical_height << " physical)"
                             << (display.IsPrimary() ? " primary" : " external");
                    const bool requested_matches = requested_width <= 0 ||
                        ((candidate.width == requested_width && candidate.height == requested_height) ||
                         (physical_width == requested_width && physical_height == requested_height));
                    if (requested_matches) {
                        matches.push_back(index);
                        if (!display.IsPrimary())
                            external_matches.push_back(index);
                    }
                }
                snapshot = detected.str();

                if (display_index >= 0) {
                    if (display_index < count &&
                        std::find(matches.begin(), matches.end(), display_index) != matches.end()) {
                        selected = display_index;
                        const wxDisplay display(selected);
                        geometry = display.GetGeometry();
                        selected_scale = display.GetScaleFactor();
                        found = true;
                    }
                    return;
                }
                const std::vector<int> &preferred = external_matches.empty() ? matches : external_matches;
                if (preferred.size() > 1)
                    throw std::runtime_error("Multiple displays match the requested projector resolution; set the display index explicitly");
                if (preferred.size() == 1) {
                    selected = preferred.front();
                    const wxDisplay display(selected);
                    geometry = display.GetGeometry();
                    selected_scale = display.GetScaleFactor();
                    found = true;
                }
            });
            if (snapshot != last_snapshot) {
                logger_("Detected displays: " + (snapshot.empty() ? std::string("none") : snapshot));
                last_snapshot = snapshot;
            }
            if (found)
                break;
            if (!waiting_logged) {
                logger_("Waiting up to " + std::to_string(int(std::ceil(detection_timeout_seconds))) +
                        " seconds for the " + std::to_string(requested_width) + "x" +
                        std::to_string(requested_height) +
                        " extended projector display. USB light-engine detection does not also confirm the HDMI display; check macOS Displays if it does not appear.");
                waiting_logged = true;
            }
            if (std::chrono::steady_clock::now() >= deadline)
                throw std::runtime_error(
                    "The requested extended projector display was not detected before timeout; macOS reported " +
                    (snapshot.empty() ? std::string("no displays") : snapshot));
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        logger_("Selected projector display " + std::to_string(selected) + " at " +
                std::to_string(geometry.width) + "x" + std::to_string(geometry.height) +
                " logical @" + std::to_string(selected_scale) + "x");
        invoke([this, geometry, selected_scale, fullscreen] {
            frame_ = new ProjectionFrame(nullptr, selected_scale, cancel_);
            frame_->SetPosition(geometry.GetPosition());
            frame_->SetSize(geometry.GetSize());
            frame_->Show();
#ifdef __WXOSX__
            (void)fullscreen;
            frame_->SetPosition(geometry.GetPosition());
            frame_->SetSize(geometry.GetSize());
#else
            if (fullscreen)
                frame_->ShowFullScreen(true, wxFULLSCREEN_ALL);
#endif
            frame_->Raise();
            frame_->SetFocus();
            frame_->black();
        });
    }

    void close() noexcept override
    {
        try {
            invoke([this] {
                if (frame_) {
                    frame_->black();
                    frame_->Destroy();
                    frame_ = nullptr;
                }
            });
        } catch (...) {}
    }

    void black() noexcept override
    {
        try {
            invoke([this] {
                if (frame_)
                    frame_->black();
            });
        } catch (...) {}
    }

    void validate_image(const std::filesystem::path &path, bool allow_scaling) override
    {
        invoke([this, path, allow_scaling] {
            wxImage image(wxString::FromUTF8(path.string()));
            if (!image.IsOk())
                throw std::runtime_error("Could not load layer " + path.string());
            const wxSize size = frame_->framebuffer_size();
            if (!allow_scaling && (image.GetWidth() != size.x || image.GetHeight() != size.y))
                throw std::runtime_error(
                    "First layer is " + std::to_string(image.GetWidth()) + "x" +
                    std::to_string(image.GetHeight()) + ", but the selected display window is " +
                    std::to_string(size.x) + "x" + std::to_string(size.y));
        });
    }

    void show_image(const std::filesystem::path &path, bool scale,
                    bool flip_horizontal, bool flip_vertical) override
    {
        invoke([this, path, scale, flip_horizontal, flip_vertical] {
            wxImage image(wxString::FromUTF8(path.string()));
            if (!image.IsOk())
                throw std::runtime_error("Could not load layer " + path.string());
            frame_->show(std::move(image), scale, flip_horizontal, flip_vertical);
        });
    }

private:
    void invoke(std::function<void()> action)
    {
        if (wxIsMainThread()) {
            action();
            return;
        }
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        wxTheApp->CallAfter([action = std::move(action), promise] {
            try {
                action();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        future.get();
    }

    ProjectionFrame *frame_ { nullptr };
    std::function<void()> cancel_;
    pp::Logger logger_;
    std::function<bool()> cancelled_;
    std::function<bool()> paused_;
};

} // namespace

std::vector<double> estimate_native_dlp_layer_completion_seconds(const SLAPrint &print)
{
    pp::Settings settings = settings_from_saved_config(print.png_export_dir());
    const double sliced_layer_height = print.default_object_config().layer_height.value;
    if (std::isfinite(sliced_layer_height) && sliced_layer_height > 0.0) {
        settings.layer_height_mm = sliced_layer_height;
        if (saved_pump_height_is_automatic())
            settings.pump_height_mm = 2.0 * sliced_layer_height;
    }
    return pp::estimate_layer_completion_seconds(settings, print.print_layers().size());
}

double estimate_native_dlp_print_seconds(const SLAPrint &print)
{
    const std::vector<double> completion_times =
        estimate_native_dlp_layer_completion_seconds(print);
    return completion_times.empty() ? 0.0 : completion_times.back();
}

void remember_native_dlp_sliced_layer_height(double layer_height_mm)
{
    if (!std::isfinite(layer_height_mm) || layer_height_mm <= 0.0 ||
        wxTheApp == nullptr || wxGetApp().app_config == nullptr)
        return;
    const bool automatic_pump_height = saved_pump_height_is_automatic();
    PrinterConfig saved;
    if (wxGetApp().app_config->has_section(PRINT_PANEL_CONFIG_SECTION)) {
        for (const auto &[key, value] :
             wxGetApp().app_config->get_section(PRINT_PANEL_CONFIG_SECTION))
            saved[key] = value;
    }
    saved["LAYER_HEIGHT_MM"] = Slic3r::format("%.9g", layer_height_mm);
    saved[PUMP_HEIGHT_AUTOMATIC_KEY] = automatic_pump_height ? "Yes" : "No";
    if (automatic_pump_height)
        saved["PUMP_HEIGHT_MM"] = Slic3r::format("%.9g", 2.0 * layer_height_mm);
    wxGetApp().app_config->set_section(PRINT_PANEL_CONFIG_SECTION, std::move(saved));
    wxGetApp().app_config->save();
}

class DLPPrintPanel::Impl
{
public:
    explicit Impl(DLPPrintPanel *owner)
        : owner_(owner)
        , projection_([this] { request_pause("Exposure paused with Escape"); },
                      [this](const std::string &line) {
                          wxTheApp->CallAfter([this, line] { append_log(line); });
                      },
                      [this] { return cancel_.load(); },
                      [this] { return pause_.load(); })
    {
        const pp::Settings defaults = pp::default_settings();
        root_sizer_ = new wxBoxSizer(wxVERTICAL);
        settings_notebook_ = new wxNotebook(owner_, wxID_ANY);
        auto *print_settings_page = new wxPanel(settings_notebook_);
        auto *stage_controls_page = new wxPanel(settings_notebook_);
        settings_notebook_->AddPage(print_settings_page, _L("Print Settings"), true);
        settings_notebook_->AddPage(stage_controls_page, _L("Stage Controls"));
        auto *path_sizer = new wxFlexGridSizer(2, 6, 8);
        path_sizer_ = path_sizer;
        config_sizer_ = new wxFlexGridSizer(4, 6, 10);
        image_dir_ = new wxDirPickerCtrl(print_settings_page, wxID_ANY,
                                         wxString::FromUTF8(defaults.image_directory.string()));
        constrain_width(image_dir_, SETTING_PATH_WIDTH);
        stage_port_ = text(defaults.stage_port, SETTING_PATH_WIDTH, print_settings_page);
        display_index_ = spin(defaults.display_index, -1, 16, print_settings_page);
        return_height_ = number(defaults.home_position_mm, print_settings_page);
        max_height_ = number(defaults.max_height_mm, print_settings_page);
        deadzone_thickness_ = number(defaults.deadzone_thickness_mm, print_settings_page);
        start_position_ = number(pp::automatic_start_height_mm(defaults), print_settings_page);
        start_position_->SetEditable(false);
        start_position_->SetToolTip(_L("Calculated automatically as printer height minus deadzone thickness."));
        layer_height_ = number(defaults.layer_height_mm, print_settings_page);
        stage_velocity_ = number(defaults.stage_max_velocity_mm_s, print_settings_page);
        stage_acceleration_ = number(defaults.stage_acceleration_mm_s2, print_settings_page);
        dark_time_ = number(defaults.dark_time_seconds, print_settings_page);
        exposure_ = number(defaults.exposure_seconds, print_settings_page);
        initial_exposure_ = number(defaults.initial_exposure_seconds, print_settings_page);
        uv_intensity_ = spin(defaults.uv_intensity, 0, 255, print_settings_page);
        initial_uv_intensity_ = spin(defaults.initial_uv_intensity, 0, 255, print_settings_page);
        pumping_ = new wxCheckBox(print_settings_page, wxID_ANY, _L("Enable peel/pumping motion"));
        pumping_->SetValue(defaults.pumping_enabled);
        pump_height_ = number(defaults.pump_height_mm, print_settings_page);
        pump_height_->SetToolTip(_L(
            "Defaults to twice the layer height. Edit this value to keep a custom pump height."));
        pump_acceleration_ = number(defaults.pump_acceleration_mm_s2, print_settings_page);
        pumping_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { update_pump_height_lock(); });
        fullscreen_ = new wxCheckBox(print_settings_page, wxID_ANY, _L("Fullscreen projector window"));
        fullscreen_->SetValue(defaults.fullscreen);
        scale_ = new wxCheckBox(print_settings_page, wxID_ANY, _L("Fit and center layers on display"));
        scale_->SetValue(defaults.scale_to_display);

        adjust_stage_ = new wxButton(stage_controls_page, wxID_ANY, _L("Move to Start"));
        adjust_stage_->SetToolTip(_L("Move directly to the automatic start height. This never sends a home command."));
        relative_move_ = number(0.1, stage_controls_page, 72);
        relative_move_->SetToolTip(_L("Signed relative movement in millimeters. Use a negative value to move down."));
        relative_stage_ = new wxButton(stage_controls_page, wxID_ANY, _L("Move"));
        relative_stage_->SetToolTip(_L("Move from the controller's current position by the signed millimeter value."));
        current_height_ = new wxStaticText(stage_controls_page, wxID_ANY, _L("Current height: — mm"));
        current_height_->SetToolTip(_L("Most recent encoder height reported by the stage controller."));
        wxFont current_height_font = current_height_->GetFont();
        current_height_font.SetWeight(wxFONTWEIGHT_BOLD);
        current_height_->SetFont(current_height_font);
        automatic_start_height_ = new wxStaticText(stage_controls_page, wxID_ANY,
            wxString::Format("Automatic start: %.3f mm", pp::automatic_start_height_mm(defaults)));
        jog_down_10_ = new wxButton(stage_controls_page, wxID_ANY, _L("−10 mm"));
        jog_down_10_->SetToolTip(_L("Move the stage 10 mm in the negative direction, subject to the configured limits."));
        jog_up_10_ = new wxButton(stage_controls_page, wxID_ANY, _L("+10 mm"));
        jog_up_10_->SetToolTip(_L("Move the stage 10 mm in the positive direction, subject to the configured limits."));
        send_home_ = new wxButton(stage_controls_page, wxID_ANY, _L("Send Home (0 mm)"));
        send_home_->SetToolTip(_L("Reference the controller if needed, then return the stage to exactly 0.000 mm."));
        read_position_ = new wxButton(stage_controls_page, wxID_ANY, _L("Refresh Position"));
        read_position_->SetToolTip(_L("Read the current encoder height without commanding stage motion."));
        stage_stop_ = new wxButton(stage_controls_page, wxID_ANY, _L("Stop Motion"));
        stage_stop_->SetToolTip(_L("Interrupt the active alignment, relative, or home test move."));
        stage_stop_->Disable();

        auto *status_box = new wxStaticBoxSizer(wxHORIZONTAL, stage_controls_page, _L("Stage status"));
        status_box->Add(current_height_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        status_box->Add(read_position_, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
        status_box->Add(stage_stop_, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);

        auto *absolute_box = new wxStaticBoxSizer(wxHORIZONTAL, stage_controls_page, _L("Absolute positions"));
        absolute_box->Add(automatic_start_height_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        absolute_box->Add(adjust_stage_, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
        absolute_box->Add(send_home_, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);

        auto *jog_box = new wxStaticBoxSizer(wxHORIZONTAL, stage_controls_page, _L("Relative movement"));
        jog_box->Add(jog_down_10_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        jog_box->AddStretchSpacer();
        jog_box->Add(new wxStaticText(stage_controls_page, wxID_ANY, _L("Distance (mm)")),
                     0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        jog_box->Add(relative_move_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        jog_box->Add(relative_stage_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        jog_box->AddStretchSpacer();
        jog_box->Add(jog_up_10_, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);

        auto *stage_controls_sizer = new wxBoxSizer(wxVERTICAL);
        stage_controls_sizer->Add(status_box, 0, wxEXPAND | wxALL, 10);
        stage_controls_sizer->Add(absolute_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        stage_controls_sizer->Add(jog_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        stage_controls_sizer->AddStretchSpacer();
        stage_controls_page->SetSizer(stage_controls_sizer);

        add_row(path_sizer, "Layer image directory", image_dir_);
        add_row(path_sizer, "SMC100CC serial port", stage_port_);
        add_pair(config_sizer_, "Projector display (-1 = auto)", display_index_,
                 "Return height (mm)", return_height_);
        add_pair(config_sizer_, "Printer height / maximum (mm)", max_height_,
                 "Start height (automatic)", start_position_);
        add_pair(config_sizer_, "Deadzone thickness (0-0.5 mm)", deadzone_thickness_,
                 "Layer height (mm)", layer_height_);
        add_pair(config_sizer_, "Stage max velocity (mm/s)", stage_velocity_,
                 "Stage acceleration (mm/s²)", stage_acceleration_);
        add_pair(config_sizer_, "Exposure (seconds)", exposure_,
                 "UV intensity (0-255 PWM)", uv_intensity_);
        add_pair(config_sizer_, "First-layer exposure (seconds)", initial_exposure_,
                 "First-layer UV intensity (0-255 PWM)", initial_uv_intensity_);
        add_pair(config_sizer_, "Dark time (seconds)", dark_time_,
                 "Pumping", pumping_);
        add_pair(config_sizer_, "Pump height (mm)", pump_height_,
                 "Pump acceleration (mm/s²)", pump_acceleration_);
        add_pair(config_sizer_, "Projection", fullscreen_,
                 "Layer scaling", scale_);
        config_separator_ = new wxStaticLine(print_settings_page);
        auto *print_settings_sizer = new wxBoxSizer(wxVERTICAL);
        print_settings_sizer->Add(path_sizer, 0, wxALL, 10);
        print_settings_sizer->Add(config_sizer_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
        print_settings_sizer->Add(config_separator_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        print_settings_page->SetSizer(print_settings_sizer);

        progress_label_ = new wxStaticText(owner_, wxID_ANY,
            _L("Validate the plan to see layer count and time estimate"));
        wxFont progress_font = progress_label_->GetFont();
        progress_font.SetWeight(wxFONTWEIGHT_BOLD);
        progress_font.SetPointSize(progress_font.GetPointSize() + 2);
        progress_label_->SetFont(progress_font);
        progress_bar_ = new ProgressBarPanel(owner_);
        root_sizer_->Add(progress_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
        root_sizer_->Add(progress_bar_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

        auto *buttons = new wxBoxSizer(wxHORIZONTAL);
        plan_ = new wxButton(owner_, wxID_ANY, _L("Validate & Move to Start"));
        plan_->SetToolTip(_L("Validate the print plan, then move the referenced stage directly to the automatic start height."));
        start_ = new wxButton(owner_, wxID_ANY, _L("Start Print"));
        led_off_ = new wxButton(owner_, wxID_ANY, _L("LED Off"));
        led_off_->SetToolTip(_L("Set all DLPC900 LED current channels to zero and verify the light engine is off."));
        stop_ = new wxButton(owner_, wxID_ANY, _L("STOP"));
        continue_ = new wxButton(owner_, wxID_ANY, _L("Continue"));
        quit_ = new wxButton(owner_, wxID_ANY, _L("Quit"));
        stop_->Disable();
        continue_->Hide();
        buttons->Add(plan_, 0, wxRIGHT, 8);
        buttons->Add(start_, 0, wxRIGHT, 8);
        buttons->Add(led_off_, 0, wxRIGHT, 8);
        buttons->Add(continue_, 0, wxRIGHT, 8);
        buttons->Add(quit_, 0, wxRIGHT, 8);
        buttons->AddStretchSpacer();
        buttons->Add(stop_);
        root_sizer_->Add(buttons, 0, wxEXPAND | wxALL, 12);

        log_ = new wxTextCtrl(owner_, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
        auto *preview = new wxStaticBoxSizer(wxVERTICAL, owner_, _L("Current slice"));
        preview_bitmap_ = new wxStaticBitmap(preview->GetStaticBox(), wxID_ANY, wxNullBitmap,
                                             wxDefaultPosition, wxSize(PREVIEW_WIDTH, PREVIEW_HEIGHT));
        preview_bitmap_->SetMinSize(wxSize(PREVIEW_WIDTH, PREVIEW_HEIGHT));
        preview_label_ = new wxStaticText(preview->GetStaticBox(), wxID_ANY, _L("Waiting for first slice"));
        preview->Add(preview_bitmap_, 0, wxALIGN_CENTER | wxALL, 6);
        preview->Add(preview_label_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

        auto *live = new wxBoxSizer(wxVERTICAL);
        settings_sizer_ = new wxBoxSizer(wxVERTICAL);
        settings_sizer_->Add(settings_notebook_, 1, wxEXPAND);
        live->Add(settings_sizer_, 0, wxEXPAND | wxBOTTOM, 8);
        live->Add(log_, 1, wxEXPAND | wxBOTTOM, 8);
        live->Add(preview, 1, wxEXPAND);

        auto *motion_box = new wxStaticBoxSizer(wxVERTICAL, owner_, _L("Stage height"));
        motion_graph_ = new MotionGraphPanel(motion_box->GetStaticBox());
        motion_graph_->SetMinSize(wxSize(520, -1));
        motion_box->Add(motion_graph_, 1, wxEXPAND | wxALL, 4);

        auto *body = new wxBoxSizer(wxHORIZONTAL);
        body->Add(live, 2, wxEXPAND | wxRIGHT, 8);
        body->Add(motion_box, 3, wxEXPAND);
        root_sizer_->Add(body, 1, wxEXPAND | wxALL, 12);
        owner_->SetSizer(root_sizer_);
        clear_preview();
        motion_graph_->reset(0.0, defaults.max_height_mm, defaults.layer_height_mm);

        plan_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (validate_plan(true))
                operate_stage(StageTestAction::MoveToStart);
        });
        adjust_stage_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::MoveToStart);
        });
        relative_stage_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::MoveRelative);
        });
        jog_down_10_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::JogNegative10);
        });
        jog_up_10_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::JogPositive10);
        });
        send_home_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::SendHome);
        });
        read_position_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            operate_stage(StageTestAction::ReadPosition);
        });
        stage_stop_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { stop_stage_test(); });
        max_height_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_automatic_start_height(); });
        deadzone_thickness_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_automatic_start_height(); });
        layer_height_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_automatic_pump_height(); });
        pump_height_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { pump_height_automatic_ = false; });
        led_off_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { turn_led_off(false); });
        start_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { begin_print(); });
        stop_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { request_pause("Stop requested by operator"); });
        continue_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { request_continue(); });
        quit_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { request_quit(); });
        progress_timer_ = std::make_unique<wxTimer>(owner_);
        owner_->Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
            if (running_ && !pause_.load())
                update_progress(layers_done_, last_layer_count_);
        });
        update_controls();
        update_pump_height_lock();
        append_log("Print panel ready. Review settings, validate the plan, then start the print.");
    }

    ~Impl() { shutdown(); }

    bool is_running() const { return running_; }

    void start_print(const std::string &image_directory, bool sliced_or_resliced)
    {
        apply_settings(settings_from_saved_config(image_directory));
        if (sliced_or_resliced) {
            apply_sliced_layer_geometry();
        } else {
            append_log(wxString::Format(
                "Restored saved layer height: %.3f mm (unchanged because no new slice was created)",
                cached_.layer_height_mm).ToStdString());
        }
        if (validate_plan(false))
            append_log("Review settings and validate the plan, then click Start Print.");
        else
            append_log("Choose a folder that contains layer_<number>.png images, then Validate Plan or Start Print.");
        turn_led_off(true);
    }

    void shutdown()
    {
        persist_form_settings(false, false);
        if (progress_timer_)
            progress_timer_->Stop();
        pause_ = false;
        cancel_ = true;
        if (!worker_.joinable())
            return;
        // The print thread blocks on CallAfter for projector updates. Drain those
        // on this GUI thread before joining, otherwise destructor deadlocks.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while ((running_ || adjusting_stage_ || controlling_led_) &&
               std::chrono::steady_clock::now() < deadline) {
            wxYield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (worker_.joinable())
            worker_.join();
        log_file_.close();
    }

private:
    wxTextCtrl *text(const std::string &value, int width = SETTING_FIELD_WIDTH,
                     wxWindow *parent = nullptr)
    {
        auto *control = new wxTextCtrl(parent == nullptr ? owner_ : parent, wxID_ANY,
                                       wxString::FromUTF8(value),
                                       wxDefaultPosition, wxSize(width, -1));
        constrain_width(control, width);
        return control;
    }
    wxTextCtrl *number(double value, wxWindow *parent = nullptr, int width = SETTING_FIELD_WIDTH)
    {
        auto *control = new wxTextCtrl(parent == nullptr ? owner_ : parent, wxID_ANY,
                                       wxString::Format("%.3f", value),
                                       wxDefaultPosition, wxSize(width, -1));
        constrain_width(control, width);
        return control;
    }
    wxSpinCtrl *spin(int value, int low, int high, wxWindow *parent = nullptr)
    {
        auto *control = new wxSpinCtrl(parent == nullptr ? owner_ : parent, wxID_ANY,
                                       wxEmptyString, wxDefaultPosition,
                                       wxSize(SETTING_FIELD_WIDTH, -1));
        control->SetRange(low, high);
        control->SetValue(value);
        constrain_width(control, SETTING_FIELD_WIDTH);
        return control;
    }
    static void constrain_width(wxWindow *control, int width)
    {
        control->SetMinSize(wxSize(width, -1));
        control->SetMaxSize(wxSize(width, -1));
    }
    static void add_row(wxFlexGridSizer *form, const char *label, wxWindow *control)
    {
        form->Add(new wxStaticText(control->GetParent(), wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        form->Add(control, 0, wxALIGN_CENTER_VERTICAL);
    }
    static void add_pair(wxFlexGridSizer *form, const char *left_label, wxWindow *left,
                         const char *right_label, wxWindow *right)
    {
        add_row(form, left_label, left);
        add_row(form, right_label, right);
    }

    void update_pump_height_lock()
    {
        pump_height_->Enable(pumping_->GetValue());
        pump_acceleration_->Enable(pumping_->GetValue());
    }

    void update_automatic_pump_height()
    {
        if (!pump_height_automatic_)
            return;

        double layer_height = 0.0;
        if (!layer_height_->GetValue().ToDouble(&layer_height) ||
            !std::isfinite(layer_height) || layer_height <= 0.0)
            return;

        cached_.pump_height_mm = 2.0 * layer_height;
        // ChangeValue intentionally avoids wxEVT_TEXT: this derived update must
        // not turn the automatic value into a user customization.
        pump_height_->ChangeValue(wxString::Format("%.3f", cached_.pump_height_mm));
    }

    void update_automatic_start_height()
    {
        double printer_height = 0.0;
        double deadzone = 0.0;
        if (max_height_->GetValue().ToDouble(&printer_height) &&
            deadzone_thickness_->GetValue().ToDouble(&deadzone) &&
            std::isfinite(printer_height) && std::isfinite(deadzone) &&
            printer_height > 0.0 && deadzone >= 0.0 && deadzone <= 0.5 &&
            deadzone <= printer_height) {
            const double start_height = printer_height - deadzone;
            start_position_->SetValue(wxString::Format("%.3f", start_height));
            automatic_start_height_->SetLabel(
                wxString::Format("Automatic start: %.3f mm", start_height));
        } else {
            start_position_->SetValue("—");
            automatic_start_height_->SetLabel(_L("Automatic start: — mm"));
        }
    }

    double parse(wxTextCtrl *field, const char *name)
    {
        double value = 0.;
        if (!field->GetValue().ToDouble(&value))
            throw std::runtime_error(std::string("Invalid ") + name);
        return value;
    }

    void apply_settings(const pp::Settings &settings)
    {
        pump_height_automatic_ = saved_pump_height_is_automatic();
        image_dir_->SetPath(wxString::FromUTF8(settings.image_directory.string()));
        stage_port_->SetValue(wxString::FromUTF8(settings.stage_port));
        display_index_->SetValue(settings.display_index);
        return_height_->SetValue(wxString::Format("%.3f", settings.home_position_mm));
        max_height_->SetValue(wxString::Format("%.3f", settings.max_height_mm));
        deadzone_thickness_->SetValue(wxString::Format("%.3f", settings.deadzone_thickness_mm));
        start_position_->SetValue(wxString::Format("%.3f", pp::automatic_start_height_mm(settings)));
        automatic_start_height_->SetLabel(wxString::Format(
            "Automatic start: %.3f mm", pp::automatic_start_height_mm(settings)));
        layer_height_->SetValue(wxString::Format("%.3f", settings.layer_height_mm));
        stage_velocity_->SetValue(wxString::Format("%.3f", settings.stage_max_velocity_mm_s));
        stage_acceleration_->SetValue(wxString::Format("%.3f", settings.stage_acceleration_mm_s2));
        dark_time_->SetValue(wxString::Format("%.3f", settings.dark_time_seconds));
        exposure_->SetValue(wxString::Format("%.3f", settings.exposure_seconds));
        initial_exposure_->SetValue(wxString::Format("%.3f", settings.initial_exposure_seconds));
        uv_intensity_->SetValue(settings.uv_intensity);
        initial_uv_intensity_->SetValue(settings.initial_uv_intensity);
        pumping_->SetValue(settings.pumping_enabled);
        pump_height_->ChangeValue(wxString::Format("%.3f", settings.pump_height_mm));
        pump_acceleration_->SetValue(wxString::Format("%.3f", settings.pump_acceleration_mm_s2));
        fullscreen_->SetValue(settings.fullscreen);
        scale_->SetValue(settings.scale_to_display);
        cached_ = settings;
        update_pump_height_lock();
    }

    void apply_sliced_layer_geometry()
    {
        Plater *plater = wxGetApp().plater();
        if (plater == nullptr)
            return;

        const DynamicPrintConfig &print_config = plater->active_sla_print().full_print_config();
        const bool has_persisted_settings = has_persisted_print_panel_settings();
        if (!has_persisted_settings) {
            if (const ConfigOptionInt *intensity = print_config.opt<ConfigOptionInt>("dlp_uv_intensity")) {
                cached_.uv_intensity = intensity->value;
                uv_intensity_->SetValue(cached_.uv_intensity);
                append_log(wxString::Format("UV intensity from active material preset: %d/255 PWM",
                                            cached_.uv_intensity).ToStdString());
            }
            if (const ConfigOptionInt *intensity =
                    print_config.opt<ConfigOptionInt>("dlp_initial_exposure_intensity")) {
                cached_.initial_uv_intensity = intensity->value;
                initial_uv_intensity_->SetValue(cached_.initial_uv_intensity);
                append_log(wxString::Format(
                    "First-layer UV intensity from active material preset: %d/255 PWM",
                    cached_.initial_uv_intensity).ToStdString());
            }
        }

        const double sliced = plater->active_sla_print().default_object_config().layer_height.value;
        if (!std::isfinite(sliced) || sliced <= 0.0)
            return;
        cached_.layer_height_mm = sliced;
        layer_height_->SetValue(wxString::Format("%.3f", sliced));
        remember_native_dlp_sliced_layer_height(sliced);
        if (pump_height_automatic_) {
            cached_.pump_height_mm = 2.0 * sliced;
            pump_height_->ChangeValue(wxString::Format("%.3f", cached_.pump_height_mm));
            append_log(wxString::Format("Layer height from slices: %.3f mm; automatic pump height: %.3f mm",
                                        sliced, cached_.pump_height_mm).ToStdString());
        } else {
            append_log(wxString::Format(
                "Layer height updated from slices: %.3f mm; custom pump height preserved: %.3f mm",
                sliced, cached_.pump_height_mm).ToStdString());
        }
    }

    pp::Settings settings_from_form()
    {
        pp::Settings settings = cached_;
        settings.image_directory = into_u8(image_dir_->GetPath());
        settings.stage_port = into_u8(stage_port_->GetValue());
        settings.display_index = display_index_->GetValue();
        settings.home_position_mm = parse(return_height_, "return height");
        settings.max_height_mm = parse(max_height_, "maximum height");
        settings.deadzone_thickness_mm = parse(deadzone_thickness_, "deadzone thickness");
        pp::automatic_start_height_mm(settings);
        settings.layer_height_mm = parse(layer_height_, "layer height");
        settings.stage_max_velocity_mm_s = parse(stage_velocity_, "stage maximum velocity");
        settings.stage_acceleration_mm_s2 = parse(stage_acceleration_, "stage acceleration");
        settings.dark_time_seconds = parse(dark_time_, "dark time");
        settings.exposure_seconds = parse(exposure_, "exposure");
        settings.initial_exposure_seconds = parse(initial_exposure_, "initial exposure");
        settings.uv_intensity = uv_intensity_->GetValue();
        settings.initial_uv_intensity = initial_uv_intensity_->GetValue();
        settings.pumping_enabled = pumping_->GetValue();
        settings.pump_height_mm = parse(pump_height_, "pump height");
        settings.pump_acceleration_mm_s2 = parse(pump_acceleration_, "pump acceleration");
        settings.fullscreen = fullscreen_->GetValue();
        settings.scale_to_display = scale_->GetValue();
        return settings;
    }

    bool persist_form_settings(bool announce, bool report_error)
    {
        try {
            const pp::Settings settings = settings_from_form();
            if (settings.dark_time_seconds < 0.0 || settings.exposure_seconds < 0.0 ||
                settings.initial_exposure_seconds < 0.0)
                throw std::runtime_error("Dark time and exposure times must be non-negative");
            if (settings.pumping_enabled) {
                const double minimum_dark = pp::minimum_pump_dark_time_seconds(settings);
                if (settings.dark_time_seconds + 1e-9 < minimum_dark)
                    throw std::runtime_error(Slic3r::format(
                        "Dark time must be at least %.6f seconds for the configured pumping motion",
                        minimum_dark));
            }
            if (wxTheApp == nullptr || wxGetApp().app_config == nullptr)
                throw std::runtime_error("PrusaSlicer settings storage is unavailable");

            PrinterConfig saved {
                { "STAGE_PORT", into_u8(stage_port_->GetValue()) },
                { "DISPLAY_INDEX", std::to_string(display_index_->GetValue()) },
                { "HOME_POSITION_MM", into_u8(return_height_->GetValue()) },
                { "STAGE_MAX_HEIGHT_MM", into_u8(max_height_->GetValue()) },
                { "DEADZONE_THICKNESS_MM", into_u8(deadzone_thickness_->GetValue()) },
                { "LAYER_HEIGHT_MM", into_u8(layer_height_->GetValue()) },
                { "STAGE_MAX_VELOCITY_MM_S", into_u8(stage_velocity_->GetValue()) },
                { "STAGE_ACCELERATION_MM_S2", into_u8(stage_acceleration_->GetValue()) },
                { "DARK_TIME_SECONDS", into_u8(dark_time_->GetValue()) },
                { "EXPOSURE_SECONDS", into_u8(exposure_->GetValue()) },
                { "INITIAL_EXPOSURE_SECONDS", into_u8(initial_exposure_->GetValue()) },
                { "UV_INTENSITY", std::to_string(uv_intensity_->GetValue()) },
                { "INITIAL_UV_INTENSITY", std::to_string(initial_uv_intensity_->GetValue()) },
                { "PUMPING_ENABLED", pumping_->GetValue() ? "Yes" : "No" },
                { "PUMP_HEIGHT_MM", into_u8(pump_height_->GetValue()) },
                { PUMP_HEIGHT_AUTOMATIC_KEY, pump_height_automatic_ ? "Yes" : "No" },
                { "PUMP_ACCELERATION_MM_S2", into_u8(pump_acceleration_->GetValue()) },
                { "FULLSCREEN", fullscreen_->GetValue() ? "Yes" : "No" },
                { "SCALE_TO_DISPLAY", scale_->GetValue() ? "Yes" : "No" },
            };
            wxGetApp().app_config->set_section(PRINT_PANEL_CONFIG_SECTION, std::move(saved));
            wxGetApp().app_config->save();
            cached_ = settings;
            if (announce)
                append_log("Print settings saved.");
            return true;
        } catch (const std::exception &ex) {
            if (report_error)
                show_error(owner_, from_u8(ex.what()));
            return false;
        }
    }

    bool validate_plan(bool announce)
    {
        try {
            const pp::Settings settings = settings_from_form();
            const auto plan = pp::make_plan(settings);
            last_layer_count_ = plan.layers.size();
            estimated_total_seconds_ = pp::estimate_print_seconds(settings, plan.layers.size());
            if (announce)
                append_log(wxString::Format("Plan: %zu layers, %.3f mm to %.3f mm, estimated %s",
                    plan.layers.size(), plan.planned_start_mm, plan.planned_end_mm,
                    format_duration(estimated_total_seconds_)).ToStdString());
            if (announce && settings.pumping_enabled)
                append_log(wxString::Format(
                    "Pumping per layer: %.3f mm peel, then %.3f mm to the next layer at %.3f mm/s²; minimum dark time %.3f s.",
                    settings.pump_height_mm,
                    std::abs(settings.pump_height_mm - settings.layer_height_mm),
                    settings.pump_acceleration_mm_s2,
                    pp::minimum_pump_dark_time_seconds(settings)).ToStdString());
            if (!running_)
                update_progress(0, last_layer_count_);
            motion_graph_->set_layer_count(last_layer_count_);
            persist_form_settings(false, false);
            const std::string directory = settings.image_directory.string();
            if (!directory.empty()) {
                wxGetApp().app_config->set("dlp_last_slice_directory", directory);
                wxGetApp().app_config->save();
            }
            return true;
        } catch (const std::exception &ex) {
            if (announce)
                show_error(owner_, from_u8(ex.what()));
            return false;
        }
    }

    enum class StageTestAction {
        MoveToStart,
        MoveRelative,
        JogNegative10,
        JogPositive10,
        SendHome,
        ReadPosition
    };

    void update_current_height(double height)
    {
        if (std::isfinite(height))
            current_height_->SetLabel(wxString::Format("Current height: %.3f mm", height));
    }

    void stop_stage_test()
    {
        if (!adjusting_stage_)
            return;
        cancel_ = true;
        stage_stop_->Disable();
        append_log("Stop Stage requested; stopping the active test move");
    }

    void operate_stage(StageTestAction action)
    {
        if (running_ || adjusting_stage_ || controlling_led_)
            return;

        pp::Settings settings;
        double relative_distance = 0.0;
        const bool relative = action == StageTestAction::MoveRelative ||
                              action == StageTestAction::JogNegative10 ||
                              action == StageTestAction::JogPositive10;
        try {
            settings = settings_from_form();
            if (settings.stage_port.empty())
                throw std::runtime_error("Enter the SMC100CC serial port before adjusting the stage");
            if (action == StageTestAction::MoveToStart)
                pp::automatic_start_height_mm(settings);
            if (relative) {
                if (action == StageTestAction::JogNegative10)
                    relative_distance = -10.0;
                else if (action == StageTestAction::JogPositive10)
                    relative_distance = 10.0;
                else
                    relative_distance = parse(relative_move_, "relative movement");
                if (!std::isfinite(relative_distance) || relative_distance == 0.0)
                    throw std::runtime_error("Relative movement must be a non-zero positive or negative distance in mm");
            }
            if (settings.stage_max_velocity_mm_s <= 0.0 || settings.stage_acceleration_mm_s2 <= 0.0)
                throw std::runtime_error("Stage maximum velocity and acceleration must be positive");
        } catch (const std::exception &ex) {
            show_error(owner_, from_u8(ex.what()));
            return;
        }

        if (worker_.joinable())
            worker_.join();
        cancel_ = false;
        adjusting_stage_ = true;
        update_controls();
        motion_graph_->reset(0.0, settings.max_height_mm, settings.layer_height_mm);
        switch (action) {
        case StageTestAction::MoveRelative:
        case StageTestAction::JogNegative10:
        case StageTestAction::JogPositive10:
            append_log(wxString::Format("Moving stage relatively by %+.3f mm", relative_distance).ToStdString());
            break;
        case StageTestAction::MoveToStart:
            append_log(wxString::Format(
                "Moving directly to automatic start height %.3f mm without homing (%.3f mm printer height - %.3f mm deadzone)",
                pp::automatic_start_height_mm(settings), settings.max_height_mm,
                settings.deadzone_thickness_mm).ToStdString());
            break;
        case StageTestAction::SendHome:
            append_log("Sending stage home to exactly 0.000 mm");
            break;
        case StageTestAction::ReadPosition:
            append_log("Reading current stage encoder height without commanding motion");
            break;
        }

        worker_ = std::thread([this, settings, action, relative, relative_distance] {
            const auto logger = [this](const std::string &line) {
                wxTheApp->CallAfter([this, line] { append_log(line); });
            };
            try {
                pp::SMC100CC stage(settings.stage_port, settings.controller_address,
                                   settings.baud_rate, settings.xon_xoff, cancel_, logger,
                    [this](double seconds, double height, pp::MotionPhase phase, std::size_t layer) {
                        wxTheApp->CallAfter([this, seconds, height, phase, layer] {
                            motion_graph_->add_sample(seconds, height, phase, layer);
                            update_current_height(height);
                        });
                    });
                try {
                    stage.open();
                    if (action == StageTestAction::ReadPosition) {
                        const double current = stage.get_position();
                        stage.close();
                        wxTheApp->CallAfter([this, current] {
                            update_current_height(current);
                            append_log(wxString::Format(
                                "Current stage encoder height: %.3f mm", current).ToStdString());
                            adjusting_stage_ = false;
                            update_controls();
                        });
                        return;
                    }
                    if (action == StageTestAction::SendHome)
                        stage.initialize();
                    else
                        stage.require_referenced();
                    stage.configure_motion(settings.stage_max_velocity_mm_s,
                                           settings.stage_acceleration_mm_s2);
                    const double current = stage.get_position();
                    const double target = relative ?
                        current + relative_distance : action == StageTestAction::SendHome ?
                        0.0 : pp::automatic_start_height_mm(settings);
                    if (!std::isfinite(target) || target < 0.0 || target > settings.max_height_mm) {
                        if (relative)
                            throw std::runtime_error(
                                "Relative move would place the stage outside 0 mm and the configured maximum height");
                        if (action == StageTestAction::SendHome)
                            throw std::runtime_error(
                                "The 0 mm home target is outside the configured stage range");
                        throw std::runtime_error(
                            "Start height is outside 0 mm and the configured maximum height");
                    }
                    if (action == StageTestAction::MoveToStart) {
                        constexpr double deceleration_distance_mm = 1.0;
                        if (const auto waypoint = pp::initial_deceleration_waypoint(
                                current, target, deceleration_distance_mm)) {
                            logger(wxString::Format(
                                "Initial deceleration: stopping at %.3f mm before the final 1.000 mm approach to the start height",
                                *waypoint).ToStdString());
                            stage.move_absolute(*waypoint,
                                "Initial deceleration before print start height");
                        }
                    }
                    const double actual = stage.move_absolute_precise(
                        target, settings.position_tolerance_mm,
                        settings.settle_seconds, settings.correction_attempts,
                        settings.max_correction_mm,
                        relative ?
                            "Move stage relative for print alignment" :
                        action == StageTestAction::SendHome ?
                            "Send stage home to 0 mm" :
                            "Adjust stage to print start height");
                    stage.close();
                    wxTheApp->CallAfter([this, actual, action, relative, relative_distance] {
                        update_current_height(actual);
                        if (relative)
                            append_log(wxString::Format(
                                "Relative move %+.3f mm complete; stage aligned at %.3f mm.",
                                relative_distance, actual).ToStdString());
                        else if (action == StageTestAction::SendHome)
                            append_log(wxString::Format(
                                "Send Home complete; stage is at %.3f mm.", actual).ToStdString());
                        else
                            append_log(wxString::Format(
                                "Stage aligned at %.3f mm; it will remain there until printing or another move.",
                                actual).ToStdString());
                        adjusting_stage_ = false;
                        update_controls();
                    });
                } catch (...) {
                    stage.stop_if_moving();
                    stage.close();
                    throw;
                }
            } catch (const std::exception &ex) {
                const std::string error = ex.what();
                wxTheApp->CallAfter([this, error] {
                    const bool cancelled = cancel_.load();
                    append_log(cancelled ? "Stage adjustment cancelled." :
                                             "Stage adjustment failed: " + error);
                    adjusting_stage_ = false;
                    update_controls();
                    if (!cancelled)
                        show_error(owner_, from_u8(error));
                });
            }
        });
    }

    void turn_led_off(bool automatic)
    {
        if (running_ || adjusting_stage_ || controlling_led_)
            return;

        const pp::Settings settings = cached_;
        if (worker_.joinable())
            worker_.join();
        cancel_ = false;
        controlling_led_ = true;
        update_controls();
        append_log(automatic ?
            "Connecting to the light engine automatically to force the UV LED board OFF…" :
            "Turning the separate UV LED board off through the light-engine enable output…");

        worker_ = std::thread([this, settings, automatic] {
            const auto logger = [this](const std::string &line) {
                wxTheApp->CallAfter([this, line] { append_log(line); });
            };
            pp::DLPC900 controller(settings.projector_vid, settings.projector_pid, logger);
            try {
                controller.open();
                controller.close();
                wxTheApp->CallAfter([this] {
                    append_log("External UV LED board verified OFF through the light engine. The configured print PWM is unchanged and will be restored at the first exposure.");
                    controlling_led_ = false;
                    update_controls();
                });
            } catch (const std::exception &ex) {
                controller.close();
                const std::string error = ex.what();
                wxTheApp->CallAfter([this, error, automatic] {
                    append_log("Could not turn the light-engine LED off: " + error);
                    controlling_led_ = false;
                    update_controls();
                    if (!automatic)
                        show_error(owner_, from_u8(error));
                });
            }
        });
    }

    void begin_print()
    {
        if (running_ || adjusting_stage_ || controlling_led_ || !validate_plan(true))
            return;

        const pp::Settings selected = settings_from_form();
        if (worker_.joinable())
            worker_.join();
        cancel_ = false;
        pause_ = false;
        return_to_plater_ = false;
        running_ = true;
        print_started_ = std::chrono::steady_clock::now();
        paused_total_ = {};
        pause_started_ = {};
        layers_done_ = 0;
        update_controls();
        update_progress(0, last_layer_count_);
        if (progress_timer_)
            progress_timer_->Start(100);
        clear_preview();
        motion_graph_->reset(0.0, selected.max_height_mm, selected.layer_height_mm);
        motion_graph_->set_layer_count(last_layer_count_);
        open_log_file();
        dlp::debug_log(Slic3r::format(
            "GUI DLP print: starting native AirPrint image_dir=%1% first_uv_intensity=%2%/255 normal_uv_intensity=%3%/255",
            selected.image_directory.string(), selected.initial_uv_intensity,
            selected.uv_intensity));
        BOOST_LOG_TRIVIAL(info) << "DLP print: starting native AirPrint for "
                                << selected.image_directory.string() << " at first-layer UV intensity "
                                << selected.initial_uv_intensity << "/255 and normal UV intensity "
                                << selected.uv_intensity << "/255 PWM";
        worker_ = std::thread([this, selected] {
            try {
                pp::AirPrintRunner runner(selected, projection_, cancel_, pause_,
                    [this](const std::string &line) { wxTheApp->CallAfter([this, line] { append_log(line); }); },
                    [this](std::size_t done, std::size_t total, const std::filesystem::path &path) {
                        wxTheApp->CallAfter([this, done, total, path] {
                            layers_done_ = done;
                            last_layer_count_ = total;
                            update_progress(done, total);
                            show_preview(path, done, total);
                        });
                    },
                    [this](double seconds, double height, pp::MotionPhase phase, std::size_t layer) {
                        wxTheApp->CallAfter([this, seconds, height, phase, layer] {
                            motion_graph_->add_sample(std::max(0.0, seconds - paused_seconds()), height, phase, layer);
                            update_current_height(height);
                        });
                    });
                runner.run();
                wxTheApp->CallAfter([this] { finish("Print completed safely.", false); });
            } catch (const std::exception &ex) {
                const std::string error = ex.what();
                wxTheApp->CallAfter([this, error] {
                    if (return_to_plater_)
                        finish("Print stopped. Stage returned to return height.", false);
                    else
                        finish("Print stopped: " + error, true);
                });
            }
        });
    }

    void request_pause(const std::string &reason)
    {
        if (!running_ || cancel_)
            return;
        if (pause_.exchange(true))
            return;
        pause_started_ = std::chrono::steady_clock::now();
        if (progress_timer_)
            progress_timer_->Stop();
        append_log(reason);
        update_controls();
        update_progress(layers_done_, last_layer_count_);
    }

    void request_continue()
    {
        if (!running_ || !pause_ || cancel_)
            return;
        paused_total_ += std::chrono::steady_clock::now() - pause_started_;
        pause_ = false;
        append_log("Continue requested");
        if (progress_timer_)
            progress_timer_->Start(100);
        update_controls();
        update_progress(layers_done_, last_layer_count_);
    }

    void request_quit()
    {
        if (adjusting_stage_ || controlling_led_)
            return;
        if (running_) {
            if (!pause_.load() || cancel_.load())
                return;
            return_to_plater_ = true;
            pause_ = false;
            cancel_ = true;
            append_log("Quit requested; returning stage to return height");
            update_controls();
            return;
        }
        if (!persist_form_settings(true, true))
            return;
        append_log("Returning to plater");
        if (Plater *plater = wxGetApp().plater(); plater != nullptr)
            plater->select_view_3D("3D");
    }

    void finish(const std::string &message, bool error)
    {
        if (progress_timer_)
            progress_timer_->Stop();
        append_log(message);
        running_ = false;
        pause_ = false;
        update_controls();
        update_progress(layers_done_, last_layer_count_);
        if (error)
            wxBell();
        if (return_to_plater_) {
            return_to_plater_ = false;
            if (Plater *plater = wxGetApp().plater(); plater != nullptr)
                plater->select_view_3D("3D");
        }
    }

    void update_controls()
    {
        const bool running = running_.load();
        const bool adjusting = adjusting_stage_.load();
        const bool controlling_led = controlling_led_.load();
        const bool busy = running || adjusting || controlling_led;
        const bool cancelling = running && cancel_.load();
        const bool paused = running && pause_.load() && !cancelling;
        plan_->Show(!running);
        start_->Show(!running);
        plan_->Enable(!busy);
        start_->Enable(!busy);
        adjust_stage_->Enable(!busy);
        relative_stage_->Enable(!busy);
        jog_down_10_->Enable(!busy);
        jog_up_10_->Enable(!busy);
        send_home_->Enable(!busy);
        read_position_->Enable(!busy);
        stage_stop_->Enable(adjusting && !cancel_.load());
        led_off_->Show(!running);
        led_off_->Enable(!busy);
        led_off_->SetLabel(controlling_led ? _L("Turning Off…") : _L("LED Off"));
        adjust_stage_->SetLabel(adjusting ? _L("Stage Busy…") : _L("Move to Start"));
        stop_->Show(running && !paused && !cancelling);
        stop_->Enable(running && !paused && !cancelling);
        continue_->Show(paused);
        continue_->Enable(paused);
        quit_->Show(!running || paused);
        quit_->Enable(!adjusting && !controlling_led && (!running || paused));
        settings_notebook_->Show(!running);
        set_preview_expanded(running);
        progress_bar_->set_paused(paused);
        motion_graph_->set_paused(paused);
        root_sizer_->Layout();
        owner_->Layout();
    }

    double paused_seconds() const
    {
        auto paused = paused_total_;
        if (running_ && pause_.load())
            paused += std::chrono::steady_clock::now() - pause_started_;
        return std::chrono::duration<double>(paused).count();
    }

    double elapsed_seconds() const
    {
        if (!running_)
            return 0.0;
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - print_started_).count() - paused_seconds();
        return std::max(0.0, elapsed);
    }

    void update_progress(std::size_t done, std::size_t total)
    {
        const double elapsed = elapsed_seconds();
        double fraction = 0.0;
        if (running_ && estimated_total_seconds_ > 0.0)
            fraction = std::clamp(elapsed / estimated_total_seconds_, 0.0, 1.0);
        else if (!running_ && total > 0 && done >= total)
            fraction = 1.0;
        progress_bar_->set_fraction(fraction);
        if (total == 0) {
            progress_label_->SetLabel(_L("Validate the plan to see layer count and time estimate"));
            return;
        }
        const int percent = int(std::lround(100.0 * fraction));
        wxString label = wxString::Format("%zu / %zu layers   %d%%", done, total, percent);
        if (running_) {
            const double remaining = std::max(0.0, estimated_total_seconds_ - elapsed);
            if (pause_.load() && !cancel_.load())
                label += "   PAUSED";
            label += wxString::Format("   elapsed %s   ~%s remaining",
                format_duration(elapsed), format_duration(remaining));
        } else {
            label += wxString::Format("   estimated %s", format_duration(estimated_total_seconds_));
        }
        progress_label_->SetLabel(label);
    }

    void append_log(const std::string &line)
    {
        log_->AppendText(wxString::FromUTF8(line) + "\n");
        if (log_file_.is_open()) {
            log_file_ << line << '\n';
            log_file_.flush();
        }
    }

    void open_log_file()
    {
        log_file_.close();
#ifdef SLIC3R_DLP_FILE_LOG
        const boost::filesystem::path printer = printer_config_directory();
        if (printer.empty())
            return;
        const boost::filesystem::path log_dir = printer.parent_path() / "logs";
        boost::system::error_code error;
        boost::filesystem::create_directories(log_dir, error);
        const boost::filesystem::path log_path = log_dir / "airprint.log";
        log_file_.open(log_path.string(), std::ios::out | std::ios::trunc);
        if (log_file_.is_open())
            append_log("Writing print log to " + log_path.string());
#endif
    }

    wxImage preview_canvas() const
    {
        wxImage canvas(preview_width_, preview_height_);
        canvas.SetRGB(wxRect(0, 0, preview_width_, preview_height_), 0, 0, 0);
        return canvas;
    }

    void clear_preview()
    {
        preview_path_.clear();
        preview_index_ = 0;
        preview_total_ = 0;
        preview_bitmap_->SetBitmap(wxBitmap(preview_canvas()));
        preview_label_->SetLabel(_L("Waiting for first slice"));
    }

    void show_preview(const std::filesystem::path &path, std::size_t index, std::size_t total)
    {
        preview_path_ = path;
        preview_index_ = index;
        preview_total_ = total;
        wxImage image(wxString::FromUTF8(path.string()));
        if (!image.IsOk()) {
            preview_label_->SetLabel("Could not load " + wxString::FromUTF8(path.filename().string()));
            return;
        }
        const double scale = std::min(double(preview_width_) / image.GetWidth(),
                                      double(preview_height_) / image.GetHeight());
        const int width = std::max(1, int(std::lround(image.GetWidth() * scale)));
        const int height = std::max(1, int(std::lround(image.GetHeight() * scale)));
        image.Rescale(width, height, wxIMAGE_QUALITY_NEAREST);
        wxImage canvas = preview_canvas();
        canvas.Paste(image, (preview_width_ - width) / 2, (preview_height_ - height) / 2);
        preview_bitmap_->SetBitmap(wxBitmap(canvas));
        preview_label_->SetLabel(wxString::Format("Slice %zu/%zu — %s", index, total,
            wxString::FromUTF8(path.filename().string())));
        preview_label_->Wrap(preview_width_);
    }

    void set_preview_expanded(bool expanded)
    {
        preview_width_ = expanded ? EXPANDED_PREVIEW_WIDTH : PREVIEW_WIDTH;
        preview_height_ = expanded ? EXPANDED_PREVIEW_HEIGHT : PREVIEW_HEIGHT;
        preview_bitmap_->SetMinSize(wxSize(preview_width_, preview_height_));
        if (preview_path_.empty())
            clear_preview();
        else
            show_preview(preview_path_, preview_index_, preview_total_);
    }

    DLPPrintPanel *owner_ {};
    wxDirPickerCtrl *image_dir_ {};
    wxTextCtrl *stage_port_ {}, *return_height_ {}, *max_height_ {}, *deadzone_thickness_ {};
    wxTextCtrl *start_position_ {}, *layer_height_ {};
    wxTextCtrl *stage_velocity_ {}, *stage_acceleration_ {}, *relative_move_ {};
    wxTextCtrl *dark_time_ {}, *exposure_ {}, *initial_exposure_ {};
    wxTextCtrl *pump_height_ {}, *pump_acceleration_ {}, *log_ {};
    wxSpinCtrl *display_index_ {}, *uv_intensity_ {}, *initial_uv_intensity_ {};
    wxCheckBox *pumping_ {}, *fullscreen_ {}, *scale_ {};
    wxButton *plan_ {}, *start_ {}, *led_off_ {}, *adjust_stage_ {}, *relative_stage_ {};
    wxButton *jog_down_10_ {}, *jog_up_10_ {};
    wxButton *send_home_ {}, *read_position_ {}, *stage_stop_ {};
    wxButton *stop_ {}, *continue_ {}, *quit_ {};
    ProgressBarPanel *progress_bar_ {};
    wxStaticText *progress_label_ {};
    MotionGraphPanel *motion_graph_ {};
    wxStaticBitmap *preview_bitmap_ {};
    wxStaticText *preview_label_ {}, *current_height_ {}, *automatic_start_height_ {};
    wxNotebook *settings_notebook_ {};
    wxBoxSizer *root_sizer_ {};
    wxBoxSizer *settings_sizer_ {};
    wxFlexGridSizer *path_sizer_ {};
    wxFlexGridSizer *config_sizer_ {};
    wxStaticLine *config_separator_ {};
    int preview_width_ { PREVIEW_WIDTH };
    int preview_height_ { PREVIEW_HEIGHT };
    std::filesystem::path preview_path_;
    std::size_t preview_index_ { 0 };
    std::size_t preview_total_ { 0 };
    std::size_t last_layer_count_ { 0 };
    std::size_t layers_done_ { 0 };
    double estimated_total_seconds_ { 0.0 };
    std::chrono::steady_clock::time_point print_started_ {};
    std::chrono::steady_clock::time_point pause_started_ {};
    std::chrono::steady_clock::duration paused_total_ {};
    std::atomic_bool cancel_ { false };
    std::atomic_bool pause_ { false };
    std::atomic_bool running_ { false };
    std::atomic_bool adjusting_stage_ { false };
    std::atomic_bool controlling_led_ { false };
    std::atomic_bool return_to_plater_ { false };
    bool pump_height_automatic_ { true };
    std::unique_ptr<wxTimer> progress_timer_;
    std::thread worker_;
    std::ofstream log_file_;
    pp::Settings cached_ { pp::default_settings() };
    GuiProjection projection_;
};

DLPPrintPanel::DLPPrintPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , impl_(std::make_unique<Impl>(this))
{}

DLPPrintPanel::~DLPPrintPanel() = default;

void DLPPrintPanel::start_print(const std::string &image_directory, bool sliced_or_resliced)
{
    impl_->start_print(image_directory, sliced_or_resliced);
}

bool DLPPrintPanel::is_running() const
{
    return impl_->is_running();
}

}} // namespace Slic3r::GUI
