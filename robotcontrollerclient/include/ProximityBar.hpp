#pragma once

#include <gtkmm.h>
#include <cairomm/context.h>
#include <string>
#include <cmath>
#include <chrono>

/**
 * ProximityBar - A vertical proximity indicator for lidar distance readings.
 * 
 * Displays three zones:
 *   - APPROACH (blue)  : target is far away, keep driving
 *   - OPTIMAL (green)  : target is in the dump range sweet spot
 *   - WARNING (red)    : too close, risk of collision
 * 
 * The active zone is highlighted while inactive zones are dimmed.
 * The warning zone blinks when the rover enters it.
 * A dashed border marks the optimal zone so it's always visible as a target.
 * A white needle and numeric readout show the precise distance.
 * 
 * Visibility is controlled by arm position — when the bucket is lowered
 * for excavation, the lidar is blocked, so the bar auto-hides with a
 * smooth fade. Call set_arm_position() with each arm actuator update;
 * the bar appears only when the arm is above the configured threshold.
 * 
 * Designed to overlay on top of the video feed with a semi-transparent
 * background, matching how the Speedometer widgets are placed via the
 * GtkOverlay in setupGUI().
 * 
 * Usage:
 *   ProximityBar* proximityBar = Gtk::manage(new ProximityBar());
 *   proximityBar->set_size_request(100, 380);
 *   pSpeedLeft->add(*proximityBar);  // or any overlay placeholder
 * 
 *   // In your sensor update handlers:
 *   proximityBar->set_distance(lidar_distance_meters);
 *   proximityBar->set_arm_position(arm_sensor_position);
 */
class ProximityBar : public Gtk::DrawingArea {
public:
    enum class Zone {
        APPROACH,  // Far away, keep driving
        OPTIMAL,   // In the sweet spot for dumping
        WARNING    // Too close, back off
    };

    ProximityBar() 
        : distance_m_(1.0),
          max_distance_m_(4.0),
          warning_threshold_m_(1.0),
          optimal_threshold_m_(2.0),
          arm_position_(0),
          arm_show_threshold_(400),
          bar_visible_(false),
          fade_alpha_(0.0),
          blink_on_(true),
          is_light_mode_(true)
    {
        // Blink timer for warning state (toggles every 300ms)
        blink_connection_ = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &ProximityBar::on_blink_tick), 300);

        // Smooth fade timer (~30fps for show/hide transitions)
        fade_connection_ = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &ProximityBar::on_fade_tick), 33);
    }

    ~ProximityBar() override {
        blink_connection_.disconnect();
        fade_connection_.disconnect();
    }

    // --- Primary interface ---

    /** Set the current lidar distance reading in meters. */
    void set_distance(double distance_m) {
        distance_m_ = std::max(0.0, std::min(distance_m, max_distance_m_));
        queue_draw();
    }

    double get_distance() const { return distance_m_; }

    Zone get_current_zone() const {
        if (distance_m_ < warning_threshold_m_) return Zone::WARNING;
        if (distance_m_ < optimal_threshold_m_) return Zone::OPTIMAL;
        return Zone::APPROACH;
    }

    /** 
     * Set the arm actuator sensor position (raw uint16 value from Talon).
     * The proximity bar fades in when the arm is above the threshold
     * (bucket raised, lidar has clear line of sight) and fades out
     * when the arm drops below it (bucket lowered, lidar blocked).
     */
    void set_arm_position(int position) {
        arm_position_ = position;
        bar_visible_ = (arm_position_ >= arm_show_threshold_);
        // Fade animation is driven by on_fade_tick()
    }

    /** 
     * Set the arm position threshold above which the bar becomes visible.
     * Same units as Talon "Sensor Position" (raw uint16).
     * Default: 400. Tune this to the point where your bucket clears 
     * the lidar's field of view.
     */
    void set_arm_show_threshold(int threshold) {
        arm_show_threshold_ = threshold;
    }

    /** Force the bar visible regardless of arm position (for testing). */
    void set_force_visible(bool visible) {
        bar_visible_ = visible;
        fade_alpha_ = visible ? 1.0 : 0.0;
        queue_draw();
    }

    // --- Zone configuration ---

    void set_warning_threshold(double meters) { warning_threshold_m_ = meters; queue_draw(); }
    void set_optimal_threshold(double meters) { optimal_threshold_m_ = meters; queue_draw(); }
    void set_max_distance(double meters) { max_distance_m_ = meters; queue_draw(); }
    void set_light_mode(bool light) { is_light_mode_ = light; queue_draw(); }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        // Fully faded out — draw nothing (transparent, video shows through)
        if (fade_alpha_ < 0.01) return true;

        const int w = get_allocated_width();
        const int h = get_allocated_height();

        // Push a group so the entire widget fades uniformly during transitions
        cr->push_group();

        // --- Layout constants ---
        const double pad_top = 26.0;
        const double pad_bottom = 62.0;
        const double bar_w = std::min((double)w * 0.38, 38.0);
        const double bar_x = (w - bar_w) / 2.0 - 18.0;
        const double bar_top = pad_top;
        const double bar_h = h - pad_top - pad_bottom;
        const double bar_bottom = bar_top + bar_h;
        const double tick_x = bar_x + bar_w + 8.0;

        // --- Semi-transparent background panel ---
        // Matches the transparent overlay approach used throughout the GUI.
        // The edge panels use "rgba(..., 0.0)" CSS backgrounds so video
        // shows through; here we use a slight tint so the bar is readable
        // over any video content without fully occluding the feed.
        rounded_rect(cr, 0, 0, w, h, 8.0);
        if (is_light_mode_) {
            cr->set_source_rgba(0.94, 0.98, 0.95, 0.55);
        } else {
            cr->set_source_rgba(0.043, 0.102, 0.129, 0.55);
        }
        cr->fill();

        // --- "PROXIMITY" label ---
        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(10.0);
        set_text_color(cr, 0.55, 0.63, 0.67, 0.3, 0.3, 0.3);
        Cairo::TextExtents te;
        cr->get_text_extents("PROXIMITY", te);
        cr->move_to((w - te.width) / 2.0, 17.0);
        cr->show_text("PROXIMITY");

        // --- Bar track ---
        rounded_rect(cr, bar_x, bar_top, bar_w, bar_h, 5.0);
        if (is_light_mode_) {
            cr->set_source_rgba(0.78, 0.78, 0.78, 0.8);
        } else {
            cr->set_source_rgba(0.086, 0.125, 0.157, 0.9);
        }
        cr->fill_preserve();
        if (is_light_mode_) {
            cr->set_source_rgba(0.6, 0.6, 0.6, 0.8);
        } else {
            cr->set_source_rgba(0.165, 0.227, 0.267, 0.9);
        }
        cr->set_line_width(1.0);
        cr->stroke();

        // --- Zone geometry ---
        // Top of bar = 0m (closest), bottom = max_distance (farthest)
        auto dist_to_y = [&](double d) -> double {
            return bar_top + (1.0 - d / max_distance_m_) * bar_h;
        };

        double warn_y = dist_to_y(warning_threshold_m_);
        double opt_y  = dist_to_y(optimal_threshold_m_);
        Zone current_zone = get_current_zone();

        double active_alpha = 0.90;
        double dim_alpha    = 0.18;

        // TOP zone: too far (red)
        // from max distance down to optimal threshold
        draw_zone(cr, bar_x + 2, bar_top + 2, bar_w - 4, opt_y - bar_top - 2,
                0.86, 0.15, 0.15,
                current_zone == Zone::APPROACH ? active_alpha : dim_alpha);

        // MIDDLE zone: optimal (green)
        // from optimal threshold down to warning threshold
        draw_zone(cr, bar_x + 2, opt_y, bar_w - 4, warn_y - opt_y,
                0.133, 0.773, 0.369,
                current_zone == Zone::OPTIMAL ? active_alpha : dim_alpha);

        // BOTTOM zone: too close (red)
        // from warning threshold down to 0.0m
        draw_zone(cr, bar_x + 2, warn_y, bar_w - 4, bar_bottom - 2 - warn_y,
                0.86, 0.15, 0.15,
                current_zone == Zone::WARNING ? active_alpha : dim_alpha);

        // --- Needle ---
        {
            double needle_y = dist_to_y(distance_m_);
            needle_y = std::max(bar_top + 2.0, std::min(bar_bottom - 2.0, needle_y));
 
            cr->set_source_rgb(1.0, 1.0, 1.0);
            cr->set_line_width(2.5);
            cr->move_to(bar_x + 2, needle_y);
            cr->line_to(bar_x + bar_w - 2, needle_y);
            cr->stroke();
        }

        // --- Tick labels ---
        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
        cr->set_font_size(10.0);

        double dim_tr, dim_tg, dim_tb;
        get_dim_tick_color(dim_tr, dim_tg, dim_tb);

        draw_tick(cr, dist_to_y(0.0), tick_x, bar_x + bar_w, "0.0m", dim_tr, dim_tg, dim_tb);
        draw_tick(cr, dist_to_y(warning_threshold_m_), tick_x, bar_x + bar_w,
                  format_dist(warning_threshold_m_), 0.86, 0.15, 0.15);
        draw_tick(cr, dist_to_y(optimal_threshold_m_), tick_x, bar_x + bar_w,
                  format_dist(optimal_threshold_m_), 0.133, 0.773, 0.369);
        draw_tick(cr, dist_to_y(max_distance_m_), tick_x, bar_x + bar_w,
                  format_dist(max_distance_m_), 0.86, 0.15, 0.15);

        // --- Distance readout ---
        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(20.0);
        if (current_zone == Zone::WARNING) {
            // Blink the distance text red when too close
            double text_alpha = 1.0;
            cr->set_source_rgba(0.94, 0.27, 0.27, text_alpha);
        } else {
            set_text_color(cr, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0);
        }
        std::string dist_text = format_dist(distance_m_);
        cr->get_text_extents(dist_text, te);
        cr->move_to((w - te.width) / 2.0, bar_bottom + 24);
        cr->show_text(dist_text);

        // --- Status badge (blinks only for WARNING) ---
        draw_status_badge(cr, w, bar_bottom + 32, current_zone);

        // Pop group and paint with fade alpha
        cr->pop_group_to_source();
        cr->paint_with_alpha(fade_alpha_);

        return true;
    }

private:
    double distance_m_;
    double max_distance_m_;
    double warning_threshold_m_;
    double optimal_threshold_m_;

    int arm_position_;
    int arm_show_threshold_;
    bool bar_visible_;
    double fade_alpha_;

    bool blink_on_;
    bool is_light_mode_;

    sigc::connection blink_connection_;
    sigc::connection fade_connection_;

    // --- Timers ---

    bool on_blink_tick() {
        if (get_current_zone() == Zone::WARNING && fade_alpha_ > 0.01) {
            blink_on_ = !blink_on_;
            queue_draw();
        } else {
            blink_on_ = true;
        }
        return true;
    }

    bool on_fade_tick() {
        double target = bar_visible_ ? 1.0 : 0.0;
        double step = 0.1;  // ~330ms full transition at 33ms ticks

        if (std::abs(fade_alpha_ - target) < step) {
            if (fade_alpha_ != target) {
                fade_alpha_ = target;
                queue_draw();
            }
        } else {
            fade_alpha_ += (target > fade_alpha_) ? step : -step;
            queue_draw();
        }
        return true;
    }

    // --- Drawing helpers ---

    void set_text_color(const Cairo::RefPtr<Cairo::Context>& cr,
                        double dr, double dg, double db,
                        double lr, double lg, double lb) {
        if (is_light_mode_) cr->set_source_rgb(lr, lg, lb);
        else                cr->set_source_rgb(dr, dg, db);
    }

    void get_dim_tick_color(double& r, double& g, double& b) {
        if (is_light_mode_) { r = 0.4; g = 0.4; b = 0.4; }
        else                { r = 0.40; g = 0.48; b = 0.53; }
    }

    void draw_zone(const Cairo::RefPtr<Cairo::Context>& cr,
                   double x, double y, double w, double h,
                   double r, double g, double b, double a) {
        if (h <= 0) return;
        cr->set_source_rgba(r, g, b, a);
        rounded_rect(cr, x, y, w, h, 3.0);
        cr->fill();
    }

    void draw_arrow_tip(const Cairo::RefPtr<Cairo::Context>& cr,
                        double tip_x, double tip_y, bool points_left) {
        double dx = points_left ? 6.0 : -6.0;
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->move_to(tip_x, tip_y);
        cr->line_to(tip_x + dx, tip_y - 4.5);
        cr->line_to(tip_x + dx, tip_y + 4.5);
        cr->close_path();
        cr->fill();
    }

    void draw_tick(const Cairo::RefPtr<Cairo::Context>& cr,
                   double y, double label_x, double bar_right,
                   const std::string& label, double r, double g, double b) {
        cr->set_source_rgba(r, g, b, 0.6);
        cr->set_line_width(1.0);
        cr->move_to(bar_right + 1, y);
        cr->line_to(bar_right + 4, y);
        cr->stroke();
        cr->set_source_rgb(r, g, b);
        cr->move_to(label_x, y + 3.5);
        cr->show_text(label);
    }

    void draw_status_badge(const Cairo::RefPtr<Cairo::Context>& cr,
                           double container_w, double badge_y, Zone zone) {
        std::string text;
        double bg_r, bg_g, bg_b, fg_r, fg_g, fg_b;

        switch (zone) {
            case Zone::WARNING:
                text = "TOO CLOSE";
                bg_r = 0.50; bg_g = 0.11; bg_b = 0.11;
                fg_r = 0.97; fg_g = 0.44; fg_b = 0.44;
                if (!blink_on_) {
                    bg_r *= 0.4; bg_g *= 0.4; bg_b *= 0.4;
                }
                break;
            case Zone::OPTIMAL:
                text = "OPTIMAL";
                bg_r = 0.08; bg_g = 0.33; bg_b = 0.18;
                fg_r = 0.29; fg_g = 0.85; fg_b = 0.50;
                break;
            case Zone::APPROACH:
                text = "TOO FAR";
                bg_r = 0.50; bg_g = 0.11; bg_b = 0.11;
                fg_r = 0.97; fg_g = 0.44; fg_b = 0.44;
                break;
        }

        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(11.0);
        Cairo::TextExtents te;
        cr->get_text_extents(text, te);
        double bw = te.width + 18;
        double bh = 22;
        double bx = (container_w - bw) / 2.0;

        cr->set_source_rgb(bg_r, bg_g, bg_b);
        rounded_rect(cr, bx, badge_y, bw, bh, 4.0);
        cr->fill();

        cr->set_source_rgb(fg_r, fg_g, fg_b);
        cr->move_to(bx + 9, badge_y + 15.5);
        cr->show_text(text);
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

    std::string format_dist(double d) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1fm", d);
        return std::string(buf);
    }
};