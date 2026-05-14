#ifndef BUCKET_TILT_INDICATOR_HPP
#define BUCKET_TILT_INDICATOR_HPP

#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include <string>

/**
 * BucketTiltIndicator
 *
 * Vector-drawn bucket tilt gauge. Replaces the rotated PNG indicator
 * (newbucket.png + rotate_image()) with a self-drawing widget that:
 *   - renders a proper bucket silhouette at the current tilt
 *   - draws a horizon reference line and a graduated arc with tick marks
 *   - colour-grades the warning ring (green -> amber -> red) instead of
 *     a binary white/red flash
 *   - shows a large numeric degree readout below the gauge
 *   - flags the load state (CRADLED / LEVEL / DUMPING) so the operator
 *     can tell at a glance whether material would spill
 *
 * Drop-in replacement for the old Gtk::Image-based indicator. The widget
 * owns its own redraw; the call site just does `indicator->set_angle(deg)`.
 *
 * Default warning thresholds match the talos.urdf Bucket_Joint limits
 * (-0.45 rad -> 2.2 rad, i.e. ~ -26 deg .. 126 deg) with a small margin.
 * Override with set_warning_angles() if you want different bounds.
 */
class BucketTiltIndicator : public Gtk::DrawingArea {
public:
    explicit BucketTiltIndicator(const std::string& title = "Bucket Tilt");
    ~BucketTiltIndicator() override = default;

    /** Set the current bucket tilt in degrees (world-frame). Triggers a redraw. */
    void set_angle(double degrees);

    /** Hard limits (past these the ring goes red). Defaults: -26, 126. */
    void set_warning_angles(double low_deg, double high_deg);

    /** Soft caution margin in degrees inside each hard limit. Default: 15. */
    void set_caution_margin(double margin_deg);

    /** Light/dark theme toggle to match the rest of the GUI. */
    void set_light_mode(bool light);

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

private:
    void draw_arc_ticks(const Cairo::RefPtr<Cairo::Context>& cr,
                        double cx, double cy, double radius) const;
    void draw_bucket(const Cairo::RefPtr<Cairo::Context>& cr,
                     double cx, double cy, double size, double angle_rad) const;
    void draw_readout(const Cairo::RefPtr<Cairo::Context>& cr,
                      double cx, double y, double width) const;

    // Pick a status colour for the current angle (green/amber/red).
    void status_color(double& r, double& g, double& b) const;
    const char* status_text() const;

    std::string title_;
    double angle_deg_ = 0.0;
    double low_deg_   = -26.0;   // matches URDF Bucket_Joint lower limit
    double high_deg_  = 126.0;   // matches URDF Bucket_Joint upper limit
    double caution_margin_deg_ = 15.0;
    bool light_mode_ = false;
};

#endif // BUCKET_TILT_INDICATOR_HPP
