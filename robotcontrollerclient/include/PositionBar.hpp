#pragma once

#include <gtkmm.h>
#include <cairomm/context.h>
#include <string>
#include <cmath>
#include <algorithm>

/**
 * PositionBar - Enhanced vertical position indicator with scale and readout.
 * 
 * Drop-in replacement for the plain DrawingArea bars used in arm/bucket
 * position indicators. Adds:
 *   - Numeric position readout above the bar
 *   - Colored fill (green → yellow → red based on position)
 *   - White needle at current position
 *   - Warning zone coloring at extremes
 * 
 * Works in both paired mode (L/R arms, L/R buckets) and single-actuator
 * mode (dump bot's single bucket motor).
 * 
 * Usage:
 *   PositionBar* bar = Gtk::manage(new PositionBar());
 *   bar->set_size_request(32, 200);
 *   bar->set_range(0, 920);
 *   bar->set_position(pos);
 */
class PositionBar : public Gtk::DrawingArea {
public:
    PositionBar()
        : position_(0), min_val_(0), max_val_(920),
          warning_low_(100), warning_high_(820),
          is_light_mode_(true) {}

    void set_position(int pos) {
        position_ = pos;
        queue_draw();
    }

    int get_position() const { return position_; }

    /** Set the raw sensor value range. Default: 0-920 for arm, 0-700 for bucket. */
    void set_range(int min_val, int max_val) {
        min_val_ = min_val;
        max_val_ = max_val;
        queue_draw();
    }

    /** Values outside these thresholds turn the fill red. */
    void set_warning_limits(int low, int high) {
        warning_low_ = low;
        warning_high_ = high;
    }

    void set_light_mode(bool light) {
        is_light_mode_ = light;
        queue_draw();
    }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        const int w = get_allocated_width();
        const int h = get_allocated_height();

        const double readout_h = 18.0;
        const double bar_top = readout_h + 4;
        const double bar_h = h - bar_top - 4;
        const double bar_x = 2.0;
        const double bar_w = w - 4.0;

        // Transparent background
        cr->set_source_rgba(0, 0, 0, 0);
        cr->paint();

        // Numeric readout
        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(11.0);

        double ratio = normalized();
        bool in_warning = (position_ < warning_low_ || position_ > warning_high_);

        if (in_warning) {
            cr->set_source_rgb(0.94, 0.27, 0.27);
        } else {
            cr->set_source_rgb(0.29, 0.85, 0.50);
        }

        std::string val_text = std::to_string(position_);
        Cairo::TextExtents te;
        cr->get_text_extents(val_text, te);
        cr->move_to((w - te.width) / 2.0, readout_h - 2);
        cr->show_text(val_text);

        // Bar track
        rounded_rect(cr, bar_x, bar_top, bar_w, bar_h, 3.0);
        if (is_light_mode_) {
            cr->set_source_rgba(0.82, 0.82, 0.82, 0.8);
        } else {
            cr->set_source_rgba(0.10, 0.16, 0.20, 0.9);
        }
        cr->fill_preserve();
        if (is_light_mode_) {
            cr->set_source_rgba(0.65, 0.65, 0.65, 0.8);
        } else {
            cr->set_source_rgba(0.20, 0.28, 0.33, 0.9);
        }
        cr->set_line_width(1.0);
        cr->stroke();

        // Filled portion (bottom-up)
        double fill_h = ratio * (bar_h - 4);
        if (fill_h > 1) {
            double fill_y = bar_top + bar_h - 2 - fill_h;

            double r, g, b;
            if (in_warning) {
                r = 0.94; g = 0.27; b = 0.27;
            } else if (ratio < 0.15 || ratio > 0.85) {
                r = 0.98; g = 0.75; b = 0.17;
            } else {
                r = 0.13; g = 0.77; b = 0.37;
            }

            cr->set_source_rgba(r, g, b, 0.75);
            rounded_rect(cr, bar_x + 2, fill_y, bar_w - 4, fill_h, 2.0);
            cr->fill();
        }

        // Needle
        double needle_y = bar_top + (1.0 - ratio) * bar_h;
        needle_y = std::max(bar_top + 2.0, std::min(bar_top + bar_h - 2.0, needle_y));

        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->set_line_width(2.0);
        cr->move_to(bar_x - 3, needle_y);
        cr->line_to(bar_x + bar_w + 3, needle_y);
        cr->stroke();

        // Arrow tips
        cr->move_to(bar_x - 3, needle_y);
        cr->line_to(bar_x + 1, needle_y - 3);
        cr->line_to(bar_x + 1, needle_y + 3);
        cr->close_path();
        cr->fill();

        cr->move_to(bar_x + bar_w + 3, needle_y);
        cr->line_to(bar_x + bar_w - 1, needle_y - 3);
        cr->line_to(bar_x + bar_w - 1, needle_y + 3);
        cr->close_path();
        cr->fill();

        return true;
    }

private:
    int position_;
    int min_val_, max_val_;
    int warning_low_, warning_high_;
    bool is_light_mode_;

    double normalized() const {
        if (max_val_ <= min_val_) return 0.0;
        return std::clamp((double)(position_ - min_val_) / (max_val_ - min_val_), 0.0, 1.0);
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

/**
 * SyncStatusLabel - Badge showing L/R sync state.
 * Shows "SYNCED" in green or "DESYNCED" in red.
 */
class SyncStatusLabel : public Gtk::DrawingArea {
public:
    SyncStatusLabel() : synced_(true), is_light_mode_(true) {
        set_size_request(80, 18);
    }

    void update(int left_pos, int right_pos, int threshold = 50) {
        synced_ = std::abs(left_pos - right_pos) <= threshold;
        queue_draw();
    }

    void set_light_mode(bool light) { is_light_mode_ = light; queue_draw(); }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        const int w = get_allocated_width();
        const int h = get_allocated_height();

        double bg_r, bg_g, bg_b, fg_r, fg_g, fg_b;
        std::string text;

        if (synced_) {
            text = "SYNCED";
            bg_r = 0.08; bg_g = 0.33; bg_b = 0.18;
            fg_r = 0.29; fg_g = 0.85; fg_b = 0.50;
        } else {
            text = "DESYNCED";
            bg_r = 0.50; bg_g = 0.11; bg_b = 0.11;
            fg_r = 0.97; fg_g = 0.44; fg_b = 0.44;
        }

        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(9.0);
        Cairo::TextExtents te;
        cr->get_text_extents(text, te);
        double badge_w = te.width + 14;
        double badge_h = 16;
        double badge_x = (w - badge_w) / 2.0;
        double badge_y = (h - badge_h) / 2.0;

        cr->set_source_rgb(bg_r, bg_g, bg_b);
        cr->rectangle(badge_x, badge_y, badge_w, badge_h);
        cr->fill();

        cr->set_source_rgb(fg_r, fg_g, fg_b);
        cr->move_to(badge_x + 7, badge_y + 12);
        cr->show_text(text);

        return true;
    }

private:
    bool synced_;
    bool is_light_mode_;
};
