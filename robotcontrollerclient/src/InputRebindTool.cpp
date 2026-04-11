/*
 * input_rebind_tool.cpp
 *
 * GTK3 + SDL2 GUI application for joystick/controller input rebinding.
 * Detects connected joysticks via SDL2, shows live axis/button state in
 * Cairo-rendered widgets, lets the user assign each input to a robot action
 * via dropdown menus, and saves/loads JSON config files that control.cpp
 * can consume via InputMapper.hpp.
 *
 * Build (add to CMakeLists.txt as a second target):
 *   add_executable(input_rebind_tool src/input_rebind_tool.cpp)
 *   target_link_libraries(input_rebind_tool SDL2 ${GTKMM_LIBRARIES} ${CAIRO_LIBRARIES} pthread)
 *
 * Usage:
 *   ./input_rebind_tool                           # Fresh start
 *   ./input_rebind_tool --load controller_config.json  # Edit existing config
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>

#include <SDL2/SDL.h>
#include <gtkmm.h>
#include <gdkmm.h>
#include <cairomm/context.h>

#include "InputMapper.hpp"

// ─── Action Lists ─────────────────────────────────────────────────────────────

static const std::vector<std::string> AXIS_ACTIONS = {
    "(unmapped)",
    "Roll",
    "Pitch",
    "Arm",
    "Bucket",
    "Left Speed",
    "Right Speed",
    "Throttle",
    "Yaw",
    "Custom 1",
    "Custom 2",
};

static const std::map<std::string, std::pair<int,int>> ACTION_TO_OUTPUT = {
    {"Roll",        {0, 0}},
    {"Pitch",       {0, 1}},
    {"Arm",         {1, 1}},
    {"Bucket",      {1, 0}},
    {"Left Speed",  {0, 0}},
    {"Right Speed", {1, 0}},
    {"Throttle",    {0, 2}},
    {"Yaw",         {0, 3}},
    {"Custom 1",    {2, 0}},
    {"Custom 2",    {2, 1}},
};

static const std::vector<std::string> BUTTON_ACTIONS = {
    "(unmapped)",
    "E-Stop",
    "Toggle Autonomy",
    "Reset Sensors",
    "Arm Home",
    "Bucket Home",
    "Toggle Camera",
    "Speed Mode",
    "Horn",
    "Light Toggle",
    "Custom Btn 1",
    "Custom Btn 2",
};

// ─── Joystick State ───────────────────────────────────────────────────────────

struct JoystickState {
    SDL_Joystick* handle = nullptr;
    SDL_GameController* controller = nullptr;
    std::string name;
    int index = -1;
    int numAxes = 0;
    int numButtons = 0;
    int numHats = 0;
    bool isGameController = false;
    std::vector<float> axes;
    std::vector<bool> buttons;
    std::vector<int> hats;
};

// ─── Stick Visualizer Widget ──────────────────────────────────────────────────

class StickWidget : public Gtk::DrawingArea {
public:
    StickWidget() {
        set_size_request(120, 120);
    }

    void set_position(float x, float y) {
        x_ = x; y_ = y;
        queue_draw();
    }

    void set_label(const std::string& label) { label_ = label; }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        int w = get_allocated_width();
        int h = get_allocated_height();
        double cx = w / 2.0, cy = h / 2.0;
        double radius = std::min(w, h) / 2.0 - 8;

        // Background circle
        cr->set_source_rgb(0.12, 0.14, 0.18);
        cr->arc(cx, cy, radius, 0, 2 * M_PI);
        cr->fill();

        // Border
        cr->set_source_rgb(0.25, 0.30, 0.38);
        cr->set_line_width(1.5);
        cr->arc(cx, cy, radius, 0, 2 * M_PI);
        cr->stroke();

        // Crosshair
        cr->set_source_rgba(0.3, 0.35, 0.42, 0.6);
        cr->set_line_width(0.8);
        cr->move_to(cx - radius, cy);
        cr->line_to(cx + radius, cy);
        cr->stroke();
        cr->move_to(cx, cy - radius);
        cr->line_to(cx, cy + radius);
        cr->stroke();

        // Dot
        double dx = cx + x_ * (radius - 8);
        double dy = cy + y_ * (radius - 8);
        cr->set_source_rgb(0.22, 0.55, 1.0);
        cr->arc(dx, dy, 8, 0, 2 * M_PI);
        cr->fill();

        // White ring on dot
        cr->set_source_rgb(1, 1, 1);
        cr->set_line_width(1.5);
        cr->arc(dx, dy, 8, 0, 2 * M_PI);
        cr->stroke();

        // Label
        cr->set_source_rgb(0.6, 0.65, 0.7);
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(10);
        Cairo::TextExtents te;
        cr->get_text_extents(label_, te);
        cr->move_to(cx - te.width / 2, cy + radius + 14);
        cr->show_text(label_);

        return true;
    }

private:
    float x_ = 0, y_ = 0;
    std::string label_;
};

// ─── Axis Bar Widget ──────────────────────────────────────────────────────────

class AxisBarWidget : public Gtk::DrawingArea {
public:
    AxisBarWidget() {
        set_size_request(200, 22);
    }

    void set_value(float v) { value_ = v; queue_draw(); }
    void set_label(const std::string& l) { label_ = l; }
    void set_active(bool a) { active_ = a; queue_draw(); }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        int w = get_allocated_width();
        int h = get_allocated_height();

        // Background
        cr->set_source_rgb(0.12, 0.14, 0.18);
        cr->rectangle(0, 0, w, h);
        cr->fill();

        // Bar track
        double trackY = h / 2.0;
        double trackH = 6;
        double margin = 40;
        double trackW = w - margin * 2;

        cr->set_source_rgb(0.2, 0.22, 0.28);
        cr->rectangle(margin, trackY - trackH / 2, trackW, trackH);
        cr->fill();

        // Center line
        cr->set_source_rgb(0.3, 0.35, 0.42);
        cr->set_line_width(1);
        cr->move_to(margin + trackW / 2, trackY - trackH);
        cr->line_to(margin + trackW / 2, trackY + trackH);
        cr->stroke();

        // Value indicator
        double posX = margin + (value_ + 1.0) / 2.0 * trackW;
        if (active_)
            cr->set_source_rgb(0.22, 0.55, 1.0);
        else
            cr->set_source_rgb(0.4, 0.42, 0.48);
        cr->arc(posX, trackY, 5, 0, 2 * M_PI);
        cr->fill();

        // Label
        cr->set_source_rgb(0.55, 0.6, 0.65);
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
        cr->set_font_size(10);
        cr->move_to(2, h / 2.0 + 4);
        cr->show_text(label_);

        // Value text
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", value_);
        Cairo::TextExtents te;
        cr->get_text_extents(buf, te);
        cr->move_to(w - te.width - 2, h / 2.0 + 4);
        cr->show_text(buf);

        return true;
    }

private:
    float value_ = 0;
    std::string label_;
    bool active_ = false;
};

// ─── Button Grid Widget ──────────────────────────────────────────────────────

class ButtonGridWidget : public Gtk::DrawingArea {
public:
    ButtonGridWidget() {
        set_size_request(300, 50);
    }

    void set_states(const std::vector<bool>& states) {
        states_ = states;
        int cols = 12;
        int rows = (states_.size() + cols - 1) / cols;
        set_size_request(300, std::max(50, rows * 32 + 20));
        queue_draw();
    }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        int w = get_allocated_width();
        int h = get_allocated_height();

        cr->set_source_rgb(0.12, 0.14, 0.18);
        cr->rectangle(0, 0, w, h);
        cr->fill();

        if (states_.empty()) return true;

        int cols = 12;
        double cellW = 28, cellH = 26;
        double startX = 8, startY = 8;

        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(10);

        for (size_t i = 0; i < states_.size(); i++) {
            int col = i % cols;
            int row = i / cols;
            double x = startX + col * (cellW + 4);
            double y = startY + row * (cellH + 4);

            double r = 5;
            cr->begin_new_sub_path();
            cr->arc(x + cellW - r, y + r, r, -M_PI / 2, 0);
            cr->arc(x + cellW - r, y + cellH - r, r, 0, M_PI / 2);
            cr->arc(x + r, y + cellH - r, r, M_PI / 2, M_PI);
            cr->arc(x + r, y + r, r, M_PI, 3 * M_PI / 2);
            cr->close_path();

            if (states_[i]) {
                cr->set_source_rgb(0.22, 0.55, 1.0);
                cr->fill_preserve();
                cr->set_source_rgb(1, 1, 1);
                cr->set_line_width(1);
                cr->stroke();
            } else {
                cr->set_source_rgb(0.18, 0.20, 0.25);
                cr->fill_preserve();
                cr->set_source_rgb(0.28, 0.32, 0.38);
                cr->set_line_width(1);
                cr->stroke();
            }

            char buf[4];
            snprintf(buf, sizeof(buf), "%zu", i);
            Cairo::TextExtents te;
            cr->get_text_extents(buf, te);
            if (states_[i])
                cr->set_source_rgb(1, 1, 1);
            else
                cr->set_source_rgb(0.5, 0.55, 0.6);
            cr->move_to(x + (cellW - te.width) / 2, y + (cellH + te.height) / 2);
            cr->show_text(buf);
        }

        return true;
    }

private:
    std::vector<bool> states_;
};

// ─── Main Application Window ─────────────────────────────────────────────────

class RebindWindow : public Gtk::Window {
public:
    RebindWindow(const std::string& loadFile = "")
        : loadFile_(loadFile)
    {
        set_title("Controller Input Rebinding Tool");
        set_default_size(950, 720);

        // Dark theme CSS
        auto css = Gtk::CssProvider::create();
        css->load_from_data(
            "window, box, notebook, scrolledwindow, viewport, frame, grid, label, button, combobox, spinbutton, entry, checkbutton {"
            "  background-color: #0e1117; color: #d4dce8; font-family: 'Proxima Nova', 'Sans'; }"
            "button { border: 1px solid #2a3040; background: #161e28; padding: 6px 14px; border-radius: 4px; }"
            "button:hover { background: #1c2636; border-color: #3a8bfd; }"
            "combobox button { padding: 4px 8px; }"
            "entry, spinbutton { background: #161e28; border: 1px solid #2a3040; padding: 4px; border-radius: 3px; }"
            "notebook tab { background: #161e28; border: 1px solid #2a3040; padding: 6px 16px; }"
            "notebook tab:checked { background: #1c2636; border-bottom-color: #3a8bfd; }"
            "frame { border: 1px solid #1e2838; }"
            ".status-connected { color: #34d399; }"
            ".status-disconnected { color: #f87171; }"
            ".section-title { font-weight: bold; font-size: 13px; color: #8899aa; }"
            ".accent-btn { background: #3a8bfd; border-color: #3a8bfd; color: white; }"
            ".accent-btn:hover { background: #5aa0fd; }"
            ".danger-btn { border-color: #f87171; color: #f87171; }"
            ".danger-btn:hover { background: rgba(248, 113, 113, 0.15); }"
            "separator { background: #1e2838; min-height: 1px; }"
        );
        Gtk::StyleContext::add_provider_for_screen(
            Gdk::Screen::get_default(), css, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        // ── Build UI ──
        auto mainBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
        mainBox->set_border_width(8);

        // Title bar
        auto titleBar = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
        titleBar->set_margin_bottom(8);
        auto titleLabel = Gtk::manage(new Gtk::Label());
        titleLabel->set_markup("<span size='14000' weight='bold'>\xF0\x9F\x8E\xAE  Controller Input Rebinding Tool</span>");
        titleLabel->set_halign(Gtk::ALIGN_START);
        titleBar->pack_start(*titleLabel, Gtk::PACK_EXPAND_WIDGET);

        auto btnLoad = Gtk::manage(new Gtk::Button("Load"));
        btnLoad->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onLoad));
        titleBar->pack_end(*btnLoad, Gtk::PACK_SHRINK);

        auto btnSave = Gtk::manage(new Gtk::Button("Save"));
        btnSave->get_style_context()->add_class("accent-btn");
        btnSave->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onSave));
        titleBar->pack_end(*btnSave, Gtk::PACK_SHRINK);

        mainBox->pack_start(*titleBar, Gtk::PACK_SHRINK);

        // Status bar
        statusBar_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 20));
        statusBar_->set_margin_bottom(6);
        statusLabel_ = Gtk::manage(new Gtk::Label("No controller detected"));
        statusLabel_->get_style_context()->add_class("status-disconnected");
        statusLabel_->set_halign(Gtk::ALIGN_START);
        statusBar_->pack_start(*statusLabel_, Gtk::PACK_SHRINK);

        axesCountLabel_ = Gtk::manage(new Gtk::Label(""));
        statusBar_->pack_start(*axesCountLabel_, Gtk::PACK_SHRINK);
        buttonsCountLabel_ = Gtk::manage(new Gtk::Label(""));
        statusBar_->pack_start(*buttonsCountLabel_, Gtk::PACK_SHRINK);
        mainBox->pack_start(*statusBar_, Gtk::PACK_SHRINK);

        mainBox->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        // Notebook
        notebook_ = Gtk::manage(new Gtk::Notebook());
        notebook_->set_margin_top(4);

        auto monitorPage = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
        monitorPage->set_border_width(8);
        buildMonitorPage(monitorPage);
        notebook_->append_page(*monitorPage, "Live Monitor");

        auto axisPage = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
        axisPage->set_border_width(8);
        buildAxisMappingPage(axisPage);
        notebook_->append_page(*axisPage, "Axis Mappings");

        auto buttonPage = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
        buttonPage->set_border_width(8);
        buildButtonMappingPage(buttonPage);
        notebook_->append_page(*buttonPage, "Button Mappings");

        auto settingsPage = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
        settingsPage->set_border_width(8);
        buildSettingsPage(settingsPage);
        notebook_->append_page(*settingsPage, "Settings");

        mainBox->pack_start(*notebook_, Gtk::PACK_EXPAND_WIDGET);
        add(*mainBox);

        // ── Initialize SDL ──
        if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) != 0) {
            statusLabel_->set_text(std::string("SDL Init failed: ") + SDL_GetError());
        }
        SDL_JoystickEventState(SDL_ENABLE);

        if (!loadFile_.empty()) {
            config_ = InputConfig::loadFromFile(loadFile_);
            syncUIFromConfig();
        }

        openJoysticks();

        pollConnection_ = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &RebindWindow::onPollTimer), 33);

        show_all();
    }

    ~RebindWindow() override {
        pollConnection_.disconnect();
        closeJoysticks();
        SDL_Quit();
    }

private:
    InputConfig config_;
    std::string loadFile_;
    std::vector<JoystickState> joysticks_;
    sigc::connection pollConnection_;

    enum class ListenMode { NONE, AXIS, BUTTON };
    ListenMode listenMode_ = ListenMode::NONE;
    Gtk::Dialog* listenDialog_ = nullptr;

    // Widgets
    Gtk::Label* statusLabel_;
    Gtk::Label* axesCountLabel_;
    Gtk::Label* buttonsCountLabel_;
    Gtk::Box* statusBar_;
    Gtk::Notebook* notebook_;

    Gtk::Box* sticksBox_;
    Gtk::Box* axesBarsBox_;
    ButtonGridWidget* buttonGrid_;
    std::vector<StickWidget*> stickWidgets_;
    std::vector<AxisBarWidget*> axisBarWidgets_;
    bool monitorBuilt_ = false;

    Gtk::Box* axisMappingBox_;
    Gtk::Box* buttonMappingBox_;

    Gtk::SpinButton* deadZoneSpin_;
    Gtk::CheckButton* invertYCheck_;
    Gtk::CheckButton* controllerCheck_;
    Gtk::CheckButton* twoJoysticksCheck_;
    Gtk::CheckButton* altLayoutCheck_;

    // ─── Build Pages ──────────────────────────────────────────────────────

    void buildMonitorPage(Gtk::Box* page) {
        auto sticksLabel = Gtk::manage(new Gtk::Label("Sticks"));
        sticksLabel->get_style_context()->add_class("section-title");
        sticksLabel->set_halign(Gtk::ALIGN_START);
        page->pack_start(*sticksLabel, Gtk::PACK_SHRINK);

        sticksBox_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 12));
        sticksBox_->set_halign(Gtk::ALIGN_CENTER);
        page->pack_start(*sticksBox_, Gtk::PACK_SHRINK);

        page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        auto axesLabel = Gtk::manage(new Gtk::Label("All Axes"));
        axesLabel->get_style_context()->add_class("section-title");
        axesLabel->set_halign(Gtk::ALIGN_START);
        page->pack_start(*axesLabel, Gtk::PACK_SHRINK);

        auto axesScroll = Gtk::manage(new Gtk::ScrolledWindow());
        axesScroll->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        axesBarsBox_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 2));
        axesScroll->add(*axesBarsBox_);
        page->pack_start(*axesScroll, Gtk::PACK_EXPAND_WIDGET);

        page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        auto btnsLabel = Gtk::manage(new Gtk::Label("Buttons"));
        btnsLabel->get_style_context()->add_class("section-title");
        btnsLabel->set_halign(Gtk::ALIGN_START);
        page->pack_start(*btnsLabel, Gtk::PACK_SHRINK);

        buttonGrid_ = Gtk::manage(new ButtonGridWidget());
        page->pack_start(*buttonGrid_, Gtk::PACK_SHRINK);
    }

    void buildAxisMappingPage(Gtk::Box* page) {
        auto topRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
        auto infoLabel = Gtk::manage(new Gtk::Label("Assign each joystick axis to a robot action. Click \"Listen\" to detect an axis by moving it."));
        infoLabel->set_halign(Gtk::ALIGN_START);
        infoLabel->set_line_wrap(true);
        topRow->pack_start(*infoLabel, Gtk::PACK_EXPAND_WIDGET);

        auto btnListen = Gtk::manage(new Gtk::Button("Listen for Axis"));
        btnListen->get_style_context()->add_class("accent-btn");
        btnListen->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onListenAxis));
        topRow->pack_end(*btnListen, Gtk::PACK_SHRINK);

        auto btnAutoDetect = Gtk::manage(new Gtk::Button("Auto-Detect All"));
        btnAutoDetect->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onAutoDetectAxes));
        topRow->pack_end(*btnAutoDetect, Gtk::PACK_SHRINK);

        page->pack_start(*topRow, Gtk::PACK_SHRINK);
        page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        auto scroll = Gtk::manage(new Gtk::ScrolledWindow());
        scroll->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        axisMappingBox_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        scroll->add(*axisMappingBox_);
        page->pack_start(*scroll, Gtk::PACK_EXPAND_WIDGET);
    }

    void buildButtonMappingPage(Gtk::Box* page) {
        auto topRow = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
        auto infoLabel = Gtk::manage(new Gtk::Label("Assign each button to a robot action. Click \"Listen\" to detect a button press."));
        infoLabel->set_halign(Gtk::ALIGN_START);
        infoLabel->set_line_wrap(true);
        topRow->pack_start(*infoLabel, Gtk::PACK_EXPAND_WIDGET);

        auto btnListen = Gtk::manage(new Gtk::Button("Listen for Button"));
        btnListen->get_style_context()->add_class("accent-btn");
        btnListen->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onListenButton));
        topRow->pack_end(*btnListen, Gtk::PACK_SHRINK);

        auto btnAutoDetect = Gtk::manage(new Gtk::Button("Auto-Detect All"));
        btnAutoDetect->signal_clicked().connect(sigc::mem_fun(*this, &RebindWindow::onAutoDetectButtons));
        topRow->pack_end(*btnAutoDetect, Gtk::PACK_SHRINK);

        page->pack_start(*topRow, Gtk::PACK_SHRINK);
        page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        auto scroll = Gtk::manage(new Gtk::ScrolledWindow());
        scroll->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        buttonMappingBox_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        scroll->add(*buttonMappingBox_);
        page->pack_start(*scroll, Gtk::PACK_EXPAND_WIDGET);
    }

    void buildSettingsPage(Gtk::Box* page) {
        auto title = Gtk::manage(new Gtk::Label("Global Settings"));
        title->get_style_context()->add_class("section-title");
        title->set_halign(Gtk::ALIGN_START);
        page->pack_start(*title, Gtk::PACK_SHRINK);

        auto grid = Gtk::manage(new Gtk::Grid());
        grid->set_row_spacing(10);
        grid->set_column_spacing(20);
        grid->set_margin_top(8);

        int row = 0;

        grid->attach(*Gtk::manage(new Gtk::Label("Dead Zone:")), 0, row);
        deadZoneSpin_ = Gtk::manage(new Gtk::SpinButton());
        deadZoneSpin_->set_range(0, 32000);
        deadZoneSpin_->set_increments(500, 2000);
        deadZoneSpin_->set_value(config_.deadZone);
        grid->attach(*deadZoneSpin_, 1, row);
        row++;

        invertYCheck_ = Gtk::manage(new Gtk::CheckButton("Invert Y Axes Globally"));
        invertYCheck_->set_active(config_.invertY);
        grid->attach(*invertYCheck_, 0, row, 2, 1);
        row++;

        controllerCheck_ = Gtk::manage(new Gtk::CheckButton("Controller Mode (single gamepad)"));
        controllerCheck_->set_active(config_.isController);
        grid->attach(*controllerCheck_, 0, row, 2, 1);
        row++;

        twoJoysticksCheck_ = Gtk::manage(new Gtk::CheckButton("Two Joysticks"));
        twoJoysticksCheck_->set_active(config_.twoJoysticks);
        grid->attach(*twoJoysticksCheck_, 0, row, 2, 1);
        row++;

        altLayoutCheck_ = Gtk::manage(new Gtk::CheckButton("Alternate Layout"));
        altLayoutCheck_->set_active(config_.useAltLayout);
        grid->attach(*altLayoutCheck_, 0, row, 2, 1);
        row++;

        page->pack_start(*grid, Gtk::PACK_SHRINK);
        page->pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)), Gtk::PACK_SHRINK);

        auto presetsLabel = Gtk::manage(new Gtk::Label("Presets"));
        presetsLabel->get_style_context()->add_class("section-title");
        presetsLabel->set_halign(Gtk::ALIGN_START);
        presetsLabel->set_margin_top(10);
        page->pack_start(*presetsLabel, Gtk::PACK_SHRINK);

        auto presetsBox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
        presetsBox->set_margin_top(4);

        auto btn2Joy = Gtk::manage(new Gtk::Button("Two Joysticks"));
        btn2Joy->signal_clicked().connect([this]() {
            config_ = makeDefaultConfig_TwoJoysticks();
            syncUIFromConfig(); rebuildAxisMappingRows(); rebuildButtonMappingRows();
        });
        presetsBox->pack_start(*btn2Joy, Gtk::PACK_SHRINK);

        auto btnCtrl = Gtk::manage(new Gtk::Button("Single Controller"));
        btnCtrl->signal_clicked().connect([this]() {
            config_ = makeDefaultConfig_Controller();
            syncUIFromConfig(); rebuildAxisMappingRows(); rebuildButtonMappingRows();
        });
        presetsBox->pack_start(*btnCtrl, Gtk::PACK_SHRINK);

        auto btn1Joy = Gtk::manage(new Gtk::Button("Single Joystick"));
        btn1Joy->signal_clicked().connect([this]() {
            config_ = makeDefaultConfig_SingleJoystick();
            syncUIFromConfig(); rebuildAxisMappingRows(); rebuildButtonMappingRows();
        });
        presetsBox->pack_start(*btn1Joy, Gtk::PACK_SHRINK);

        auto btnClear = Gtk::manage(new Gtk::Button("Clear All"));
        btnClear->get_style_context()->add_class("danger-btn");
        btnClear->signal_clicked().connect([this]() {
            config_.axisMappings.clear(); config_.buttonMappings.clear();
            rebuildAxisMappingRows(); rebuildButtonMappingRows();
        });
        presetsBox->pack_end(*btnClear, Gtk::PACK_SHRINK);

        page->pack_start(*presetsBox, Gtk::PACK_SHRINK);
    }

    // ─── Joystick Management ──────────────────────────────────────────────

    void openJoysticks() {
        closeJoysticks();
        int count = SDL_NumJoysticks();
        for (int i = 0; i < count; i++) {
            JoystickState js;
            js.index = i;
            if (SDL_IsGameController(i)) {
                js.isGameController = true;
                js.controller = SDL_GameControllerOpen(i);
                if (js.controller) {
                    js.handle = SDL_GameControllerGetJoystick(js.controller);
                    js.name = SDL_GameControllerName(js.controller);
                }
            } else {
                js.handle = SDL_JoystickOpen(i);
                if (js.handle) js.name = SDL_JoystickName(js.handle);
            }
            if (js.handle) {
                js.numAxes = SDL_JoystickNumAxes(js.handle);
                js.numButtons = SDL_JoystickNumButtons(js.handle);
                js.numHats = SDL_JoystickNumHats(js.handle);
                js.axes.resize(js.numAxes, 0.0f);
                js.buttons.resize(js.numButtons, false);
                js.hats.resize(js.numHats, 0);
                joysticks_.push_back(js);
            }
        }
    }

    void closeJoysticks() {
        for (auto& js : joysticks_) {
            if (js.controller) SDL_GameControllerClose(js.controller);
            else if (js.handle) SDL_JoystickClose(js.handle);
        }
        joysticks_.clear();
    }

    // ─── Poll Timer ───────────────────────────────────────────────────────

    bool onPollTimer() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_JOYAXISMOTION: {
                    int ji = event.jaxis.which;
                    if (ji >= 0 && ji < (int)joysticks_.size()) {
                        joysticks_[ji].axes[event.jaxis.axis] = event.jaxis.value / 32768.0f;
                        if (listenMode_ == ListenMode::AXIS && std::abs(event.jaxis.value / 32768.0f) > 0.5f)
                            onAxisDetected(ji, event.jaxis.axis);
                    }
                    break;
                }
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP: {
                    int ji = event.jbutton.which;
                    if (ji >= 0 && ji < (int)joysticks_.size()) {
                        joysticks_[ji].buttons[event.jbutton.button] = (event.jbutton.state == SDL_PRESSED);
                        if (listenMode_ == ListenMode::BUTTON && event.jbutton.state == SDL_PRESSED)
                            onButtonDetected(ji, event.jbutton.button);
                    }
                    break;
                }
                case SDL_JOYHATMOTION: {
                    int ji = event.jhat.which;
                    if (ji >= 0 && ji < (int)joysticks_.size())
                        joysticks_[ji].hats[event.jhat.hat] = event.jhat.value;
                    break;
                }
                case SDL_JOYDEVICEADDED:
                case SDL_JOYDEVICEREMOVED:
                    openJoysticks();
                    monitorBuilt_ = false;
                    break;
            }
        }
        updateStatusBar();
        updateMonitor();
        return true;
    }

    void updateStatusBar() {
        if (joysticks_.empty()) {
            statusLabel_->set_text("No controller detected");
            statusLabel_->get_style_context()->remove_class("status-connected");
            statusLabel_->get_style_context()->add_class("status-disconnected");
            axesCountLabel_->set_text("");
            buttonsCountLabel_->set_text("");
        } else {
            auto& js = joysticks_[0];
            std::string name = js.name;
            if (name.length() > 40) name = name.substr(0, 37) + "...";
            statusLabel_->set_text(name + (js.isGameController ? " [Controller]" : ""));
            statusLabel_->get_style_context()->remove_class("status-disconnected");
            statusLabel_->get_style_context()->add_class("status-connected");
            axesCountLabel_->set_text("Axes: " + std::to_string(js.numAxes));
            buttonsCountLabel_->set_text("Buttons: " + std::to_string(js.numButtons));
        }
    }

    void updateMonitor() {
        if (joysticks_.empty()) return;
        auto& js = joysticks_[0];

        if (!monitorBuilt_) {
            for (auto* child : sticksBox_->get_children()) sticksBox_->remove(*child);
            for (auto* child : axesBarsBox_->get_children()) axesBarsBox_->remove(*child);
            stickWidgets_.clear();
            axisBarWidgets_.clear();

            int stickCount = js.numAxes / 2;
            for (int s = 0; s < std::min(stickCount, 4); s++) {
                auto* sw = Gtk::manage(new StickWidget());
                sw->set_label("Stick " + std::to_string(s) + " (A" + std::to_string(s*2) + ",A" + std::to_string(s*2+1) + ")");
                sticksBox_->pack_start(*sw, Gtk::PACK_SHRINK);
                stickWidgets_.push_back(sw);
            }

            for (int a = 0; a < js.numAxes; a++) {
                auto* bar = Gtk::manage(new AxisBarWidget());
                bar->set_label("A" + std::to_string(a));
                axesBarsBox_->pack_start(*bar, Gtk::PACK_SHRINK);
                axisBarWidgets_.push_back(bar);
            }

            sticksBox_->show_all();
            axesBarsBox_->show_all();
            monitorBuilt_ = true;
        }

        for (size_t s = 0; s < stickWidgets_.size(); s++) {
            float x = js.axes[s * 2];
            float y = (s * 2 + 1 < js.axes.size()) ? js.axes[s * 2 + 1] : 0;
            stickWidgets_[s]->set_position(x, y);
        }

        for (size_t a = 0; a < axisBarWidgets_.size() && a < js.axes.size(); a++) {
            axisBarWidgets_[a]->set_value(js.axes[a]);
            axisBarWidgets_[a]->set_active(std::abs(js.axes[a]) > 0.1f);
        }

        buttonGrid_->set_states(js.buttons);
    }

    // ─── Listen Mode ──────────────────────────────────────────────────────

    void onListenAxis() {
        listenMode_ = ListenMode::AXIS;
        showListenDialog("Move an Axis", "Push a joystick or trigger on your controller...");
    }

    void onListenButton() {
        listenMode_ = ListenMode::BUTTON;
        showListenDialog("Press a Button", "Press any button on your controller...");
    }

    void showListenDialog(const std::string& title, const std::string& message) {
        listenDialog_ = new Gtk::Dialog(title, *this, true);
        listenDialog_->set_default_size(380, 150);
        auto* content = listenDialog_->get_content_area();

        auto* label = Gtk::manage(new Gtk::Label(message));
        label->set_margin_top(20);
        label->set_margin_bottom(10);
        content->pack_start(*label, Gtk::PACK_SHRINK);

        auto* btnCancel = Gtk::manage(new Gtk::Button("Cancel"));
        btnCancel->set_halign(Gtk::ALIGN_CENTER);
        btnCancel->set_margin_bottom(10);
        btnCancel->signal_clicked().connect([this]() { dismissListenDialog(); });
        content->pack_start(*btnCancel, Gtk::PACK_SHRINK);

        listenDialog_->show_all();
    }

    void dismissListenDialog() {
        listenMode_ = ListenMode::NONE;
        if (listenDialog_) { listenDialog_->hide(); delete listenDialog_; listenDialog_ = nullptr; }
    }

    void onAxisDetected(int joystick, int axis) {
        dismissListenDialog();
        for (auto& m : config_.axisMappings)
            if (m.inputJoystick == joystick && m.inputAxis == axis) return;

        config_.axisMappings.push_back({joystick, axis, 0, 0, "(unmapped)", false});
        rebuildAxisMappingRows();
        notebook_->set_current_page(1);
    }

    void onButtonDetected(int joystick, int button) {
        dismissListenDialog();
        for (auto& m : config_.buttonMappings)
            if (m.inputJoystick == joystick && m.inputButton == button) return;

        config_.buttonMappings.push_back({joystick, button, "(unmapped)"});
        rebuildButtonMappingRows();
        notebook_->set_current_page(2);
    }

    // ─── Auto-Detect ──────────────────────────────────────────────────────

    void onAutoDetectAxes() {
        config_.axisMappings.clear();
        for (size_t j = 0; j < joysticks_.size(); j++)
            for (int a = 0; a < joysticks_[j].numAxes; a++)
                config_.axisMappings.push_back({(int)j, a, 0, 0, "(unmapped)", false});
        rebuildAxisMappingRows();
    }

    void onAutoDetectButtons() {
        config_.buttonMappings.clear();
        for (size_t j = 0; j < joysticks_.size(); j++)
            for (int b = 0; b < joysticks_[j].numButtons; b++)
                config_.buttonMappings.push_back({(int)j, b, "(unmapped)"});
        rebuildButtonMappingRows();
    }

    // ─── Mapping Row Builders ─────────────────────────────────────────────

    void rebuildAxisMappingRows() {
        for (auto* child : axisMappingBox_->get_children()) axisMappingBox_->remove(*child);

        if (config_.axisMappings.empty()) {
            auto* lbl = Gtk::manage(new Gtk::Label("No axis mappings. Use \"Listen\" or \"Auto-Detect\" to add inputs."));
            lbl->set_margin_top(30);
            axisMappingBox_->pack_start(*lbl, Gtk::PACK_SHRINK);
        } else {
            for (size_t i = 0; i < config_.axisMappings.size(); i++)
                axisMappingBox_->pack_start(*createAxisMappingRow(i), Gtk::PACK_SHRINK);
        }
        axisMappingBox_->show_all();
    }

    Gtk::Box* createAxisMappingRow(size_t index) {
        auto& m = config_.axisMappings[index];
        auto* row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
        row->set_margin_top(2); row->set_margin_bottom(2);

        auto* inputLabel = Gtk::manage(new Gtk::Label("J" + std::to_string(m.inputJoystick) + " Axis " + std::to_string(m.inputAxis)));
        inputLabel->set_size_request(100, -1);
        inputLabel->set_halign(Gtk::ALIGN_START);
        row->pack_start(*inputLabel, Gtk::PACK_SHRINK);

        row->pack_start(*Gtk::manage(new Gtk::Label("\xe2\x86\x92")), Gtk::PACK_SHRINK);

        auto* combo = Gtk::manage(new Gtk::ComboBoxText());
        for (auto& action : AXIS_ACTIONS) combo->append(action);
        combo->set_active_text(m.action);
        combo->signal_changed().connect([this, index, combo]() {
            if (index < config_.axisMappings.size()) {
                config_.axisMappings[index].action = combo->get_active_text();
                auto it = ACTION_TO_OUTPUT.find(config_.axisMappings[index].action);
                if (it != ACTION_TO_OUTPUT.end()) {
                    config_.axisMappings[index].outputJoystick = it->second.first;
                    config_.axisMappings[index].outputAxis = it->second.second;
                }
            }
        });
        row->pack_start(*combo, Gtk::PACK_EXPAND_WIDGET);

        auto* invertCheck = Gtk::manage(new Gtk::CheckButton("Invert"));
        invertCheck->set_active(m.invert);
        invertCheck->signal_toggled().connect([this, index, invertCheck]() {
            if (index < config_.axisMappings.size())
                config_.axisMappings[index].invert = invertCheck->get_active();
        });
        row->pack_start(*invertCheck, Gtk::PACK_SHRINK);

        auto* btnDel = Gtk::manage(new Gtk::Button("\xe2\x9c\x95"));
        btnDel->get_style_context()->add_class("danger-btn");
        btnDel->signal_clicked().connect([this, index]() {
            if (index < config_.axisMappings.size()) {
                config_.axisMappings.erase(config_.axisMappings.begin() + index);
                rebuildAxisMappingRows();
            }
        });
        row->pack_end(*btnDel, Gtk::PACK_SHRINK);

        return row;
    }

    void rebuildButtonMappingRows() {
        for (auto* child : buttonMappingBox_->get_children()) buttonMappingBox_->remove(*child);

        if (config_.buttonMappings.empty()) {
            auto* lbl = Gtk::manage(new Gtk::Label("No button mappings. Use \"Listen\" or \"Auto-Detect\" to add inputs."));
            lbl->set_margin_top(30);
            buttonMappingBox_->pack_start(*lbl, Gtk::PACK_SHRINK);
        } else {
            for (size_t i = 0; i < config_.buttonMappings.size(); i++)
                buttonMappingBox_->pack_start(*createButtonMappingRow(i), Gtk::PACK_SHRINK);
        }
        buttonMappingBox_->show_all();
    }

    Gtk::Box* createButtonMappingRow(size_t index) {
        auto& m = config_.buttonMappings[index];
        auto* row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
        row->set_margin_top(2); row->set_margin_bottom(2);

        auto* inputLabel = Gtk::manage(new Gtk::Label("J" + std::to_string(m.inputJoystick) + " Btn " + std::to_string(m.inputButton)));
        inputLabel->set_size_request(120, -1);
        inputLabel->set_halign(Gtk::ALIGN_START);
        row->pack_start(*inputLabel, Gtk::PACK_SHRINK);

        row->pack_start(*Gtk::manage(new Gtk::Label("\xe2\x86\x92")), Gtk::PACK_SHRINK);

        auto* combo = Gtk::manage(new Gtk::ComboBoxText());
        for (auto& action : BUTTON_ACTIONS) combo->append(action);
        combo->set_active_text(m.action);
        combo->signal_changed().connect([this, index, combo]() {
            if (index < config_.buttonMappings.size())
                config_.buttonMappings[index].action = combo->get_active_text();
        });
        row->pack_start(*combo, Gtk::PACK_EXPAND_WIDGET);

        auto* btnDel = Gtk::manage(new Gtk::Button("\xe2\x9c\x95"));
        btnDel->get_style_context()->add_class("danger-btn");
        btnDel->signal_clicked().connect([this, index]() {
            if (index < config_.buttonMappings.size()) {
                config_.buttonMappings.erase(config_.buttonMappings.begin() + index);
                rebuildButtonMappingRows();
            }
        });
        row->pack_end(*btnDel, Gtk::PACK_SHRINK);

        return row;
    }

    // ─── Config Sync ──────────────────────────────────────────────────────

    void syncConfigFromUI() {
        config_.deadZone = deadZoneSpin_->get_value_as_int();
        config_.invertY = invertYCheck_->get_active();
        config_.isController = controllerCheck_->get_active();
        config_.twoJoysticks = twoJoysticksCheck_->get_active();
        config_.useAltLayout = altLayoutCheck_->get_active();
    }

    void syncUIFromConfig() {
        deadZoneSpin_->set_value(config_.deadZone);
        invertYCheck_->set_active(config_.invertY);
        controllerCheck_->set_active(config_.isController);
        twoJoysticksCheck_->set_active(config_.twoJoysticks);
        altLayoutCheck_->set_active(config_.useAltLayout);
    }

    // ─── Save / Load ──────────────────────────────────────────────────────

    void onSave() {
        syncConfigFromUI();
        Gtk::FileChooserDialog dialog("Save Controller Config", Gtk::FILE_CHOOSER_ACTION_SAVE);
        dialog.set_transient_for(*this);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Save", Gtk::RESPONSE_OK);
        dialog.set_current_name("controller_config.json");
        dialog.set_do_overwrite_confirmation(true);

        auto filter = Gtk::FileFilter::create();
        filter->set_name("JSON files");
        filter->add_pattern("*.json");
        dialog.add_filter(filter);

        if (dialog.run() == Gtk::RESPONSE_OK) {
            if (config_.saveToFile(dialog.get_filename())) {
                Gtk::MessageDialog msg(*this, "Config saved successfully!", false, Gtk::MESSAGE_INFO);
                msg.run();
            }
        }
    }

    void onLoad() {
        Gtk::FileChooserDialog dialog("Load Controller Config", Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.set_transient_for(*this);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Open", Gtk::RESPONSE_OK);

        auto filter = Gtk::FileFilter::create();
        filter->set_name("JSON files");
        filter->add_pattern("*.json");
        dialog.add_filter(filter);

        if (dialog.run() == Gtk::RESPONSE_OK) {
            config_ = InputConfig::loadFromFile(dialog.get_filename());
            syncUIFromConfig();
            rebuildAxisMappingRows();
            rebuildButtonMappingRows();
        }
    }
};

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    std::string loadFile;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--load") && i + 1 < argc)
            loadFile = argv[++i];
        else if (!strcmp(argv[i], "--help")) {
            std::cout << "Usage: input_rebind_tool [--load <config.json>]\n";
            return 0;
        }
    }

    auto app = Gtk::Application::create(argc, argv, "edu.uark.razorbotz.rebind");
    RebindWindow window(loadFile);
    return app->run(window);
}