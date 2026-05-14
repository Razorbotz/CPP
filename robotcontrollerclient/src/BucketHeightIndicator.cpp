#include "BucketHeightIndicator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
constexpr double DEG2RAD = M_PI / 180.0;
} // namespace

BucketHeightIndicator::BucketHeightIndicator(const std::string& title)
    : title_(title)
{
    set_size_request(260, 240);
}

void BucketHeightIndicator::set_geometry_cm(double H, double L_arm, double L_bucket) {
    H_ = H;
    L_arm_ = L_arm;
    L_bucket_ = L_bucket;
    queue_draw();
}

void BucketHeightIndicator::set_angles_deg(double arm_deg, double bucket_deg) {
    if (arm_deg == arm_deg_ && bucket_deg == bucket_deg_) return;
    arm_deg_ = arm_deg;
    bucket_deg_ = bucket_deg;
    queue_draw();
}

void BucketHeightIndicator::set_thresholds_cm(double caution_cm, double danger_cm) {
    caution_cm_ = caution_cm;
    danger_cm_  = danger_cm;
    queue_draw();
}

void BucketHeightIndicator::set_light_mode(bool light) {
    if (light == light_mode_) return;
    light_mode_ = light;
    queue_draw();
}

void BucketHeightIndicator::compute_geometry(double& arm_pivot_x, double& arm_pivot_y,
                                             double& bucket_pivot_x, double& bucket_pivot_y,
                                             double& tip_x, double& tip_y) const
{
    // World coords: x = forward (positive = front of bot), y = up.
    // Arm pivot sits at (0, H_). Same sign conventions as the existing
    // updateBucketElevationBar(): positive arm angle drops the bucket
    // (we use sin to *subtract* height), so the arm points forward and
    // down for positive angles. In world coords that maps to:
    //   arm_pivot -> bucket_pivot vector = ( cos(arm), -sin(arm) ) * L_arm
    // and similarly for the bucket link with the combined angle.
    const double arm_r    = arm_deg_ * DEG2RAD;
    const double bucket_r = (arm_deg_ + bucket_deg_) * DEG2RAD;

    arm_pivot_x = 0.0;
    arm_pivot_y = H_;

    bucket_pivot_x = arm_pivot_x + std::cos(arm_r) * L_arm_;
    bucket_pivot_y = arm_pivot_y - std::sin(arm_r) * L_arm_;

    tip_x = bucket_pivot_x + std::cos(bucket_r) * L_bucket_;
    tip_y = bucket_pivot_y - std::sin(bucket_r) * L_bucket_;
}

void BucketHeightIndicator::status_color(double& r, double& g, double& b) const {
    if (cached_height_cm_ <= danger_cm_) {
        // At or under the ground -> red (collision / digging too deep).
        r = 0.90; g = 0.22; b = 0.22;
    } else if (cached_height_cm_ <= caution_cm_) {
        r = 0.98; g = 0.72; b = 0.18;
    } else {
        r = 0.29; g = 0.78; b = 0.45;
    }
}

const char* BucketHeightIndicator::status_text() const {
    if (cached_height_cm_ < danger_cm_)   return "BELOW GROUND";
    if (cached_height_cm_ < caution_cm_)  return "DIGGING";
    if (cached_height_cm_ < 25.0)         return "LOW CARRY";
    if (cached_height_cm_ < 60.0)         return "CARRY";
    return "RAISED";
}

bool BucketHeightIndicator::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
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

    // World-space extents we need to fit on screen:
    //   x: 0 (arm pivot) ... arm_pivot + L_arm + L_bucket (worst case forward reach)
    //   y: 0 (ground) ... H_ + L_arm + L_bucket (worst case vertical reach)
    // We pad both sides so the chassis + bucket fits even at extremes.
    double apx, apy, bpx, bpy, tx, ty;
    compute_geometry(apx, apy, bpx, bpy, tx, ty);
    cached_height_cm_ = ty;

    const double world_max_x = L_arm_ + L_bucket_ + 40.0;       // ~chassis half-length behind pivot
    const double world_min_x = -60.0;                            // chassis behind arm pivot
    const double world_max_y = H_ + L_arm_ + L_bucket_ + 10.0;
    const double world_min_y = -10.0;                            // a touch below ground

    // Drawing rect (everything below the title, leave room for readout).
    const double draw_top    = 30.0;
    const double draw_bottom = H - 60.0;
    const double draw_left   = 12.0;
    const double draw_right  = W - 12.0;

    const double sx = (draw_right - draw_left) / (world_max_x - world_min_x);
    const double sy = (draw_bottom - draw_top) / (world_max_y - world_min_y);
    const double scale = std::min(sx, sy);

    // Place ground line so the ground (y_world = 0) shows near the bottom.
    const double origin_x = draw_left + (-world_min_x) * scale;       // x_world = 0 -> here
    const double origin_y = draw_bottom - (-world_min_y) * scale;     // y_world = 0 -> here

    auto wx = [&](double x){ return origin_x + x * scale; };
    auto wy = [&](double y){ return origin_y - y * scale; };

    // --- Ground line + soil hatching ---
    if (light_mode_) cr->set_source_rgb(0.45, 0.32, 0.20);
    else             cr->set_source_rgb(0.65, 0.50, 0.32);
    cr->set_line_width(2.0);
    cr->move_to(draw_left, wy(0));
    cr->line_to(draw_right, wy(0));
    cr->stroke();

    // Hatch marks below ground line.
    cr->set_line_width(1.0);
    for (double x = draw_left; x < draw_right; x += 10.0) {
        cr->move_to(x, wy(0));
        cr->line_to(x - 6.0, wy(0) + 8.0);
        cr->stroke();
    }

    // --- Height callout (vertical dimension from ground to tip) ---
    {
        double r, g, b;
        status_color(r, g, b);
        cr->set_source_rgba(r, g, b, 0.85);
        cr->set_line_width(1.5);
        std::vector<double> dashes = {3.0, 3.0};
        cr->set_dash(dashes, 0.0);
        const double dim_x = wx(tx);
        cr->move_to(dim_x, wy(0));
        cr->line_to(dim_x, wy(ty));
        cr->stroke();
        cr->unset_dash();

        // Small horizontal tick at the tip end.
        cr->move_to(dim_x - 6, wy(ty));
        cr->line_to(dim_x + 6, wy(ty));
        cr->stroke();
    }

    // --- Chassis (a rounded box behind the arm pivot) ---
    {
        const double cx0 = wx(-55.0);
        const double cx1 = wx(-5.0);
        const double cy0 = wy(35.0);    // chassis top
        const double cy1 = wy(0.0);     // ground

        if (light_mode_) cr->set_source_rgb(0.55, 0.55, 0.58);
        else             cr->set_source_rgb(0.40, 0.42, 0.46);
        cr->rectangle(cx0, cy0, cx1 - cx0, cy1 - cy0);
        cr->fill_preserve();
        cr->set_source_rgb(0.10, 0.10, 0.12);
        cr->set_line_width(1.5);
        cr->stroke();

        // Two wheels.
        const double wheel_r_world = 21.0;  // ~URDF wheel radius * 100
        cr->set_source_rgb(0.18, 0.18, 0.20);
        cr->arc(wx(-45.0), wy(wheel_r_world), wheel_r_world * scale, 0, 2 * M_PI);
        cr->fill();
        cr->arc(wx(-15.0), wy(wheel_r_world), wheel_r_world * scale, 0, 2 * M_PI);
        cr->fill();
        // Wheel hubs.
        cr->set_source_rgb(0.65, 0.65, 0.68);
        cr->arc(wx(-45.0), wy(wheel_r_world), wheel_r_world * 0.35 * scale, 0, 2 * M_PI);
        cr->fill();
        cr->arc(wx(-15.0), wy(wheel_r_world), wheel_r_world * 0.35 * scale, 0, 2 * M_PI);
        cr->fill();
    }

    // --- Arm link ---
    cr->set_source_rgb(light_mode_ ? 0.30 : 0.78,
                       light_mode_ ? 0.30 : 0.78,
                       light_mode_ ? 0.32 : 0.80);
    cr->set_line_width(7.0);
    cr->set_line_cap(Cairo::LINE_CAP_ROUND);
    cr->move_to(wx(apx), wy(apy));
    cr->line_to(wx(bpx), wy(bpy));
    cr->stroke();

    // Arm pivot.
    cr->set_source_rgb(0.10, 0.10, 0.12);
    cr->arc(wx(apx), wy(apy), 5.0, 0, 2 * M_PI);
    cr->fill();
    cr->set_source_rgb(0.85, 0.65, 0.10);
    cr->arc(wx(apx), wy(apy), 2.5, 0, 2 * M_PI);
    cr->fill();

    // --- Bucket: short link + scoop silhouette ---
    {
        // Bucket "back wall" from bucket pivot toward the tip direction.
        cr->set_source_rgb(light_mode_ ? 0.30 : 0.78,
                           light_mode_ ? 0.30 : 0.78,
                           light_mode_ ? 0.32 : 0.80);
        cr->set_line_width(5.0);
        cr->move_to(wx(bpx), wy(bpy));
        cr->line_to(wx(tx), wy(ty));
        cr->stroke();

        // Scoop curve: draw a small curl from the tip back toward the
        // bucket pivot, perpendicular to the bucket link, suggesting the
        // scoop interior.
        const double bucket_r = (arm_deg_ + bucket_deg_) * DEG2RAD;
        // Perpendicular ("down/inside" of the bucket) unit vector:
        const double nx = -std::sin(bucket_r);
        const double ny = -std::cos(bucket_r);
        const double curl = L_bucket_ * 0.45;  // depth of the scoop

        const double mid_x = (bpx + tx) * 0.5;
        const double mid_y = (bpy + ty) * 0.5;
        const double ctrl_x = mid_x + nx * curl;
        const double ctrl_y = mid_y + ny * curl;

        double r, g, b;
        status_color(r, g, b);
        cr->set_source_rgba(r, g, b, 0.55);
        cr->move_to(wx(bpx), wy(bpy));
        cr->curve_to(wx(ctrl_x), wy(ctrl_y),
                     wx(ctrl_x), wy(ctrl_y),
                     wx(tx),     wy(ty));
        cr->close_path();
        cr->fill_preserve();

        cr->set_source_rgb(0.10, 0.10, 0.12);
        cr->set_line_width(1.5);
        cr->stroke();

        // Cutting edge dot.
        cr->set_source_rgb(0.98, 0.83, 0.10);
        cr->arc(wx(tx), wy(ty), 4.0, 0, 2 * M_PI);
        cr->fill();
    }

    // Bucket pivot.
    cr->set_source_rgb(0.10, 0.10, 0.12);
    cr->arc(wx(bpx), wy(bpy), 4.0, 0, 2 * M_PI);
    cr->fill();
    cr->set_source_rgb(0.85, 0.65, 0.10);
    cr->arc(wx(bpx), wy(bpy), 2.0, 0, 2 * M_PI);
    cr->fill();

    // --- Numeric readout + status word ---
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f cm", cached_height_cm_);

        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(24.0);
        Cairo::TextExtents te2;
        cr->get_text_extents(buf, te2);
        double r, g, b;
        status_color(r, g, b);
        cr->set_source_rgb(r, g, b);
        cr->move_to((W - te2.width) / 2.0 - te2.x_bearing, H - 22.0);
        cr->show_text(buf);

        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_NORMAL);
        cr->set_font_size(12.0);
        const char* s = status_text();
        cr->get_text_extents(s, te2);
        if (light_mode_) cr->set_source_rgb(0.25, 0.25, 0.25);
        else             cr->set_source_rgb(0.80, 0.80, 0.82);
        cr->move_to((W - te2.width) / 2.0 - te2.x_bearing, H - 6.0);
        cr->show_text(s);
    }

    return true;
}
