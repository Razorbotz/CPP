#ifndef BUCKET_HEIGHT_INDICATOR_HPP
#define BUCKET_HEIGHT_INDICATOR_HPP

#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include <string>

/**
 * BucketHeightIndicator
 *
 * Live side-view diagram of the robot, showing the chassis, the arm, and
 * the bucket positioned over a ground line. The bucket's vertical
 * distance from the ground is the primary "user-friendly" cue (you see
 * where it is, not just a number on a bar), with a numeric readout in
 * centimetres reinforcing it.
 *
 * Inputs: arm angle and bucket angle in degrees (same conventions as the
 * existing updateBucketElevationBar()), plus the physical dimensions
 * H / L_arm / L_bucket so the diagram is to scale for whichever bot is
 * active.
 *
 * Replacement for the vertical PositionBar elevation_bar.
 */
class BucketHeightIndicator : public Gtk::DrawingArea {
public:
    explicit BucketHeightIndicator(const std::string& title = "Bucket Height");
    ~BucketHeightIndicator() override = default;

    /**
     * Set the per-bot geometry (centimetres):
     *   H        : height of arm pivot above ground
     *   L_arm    : arm pivot -> bucket pivot
     *   L_bucket : bucket pivot -> cutting edge (tip)
     */
    void set_geometry_cm(double H, double L_arm, double L_bucket);

    /** Update arm + bucket angles (degrees). Triggers a redraw. */
    void set_angles_deg(double arm_deg, double bucket_deg);

    /** Caution / danger thresholds in cm above ground (default 5 / 0). */
    void set_thresholds_cm(double caution_cm, double danger_cm);

    /** Light/dark theme toggle. */
    void set_light_mode(bool light);

    /** Last computed bucket-tip height above ground (cm). */
    double height_cm() const { return cached_height_cm_; }

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

private:
    // Compute tip and pivot positions in world coords (cm, ground at y=0).
    void compute_geometry(double& arm_pivot_x, double& arm_pivot_y,
                          double& bucket_pivot_x, double& bucket_pivot_y,
                          double& tip_x, double& tip_y) const;

    void status_color(double& r, double& g, double& b) const;
    const char* status_text() const;

    std::string title_;
    double H_         = 17.0;   // cm
    double L_arm_     = 68.3;   // cm
    double L_bucket_  = 30.9;   // cm
    double arm_deg_    = 0.0;
    double bucket_deg_ = 0.0;
    double caution_cm_ = 5.0;
    double danger_cm_  = 0.0;
    double cached_height_cm_ = 0.0;
    bool light_mode_ = false;
};

#endif // BUCKET_HEIGHT_INDICATOR_HPP
