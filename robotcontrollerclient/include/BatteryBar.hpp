#pragma once

#include <gtkmm.h>
#include <cairomm/context.h>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>

/**
 * BatteryBar - Persistent battery/voltage indicator for the top controls bar.
 * 
 * Tracks the lowest bus voltage across all motors and displays it as a
 * thin horizontal bar with numeric readout. Turns yellow at the warning
 * threshold, red below critical.
 * 
 * Feed it voltage readings from any motor handler — it tracks the minimum
 * automatically. Call reset_cycle() once per update cycle (e.g., per
 * heartbeat) so stale readings don't persist from disconnected motors.
 * 
 * Usage:
 *   BatteryBar* batteryBar = Gtk::manage(new BatteryBar());
 *   batteryBar->set_size_request(-1, 28);
 *   batteryBar->set_hexpand(true);
 *   topControlsBox->pack_end(*batteryBar, Gtk::PACK_SHRINK);
 * 
 *   // In every motor handler that reads "Bus Voltage":
 *   batteryBar->report_voltage("Talon 1", voltage);
 */
class BatteryBar : public Gtk::DrawingArea {
public:
    BatteryBar()
        : min_voltage_(0.0), max_voltage_(18.0),
          warning_voltage_(12.0), critical_voltage_(11.0),
          is_light_mode_(true) {}

    /**
     * Report a voltage reading from a motor. The bar displays the
     * lowest voltage across all reported motors.
     */
    void report_voltage(const std::string& motor_name, float voltage) {
        motor_voltages_[motor_name] = voltage;
        queue_draw();
    }

    /** Call periodically to clear stale readings from disconnected motors. */
    void reset_cycle() {
        motor_voltages_.clear();
        queue_draw();
    }

    void set_voltage_range(float min_v, float max_v) {
        min_voltage_ = min_v;
        max_voltage_ = max_v;
    }

    void set_warning_voltage(float warn) { warning_voltage_ = warn; }
    void set_critical_voltage(float crit) { critical_voltage_ = crit; }
    void set_light_mode(bool light) { is_light_mode_ = light; queue_draw(); }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        const int w = get_allocated_width();
        const int h = get_allocated_height();

        // Find minimum voltage across all motors
        float current_v = max_voltage_;
        bool has_data = !motor_voltages_.empty();
        for (const auto& kv : motor_voltages_) {
            current_v = std::min(current_v, kv.second);
        }

        // Background — transparent so the top controls bar shows through
        cr->set_source_rgba(0, 0, 0, 0);
        cr->paint();

        const double pad_x = 8.0;
        const double label_w = 28.0;     // "BAT" label
        const double readout_w = 52.0;   // "15.6V" readout
        const double warn_w = 70.0;      // "12.0V warn" label
        const double bar_x = pad_x + label_w + 6;
        const double bar_w = w - bar_x - readout_w - warn_w - pad_x - 16;
        const double bar_y = (h - 14) / 2.0;
        const double bar_h = 14.0;

        if (bar_w < 20) return true; // Not enough space

        // "BAT" label
        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(10.0);
        if (is_light_mode_) cr->set_source_rgba(0.3, 0.3, 0.3, 0.8);
        else cr->set_source_rgba(0.55, 0.63, 0.67, 0.8);
        cr->move_to(pad_x, h / 2.0 + 4);
        cr->show_text("BAT");

        // Bar track
        rounded_rect(cr, bar_x, bar_y, bar_w, bar_h, 3.0);
        if (is_light_mode_) {
            cr->set_source_rgba(0.85, 0.85, 0.85, 0.8);
        } else {
            cr->set_source_rgba(0.10, 0.16, 0.20, 0.9);
        }
        cr->fill_preserve();
        if (is_light_mode_) cr->set_source_rgba(0.7, 0.7, 0.7, 0.6);
        else cr->set_source_rgba(0.20, 0.28, 0.33, 0.6);
        cr->set_line_width(1.0);
        cr->stroke();

        // Fill bar
        if (has_data) {
            double ratio = std::clamp((double)(current_v - min_voltage_) / (max_voltage_ - min_voltage_), 0.0, 1.0);
            double fill_w = ratio * (bar_w - 4);

            double r, g, b;
            get_voltage_color(current_v, r, g, b);

            if (fill_w > 1) {
                cr->set_source_rgba(r, g, b, 0.8);
                rounded_rect(cr, bar_x + 2, bar_y + 2, fill_w, bar_h - 4, 2.0);
                cr->fill();
            }

            // Warning threshold marker line on the bar
            double warn_ratio = std::clamp((double)(warning_voltage_ - min_voltage_) / (max_voltage_ - min_voltage_), 0.0, 1.0);
            double warn_x = bar_x + 2 + warn_ratio * (bar_w - 4);
            cr->set_source_rgba(0.98, 0.75, 0.17, 0.5);
            cr->set_line_width(1.0);
            cr->move_to(warn_x, bar_y + 1);
            cr->line_to(warn_x, bar_y + bar_h - 1);
            cr->stroke();

            // Voltage readout
            cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
            cr->set_font_size(13.0);
            cr->set_source_rgb(r, g, b);

            char vbuf[16];
            std::snprintf(vbuf, sizeof(vbuf), "%.1fV", current_v);
            cr->move_to(bar_x + bar_w + 8, h / 2.0 + 5);
            cr->show_text(vbuf);

        } else {
            // No data
            cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
            cr->set_font_size(11.0);
            if (is_light_mode_) cr->set_source_rgba(0.5, 0.5, 0.5, 0.6);
            else cr->set_source_rgba(0.4, 0.48, 0.53, 0.6);
            cr->move_to(bar_x + bar_w + 8, h / 2.0 + 4);
            cr->show_text("--.-V");
        }

        // Warning threshold label
        cr->set_font_size(10.0);
        if (is_light_mode_) cr->set_source_rgba(0.5, 0.5, 0.5, 0.6);
        else cr->set_source_rgba(0.55, 0.55, 0.50, 0.5);

        // Separator line
        double sep_x = bar_x + bar_w + readout_w + 4;
        cr->set_line_width(1.0);
        cr->move_to(sep_x, bar_y);
        cr->line_to(sep_x, bar_y + bar_h);
        cr->stroke();

        char wbuf[16];
        std::snprintf(wbuf, sizeof(wbuf), "%.1fV warn", warning_voltage_);
        cr->set_source_rgba(0.98, 0.75, 0.17, 0.6);
        cr->move_to(sep_x + 8, h / 2.0 + 4);
        cr->show_text(wbuf);

        return true;
    }

private:
    std::map<std::string, float> motor_voltages_;
    float min_voltage_;
    float max_voltage_;
    float warning_voltage_;
    float critical_voltage_;
    bool is_light_mode_;

    void get_voltage_color(float v, double& r, double& g, double& b) {
        if (v < critical_voltage_) {
            r = 0.94; g = 0.27; b = 0.27; // Red
        } else if (v < warning_voltage_) {
            r = 0.98; g = 0.75; b = 0.17; // Yellow
        } else {
            r = 0.13; g = 0.77; b = 0.37; // Green
        }
    }

    void rounded_rect(const Cairo::RefPtr<Cairo::Context>& cr,
                      double x, double y, double w, double h, double r) {
        cr->begin_new_sub_path();
        cr->arc(x + w - r, y + r, r, -M_PI / 2.0, 0);
        cr->arc(x + w - r, y + h - r, r, 0, M_PI / 2.0);
        cr->arc(x + r, y + h - r, r, M_PI / 2.0, M_PI);
        cr->arc(x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
        cr->close_path();
    }
};
