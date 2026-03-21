#pragma once

#include <gtkmm.h>
#include <cairomm/context.h>
#include <string>
#include <cmath>

/**
 * ArtificialHorizon - Combined roll + pitch attitude indicator.
 * 
 * A single gauge that shows both roll and pitch simultaneously:
 *   - The horizon line TILTS for roll (rotation around the center)
 *   - The horizon line SHIFTS vertically for pitch (translation)
 *   - Sky (blue) is always above the horizon, ground (brown) below
 *   - Fixed wing/reference markers stay centered
 *   - Pitch ladder lines show degree markings
 *   - Warning coloring when either axis exceeds limits
 *   - Numeric readouts for both axes
 * 
 * This matches the standard attitude indicator used in aviation and
 * ROV interfaces — one instrument, two axes, instantly readable.
 * 
 * Usage:
 *   ArtificialHorizon* attitude = Gtk::manage(new ArtificialHorizon());
 *   attitude->set_size_request(200, 200);
 *   parent->add(*attitude);
 * 
 *   // In handleZedElements:
 *   attitude->set_roll(roll_degrees);
 *   attitude->set_pitch(pitch_degrees);
 */
class ArtificialHorizon : public Gtk::DrawingArea {
public:
    ArtificialHorizon(const std::string& label = "Attitude")
        : label_(label),
          roll_deg_(0.0), pitch_deg_(0.0),
          roll_warn_(30.0), pitch_warn_(30.0),
          is_light_mode_(true) {}

    void set_roll(double degrees) { roll_deg_ = degrees; queue_draw(); }
    void set_pitch(double degrees) { pitch_deg_ = degrees; queue_draw(); }

    /** Convenience: set both at once. */
    void set_attitude(double roll_deg, double pitch_deg) {
        roll_deg_ = roll_deg;
        pitch_deg_ = pitch_deg;
        queue_draw();
    }

    double get_roll() const { return roll_deg_; }
    double get_pitch() const { return pitch_deg_; }

    /** Set ± limits for roll warning. Default: ±30° */
    void set_roll_warning(double degrees) { roll_warn_ = std::abs(degrees); }
    /** Set ± limits for pitch warning. Default: ±30° */
    void set_pitch_warning(double degrees) { pitch_warn_ = std::abs(degrees); }

    /** Shortcut matching old API — sets both warnings to the same value. */
    void set_warning_angles(double high, double low) {
        roll_warn_ = std::abs(high);
        pitch_warn_ = std::abs(high);
    }

    /** Legacy single-axis API — routes to set_roll for backward compat. */
    void set_angle(double degrees) { set_roll(degrees); }

    void set_label(const std::string& label) { label_ = label; }
    void set_light_mode(bool light) { is_light_mode_ = light; queue_draw(); }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override {
        const int w = get_allocated_width();
        const int h = get_allocated_height();

        // Layout
        const double readout_h = 34.0;   // Space for numeric readouts at bottom
        const double cx = w / 2.0;
        const double cy = (h - readout_h) / 2.0;
        const double radius = std::min(cx, cy) - 6.0;

        bool roll_warning = std::abs(roll_deg_) > roll_warn_;
        bool pitch_warning = std::abs(pitch_deg_) > pitch_warn_;
        bool any_warning = roll_warning || pitch_warning;

        double roll_rad = roll_deg_ * M_PI / 180.0;

        // Pitch drives vertical offset of the horizon.
        // Scale: at pitch_warn_ degrees, the horizon reaches the edge of the circle.
        double pitch_pixels_per_deg = (radius * 0.7) / pitch_warn_;
        double pitch_offset = -pitch_deg_ * pitch_pixels_per_deg;

        // --- Outer ring ---
        cr->set_line_width(2.0);
        if (is_light_mode_) {
            cr->set_source_rgba(0.55, 0.55, 0.55, 0.7);
        } else {
            cr->set_source_rgba(0.22, 0.30, 0.35, 0.9);
        }
        cr->arc(cx, cy, radius, 0, 2 * M_PI);
        cr->stroke();

        // --- Sky/ground fill (clipped to circle, rotated + shifted) ---
        cr->save();
        cr->arc(cx, cy, radius - 1, 0, 2 * M_PI);
        cr->clip();

        cr->save();
        cr->translate(cx, cy);
        cr->rotate(roll_rad);

        // Sky
        cr->set_source_rgba(0.15, 0.35, 0.65, 0.40);
        cr->rectangle(-radius * 2, -radius * 2, radius * 4, radius * 2 + pitch_offset);
        cr->fill();

        // Ground
        cr->set_source_rgba(0.40, 0.25, 0.12, 0.35);
        cr->rectangle(-radius * 2, pitch_offset, radius * 4, radius * 2);
        cr->fill();

        // --- Horizon line ---
        {
            double r, g, b;
            if (any_warning) {
                r = 0.94; g = 0.27; b = 0.27;
            } else {
                double ratio = std::max(std::abs(roll_deg_) / roll_warn_,
                                        std::abs(pitch_deg_) / pitch_warn_);
                ratio = std::min(ratio, 1.0);
                if (ratio > 0.7) {
                    double t = (ratio - 0.7) / 0.3;
                    r = 0.98 * t + 0.38 * (1 - t);
                    g = 0.75 * t + 0.65 * (1 - t);
                    b = 0.17 * t + 0.98 * (1 - t);
                } else {
                    r = 0.38; g = 0.65; b = 0.98;
                }
            }

            cr->set_source_rgb(r, g, b);
            cr->set_line_width(2.5);
            cr->move_to(-radius * 1.5, pitch_offset);
            cr->line_to(radius * 1.5, pitch_offset);
            cr->stroke();
        }

        // --- Pitch ladder lines (every 10° above and below horizon) ---
        {
            cr->set_line_width(1.0);
            double ladder_half_w = radius * 0.25;
            for (int deg = -40; deg <= 40; deg += 10) {
                if (deg == 0) continue;
                double y = pitch_offset - deg * pitch_pixels_per_deg;

                // Only draw if visible within the circle
                if (std::abs(y) > radius * 0.85) continue;

                if (is_light_mode_) {
                    cr->set_source_rgba(0.3, 0.3, 0.3, 0.5);
                } else {
                    cr->set_source_rgba(0.7, 0.7, 0.7, 0.4);
                }

                // Dashed for negative pitch, solid for positive
                if (deg < 0) {
                    std::vector<double> dashes = {4.0, 3.0};
                    cr->set_dash(dashes, 0);
                }
                cr->move_to(-ladder_half_w, y);
                cr->line_to(ladder_half_w, y);
                cr->stroke();
                cr->unset_dash();

                // Small degree label
                cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
                cr->set_font_size(8.0);
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%d", deg);
                cr->move_to(ladder_half_w + 4, y + 3);
                cr->show_text(buf);
            }
        }

        cr->restore(); // Undo rotate+translate
        cr->reset_clip();
        cr->restore(); // Undo clip

        // --- Roll tick marks around the outer ring ---
        {
            cr->set_line_width(1.0);
            double tick_inner = radius - 6;
            double tick_outer = radius - 1;

            for (int deg = -60; deg <= 60; deg += 10) {
                if (deg == 0) continue;
                double a = (deg - 90) * M_PI / 180.0;

                bool is_warn_tick = (std::abs(deg) >= (int)roll_warn_);
                if (is_warn_tick) {
                    cr->set_source_rgba(0.94, 0.27, 0.27, 0.5);
                } else {
                    if (is_light_mode_) cr->set_source_rgba(0.5, 0.5, 0.5, 0.4);
                    else cr->set_source_rgba(0.4, 0.48, 0.53, 0.4);
                }

                double inner = (deg % 30 == 0) ? tick_inner - 4 : tick_inner;
                cr->move_to(cx + inner * std::cos(a), cy + inner * std::sin(a));
                cr->line_to(cx + tick_outer * std::cos(a), cy + tick_outer * std::sin(a));
                cr->stroke();
            }
        }

        // --- Roll pointer (small triangle at top, rotates with roll) ---
        {
            cr->save();
            cr->translate(cx, cy);
            cr->rotate(roll_rad);

            double ptr_y = -(radius - 10);
            cr->set_source_rgb(1.0, 1.0, 1.0);
            cr->move_to(0, ptr_y);
            cr->line_to(-5, ptr_y - 8);
            cr->line_to(5, ptr_y - 8);
            cr->close_path();
            cr->fill();

            cr->restore();
        }

        // --- Fixed reference marker (top center, stationary) ---
        {
            double top_y = cy - radius + 10;
            if (is_light_mode_) cr->set_source_rgba(0.3, 0.3, 0.3, 0.6);
            else cr->set_source_rgba(0.6, 0.6, 0.6, 0.6);
            cr->move_to(cx, top_y);
            cr->line_to(cx - 5, top_y - 7);
            cr->line_to(cx + 5, top_y - 7);
            cr->close_path();
            cr->stroke();
        }

        // --- Fixed wing markers (center, green) ---
        {
            cr->set_source_rgb(0.29, 0.85, 0.50);
            cr->set_line_width(2.5);

            // Left wing
            cr->move_to(cx - radius + 14, cy);
            cr->line_to(cx - radius * 0.35, cy);
            cr->stroke();
            // Left wing down-tick
            cr->move_to(cx - radius * 0.35, cy);
            cr->line_to(cx - radius * 0.35, cy + 6);
            cr->stroke();

            // Right wing
            cr->move_to(cx + radius - 14, cy);
            cr->line_to(cx + radius * 0.35, cy);
            cr->stroke();
            // Right wing down-tick
            cr->move_to(cx + radius * 0.35, cy);
            cr->line_to(cx + radius * 0.35, cy + 6);
            cr->stroke();

            // Center dot
            cr->arc(cx, cy, 3, 0, 2 * M_PI);
            cr->fill();
        }

        // --- Numeric readouts ---
        double readout_y = cy + radius + 16;

        cr->select_font_face("monospace", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(12.0);

        // Roll readout (left side)
        {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "R %.1f\xC2\xB0", roll_deg_);
            std::string text(buf);

            if (roll_warning) cr->set_source_rgb(0.94, 0.27, 0.27);
            else set_text_color(cr);

            Cairo::TextExtents te;
            cr->get_text_extents(text, te);
            cr->move_to(cx - radius * 0.5 - te.width / 2.0, readout_y);
            cr->show_text(text);
        }

        // Pitch readout (right side)
        {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "P %.1f\xC2\xB0", pitch_deg_);
            std::string text(buf);

            if (pitch_warning) cr->set_source_rgb(0.94, 0.27, 0.27);
            else set_text_color(cr);

            Cairo::TextExtents te;
            cr->get_text_extents(text, te);
            cr->move_to(cx + radius * 0.5 - te.width / 2.0, readout_y);
            cr->show_text(text);
        }

        // --- Label ---
        cr->set_font_size(10.0);
        if (is_light_mode_) cr->set_source_rgba(0.3, 0.3, 0.3, 0.8);
        else cr->set_source_rgba(0.55, 0.63, 0.67, 0.8);
        Cairo::TextExtents lte;
        cr->get_text_extents(label_, lte);
        cr->move_to(cx - lte.width / 2.0, readout_y + 14);
        cr->show_text(label_);

        return true;
    }

private:
    std::string label_;
    double roll_deg_;
    double pitch_deg_;
    double roll_warn_;
    double pitch_warn_;
    bool is_light_mode_;

    void set_text_color(const Cairo::RefPtr<Cairo::Context>& cr) {
        if (is_light_mode_) cr->set_source_rgb(0.0, 0.0, 0.0);
        else cr->set_source_rgb(1.0, 1.0, 1.0);
    }
};