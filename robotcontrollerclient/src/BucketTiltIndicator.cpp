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
    // Background ring (the "track") — right half only (270 deg to 90 deg).
    if (light_mode_) cr->set_source_rgb(0.85, 0.85, 0.85);
    else             cr->set_source_rgb(0.20, 0.20, 0.22);
    cr->set_line_width(8.0);
    cr->arc(cx, cy, radius, -M_PI / 2.0, M_PI / 2.0); // right half (270->90)
    cr->stroke();

    // Coloured sweep from 0 deg to current angle, clipped to limits.
    const double clamped = clampd(angle_deg_, low_deg_, high_deg_);
    double r, g, b;
    status_color(r, g, b);
    cr->set_source_rgb(r, g, b);
    cr->set_line_width(8.0);

    // Angle 0 -> pointing right (0 rad). Positive = clockwise (down), negative = counter-clockwise (up).
    const double start_a = 0.0;
    const double end_a   = clamped * DEG2RAD;
    if (end_a >= start_a) cr->arc(cx, cy, radius, start_a, end_a);
    else                  cr->arc_negative(cx, cy, radius, start_a, end_a);
    cr->stroke();

    // Tick marks every 15 deg from 270 (-90) to 90.
    cr->set_line_width(1.5);
    if (light_mode_) cr->set_source_rgb(0.25, 0.25, 0.25);
    else             cr->set_source_rgb(0.85, 0.85, 0.85);
    for (int deg = -90; deg <= 90; deg += 15) {
        const double a = deg * DEG2RAD;  // 0 = right, -90 = up, +90 = down
        const bool major = (deg % 45 == 0);
        const double inner = radius - (major ? 12.0 : 6.0);
        const double outer = radius + 4.0;
        cr->move_to(cx + std::cos(a) * inner, cy + std::sin(a) * inner);
        cr->line_to(cx + std::cos(a) * outer, cy + std::sin(a) * outer);
        cr->stroke();
    }

    // Zero (level) marker — bold red-orange line pointing right (0 deg).
    cr->set_source_rgb(0.95, 0.55, 0.10);
    cr->set_line_width(2.5);
    cr->move_to(cx + radius - 14.0, cy);
    cr->line_to(cx + radius +  6.0, cy);
    cr->stroke();
}

void BucketTiltIndicator::draw_bucket(const Cairo::RefPtr<Cairo::Context>& cr,
                                      double cx, double cy, double size,
                                      double angle_rad) const
{
    cr->save();
    cr->translate(cx, cy);
    cr->rotate(angle_rad);

    // Side-profile triangle representing a skid steer bucket.
    // Pivot/mount point is at the top-left (back of bucket).
    // The triangle reads: left=back wall, bottom=floor, hypotenuse=angled front face.
    //
    //  (0,0) pivot/back-top
    //    |  \
    //    |    \   <- angled top/front face (hypotenuse)
    //    |      \
    //    +--------+  <- floor with cutting edge at front-bottom right
    //
    const double s       = size * 0.50;
    const double floor_w = 0.85 * s;   // floor length (horizontal)
    const double back_h  = 0.50 * s;   // back wall height (vertical)

    double r, g, b;
    status_color(r, g, b);

    // Main body fill — steel grey.
    cr->set_source_rgb(light_mode_ ? 0.35 : 0.68,
                       light_mode_ ? 0.35 : 0.68,
                       light_mode_ ? 0.37 : 0.70);
    cr->move_to(0,       0);        // pivot: back-top
    cr->line_to(0,       back_h);   // back-bottom
    cr->line_to(floor_w, back_h);   // front-bottom (cutting edge)
    cr->close_path();               // hypotenuse back to pivot
    cr->fill_preserve();

    // Outline.
    cr->set_source_rgb(0.08, 0.08, 0.10);
    cr->set_line_width(2.0);
    cr->stroke();

    // Inner cavity tinted with status colour so state is visible at a glance.
    cr->set_source_rgba(r, g, b, 0.35);
    const double inset = 5.0;
    cr->move_to(inset,           inset);
    cr->line_to(inset,           back_h - inset);
    cr->line_to(floor_w - inset, back_h - inset);
    cr->close_path();
    cr->fill();

    // Cutting edge — bright yellow tip at the front-bottom corner.
    cr->set_source_rgb(0.98, 0.83, 0.10);
    cr->set_line_width(3.5);
    cr->move_to(floor_w,      back_h - 12.0);
    cr->line_to(floor_w + 1.0, back_h +  2.0);
    cr->stroke();

    // Back wall accent — lighter stripe so the rear face reads clearly.
    cr->set_source_rgb(light_mode_ ? 0.55 : 0.90,
                       light_mode_ ? 0.55 : 0.90,
                       light_mode_ ? 0.57 : 0.92);
    cr->set_line_width(3.0);
    cr->move_to(0, 0);
    cr->line_to(0, back_h);
    cr->stroke();

    // Pivot marker — white filled circle with dark ring.
    cr->set_source_rgb(0.95, 0.95, 0.95);
    cr->arc(0, 0, 4.0, 0, 2.0 * M_PI);
    cr->fill();
    cr->set_source_rgb(0.08, 0.08, 0.10);
    cr->set_line_width(1.5);
    cr->arc(0, 0, 4.0, 0, 2.0 * M_PI);
    cr->stroke();

    cr->restore();

    // Horizon reference line — does NOT rotate, helps operator read the angle.
    cr->set_source_rgba(light_mode_ ? 0.20 : 0.95,
                        light_mode_ ? 0.20 : 0.95,
                        light_mode_ ? 0.20 : 0.95,
                        0.45);
    cr->set_line_width(1.0);
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
    draw_bucket(cr, cx, cy, size, -angle_deg_ * DEG2RAD);
    draw_readout(cr, cx, cy + radius + 28.0, W);

    return true;
}
