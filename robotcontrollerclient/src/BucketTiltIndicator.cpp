#include "BucketTiltIndicator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr double DEG2RAD = M_PI / 180.0;

inline double clampd(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}
} // namespace

BucketTiltIndicator::BucketTiltIndicator(const std::string& title)
    : title_(title)
{
    set_size_request(220, 240);
}

void BucketTiltIndicator::set_angle(double degrees) {
    if (degrees == angle_deg_) return;
    angle_deg_ = degrees;
    queue_draw();
}

void BucketTiltIndicator::set_warning_angles(double low_deg, double high_deg) {
    low_deg_  = low_deg;
    high_deg_ = high_deg;
    queue_draw();
}

void BucketTiltIndicator::set_caution_margin(double margin_deg) {
    caution_margin_deg_ = std::max(0.0, margin_deg);
    queue_draw();
}

void BucketTiltIndicator::set_light_mode(bool light) {
    if (light == light_mode_) return;
    light_mode_ = light;
    queue_draw();
}

void BucketTiltIndicator::status_color(double& r, double& g, double& b) const {
    // Hard limit -> red.
    if (angle_deg_ <= low_deg_ || angle_deg_ >= high_deg_) {
        r = 0.90; g = 0.22; b = 0.22;
        return;
    }
    // Caution band -> amber.
    const double low_caution  = low_deg_  + caution_margin_deg_;
    const double high_caution = high_deg_ - caution_margin_deg_;
    if (angle_deg_ < low_caution || angle_deg_ > high_caution) {
        r = 0.98; g = 0.72; b = 0.18;
        return;
    }
    // Safe -> green.
    r = 0.29; g = 0.78; b = 0.45;
}

const char* BucketTiltIndicator::status_text() const {
    // Past upper limit / near full dump.
    if (angle_deg_ >= high_deg_ - caution_margin_deg_) return "DUMPING";
    // Past lower limit / over-curled.
    if (angle_deg_ <= low_deg_ + caution_margin_deg_)  return "CRADLED";
    if (std::fabs(angle_deg_) < 5.0)                   return "LEVEL";
    return (angle_deg_ > 0.0) ? "TILTED FWD" : "TILTED BACK";
}

void BucketTiltIndicator::draw_arc_ticks(const Cairo::RefPtr<Cairo::Context>& cr,
                                         double cx, double cy, double radius) const
{
    // Background ring (the "track").
    if (light_mode_) cr->set_source_rgb(0.85, 0.85, 0.85);
    else             cr->set_source_rgb(0.20, 0.20, 0.22);
    cr->set_line_width(8.0);
    cr->arc(cx, cy, radius, M_PI, 2.0 * M_PI); // top half only
    cr->stroke();

    // Coloured sweep from 0 deg up to current angle, clipped to limits.
    const double clamped = clampd(angle_deg_, low_deg_, high_deg_);
    double r, g, b;
    status_color(r, g, b);
    cr->set_source_rgb(r, g, b);
    cr->set_line_width(8.0);

    // We map angle 0 -> straight up (-pi/2). Positive = clockwise (fwd dump).
    const double start_a = -M_PI / 2.0;
    const double end_a   = -M_PI / 2.0 + clamped * DEG2RAD;
    if (end_a >= start_a) cr->arc(cx, cy, radius, start_a, end_a);
    else                  cr->arc_negative(cx, cy, radius, start_a, end_a);
    cr->stroke();

    // Tick marks every 15 deg across the full arc.
    cr->set_line_width(1.5);
    if (light_mode_) cr->set_source_rgb(0.25, 0.25, 0.25);
    else             cr->set_source_rgb(0.85, 0.85, 0.85);
    for (int deg = -90; deg <= 90; deg += 15) {
        const double a = -M_PI / 2.0 + deg * DEG2RAD;
        const bool major = (deg % 45 == 0);
        const double inner = radius - (major ? 12.0 : 6.0);
        const double outer = radius + 4.0;
        cr->move_to(cx + std::cos(a) * inner, cy + std::sin(a) * inner);
        cr->line_to(cx + std::cos(a) * outer, cy + std::sin(a) * outer);
        cr->stroke();
    }

    // Zero (level) marker — bold red-orange line for the operator's eye.
    cr->set_source_rgb(0.95, 0.55, 0.10);
    cr->set_line_width(2.5);
    cr->move_to(cx, cy - radius - 6.0);
    cr->line_to(cx, cy - radius + 14.0);
    cr->stroke();
}

void BucketTiltIndicator::draw_bucket(const Cairo::RefPtr<Cairo::Context>& cr,
                                      double cx, double cy, double size,
                                      double angle_rad) const
{
    cr->save();
    cr->translate(cx, cy);
    cr->rotate(angle_rad);

    // Bucket silhouette, drawn around its pivot.
    // Dimensions are normalised to `size` (the gauge target size).
    const double s = size * 0.55;                 // overall scale
    const double w_top    =  0.42 * s;            // top opening half-width
    const double w_bottom =  0.30 * s;            // bottom half-width
    const double depth    =  0.55 * s;            // bucket depth
    const double back_h   =  0.08 * s;            // back wall thickness at top

    // Body fill colour — uses the status colour at low saturation so the
    // bucket itself reflects state without screaming.
    double r, g, b;
    status_color(r, g, b);

    // Outer shell (the steel).
    cr->set_source_rgb(light_mode_ ? 0.30 : 0.78,
                       light_mode_ ? 0.30 : 0.78,
                       light_mode_ ? 0.32 : 0.80);
    cr->move_to(-w_top, 0);
    cr->line_to(-w_bottom, depth);
    // curved scoop floor
    cr->curve_to(-w_bottom * 0.5,  depth + 0.20 * s,
                  w_bottom * 0.5,  depth + 0.20 * s,
                  w_bottom,        depth);
    cr->line_to( w_top, 0);
    cr->line_to( w_top, -back_h);
    cr->line_to(-w_top, -back_h);
    cr->close_path();
    cr->fill_preserve();

    cr->set_source_rgb(0.10, 0.10, 0.12);
    cr->set_line_width(2.0);
    cr->stroke();

    // Inner cavity tinted with status colour at ~35% so a glance tells you
    // the safe/caution/danger state without reading the number.
    cr->set_source_rgba(r, g, b, 0.55);
    cr->move_to(-w_top + 4, 2);
    cr->line_to(-w_bottom + 3, depth - 2);
    cr->curve_to(-w_bottom * 0.5, depth + 0.20 * s - 4,
                  w_bottom * 0.5, depth + 0.20 * s - 4,
                  w_bottom - 3,   depth - 2);
    cr->line_to(w_top - 4, 2);
    cr->close_path();
    cr->fill();

    // Cutting edge / lip — the bright reference line at the bucket's mouth.
    cr->set_source_rgb(0.98, 0.83, 0.10);
    cr->set_line_width(3.5);
    cr->move_to(-w_top, 0);
    cr->line_to( w_top, 0);
    cr->stroke();

    // Pivot marker.
    cr->set_source_rgb(0.10, 0.10, 0.12);
    cr->arc(0, 0, 4.0, 0, 2.0 * M_PI);
    cr->fill();

    cr->restore();

    // Horizon reference line through the pivot — does NOT rotate.
    cr->set_source_rgba(light_mode_ ? 0.20 : 0.95,
                        light_mode_ ? 0.20 : 0.95,
                        light_mode_ ? 0.20 : 0.95,
                        0.45);
    cr->set_line_width(1.0);
    // dashed
    std::vector<double> dashes = {4.0, 4.0};
    cr->set_dash(dashes, 0.0);
    cr->move_to(cx - size * 0.40, cy);
    cr->line_to(cx + size * 0.40, cy);
    cr->stroke();
    cr->unset_dash();
}

void BucketTiltIndicator::draw_readout(const Cairo::RefPtr<Cairo::Context>& cr,
                                       double cx, double y, double width) const
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%+.1f\u00B0", angle_deg_); // +12.3°

    // Big angle number.
    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
    cr->set_font_size(28.0);
    Cairo::TextExtents te;
    cr->get_text_extents(buf, te);

    double r, g, b;
    status_color(r, g, b);
    cr->set_source_rgb(r, g, b);
    cr->move_to(cx - te.width / 2.0 - te.x_bearing, y);
    cr->show_text(buf);

    // Status word underneath ("LEVEL" / "DUMPING" / ...).
    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
    cr->set_font_size(13.0);
    const char* status = status_text();
    cr->get_text_extents(status, te);
    if (light_mode_) cr->set_source_rgb(0.25, 0.25, 0.25);
    else             cr->set_source_rgb(0.80, 0.80, 0.82);
    cr->move_to(cx - te.width / 2.0 - te.x_bearing, y + 20.0);
    cr->show_text(status);

    // Title at top of widget — drawn here for convenience.
    (void)width;
}

bool BucketTiltIndicator::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    Gtk::Allocation alloc = get_allocation();
    const double W = alloc.get_width();
    const double H = alloc.get_height();

    // Background.
    if (light_mode_) cr->set_source_rgb(0.96, 0.96, 0.97);
    else             cr->set_source_rgb(0.10, 0.10, 0.12);
    cr->rectangle(0, 0, W, H);
    cr->fill();

    // Title.
    cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
    cr->set_font_size(12.0);
    Cairo::TextExtents te;
    cr->get_text_extents(title_, te);
    if (light_mode_) cr->set_source_rgb(0.20, 0.20, 0.20);
    else             cr->set_source_rgb(0.88, 0.88, 0.90);
    cr->move_to((W - te.width) / 2.0 - te.x_bearing, 18.0);
    cr->show_text(title_);

    // Geometry: gauge centred horizontally, biased to upper portion of
    // the widget so the numeric readout has room below.
    const double size = std::min(W, H - 60.0);
    const double cx   = W / 2.0;
    const double cy   = 30.0 + size * 0.55;     // gauge pivot
    const double radius = size * 0.42;

    draw_arc_ticks(cr, cx, cy, radius);
    draw_bucket(cr, cx, cy, size, angle_deg_ * DEG2RAD);
    draw_readout(cr, cx, cy + radius + 28.0, W);

    return true;
}
