#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <functional>
#include <vector>

// Gesture types recognized by the handler (matching macOS GestureContainerView)
enum class GestureType {
    None,
    Left,           // Drag left -> Go back
    Right,          // Drag right -> Go forward
    LShape,         // Down then right -> Close tab
    ReverseLShape,  // Down then left -> Reopen closed tab
};

// Callback for when a gesture is recognized
using GestureCallback = std::function<void(GestureType)>;

// GestureHandler: Tracks right-click + drag gestures on the browser area
// Implements Vivaldi-style mouse gestures for navigation
class GestureHandler {
public:
    GestureHandler();
    ~GestureHandler();

    // Attach to a window to intercept mouse events
    // This subclasses the window to handle right-click gestures
    bool Attach(HWND hwnd);

    // Detach from the window
    void Detach();

    // Enable/disable gesture recognition
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    // Set the callback for gesture events
    void SetGestureCallback(GestureCallback callback) { on_gesture_ = std::move(callback); }

    // Configuration
    void SetMinimumGestureDistance(int distance) { minimum_gesture_distance_ = distance; }
    int GetMinimumGestureDistance() const { return minimum_gesture_distance_; }

    void SetMinimumDownDistance(int distance) { minimum_down_distance_ = distance; }
    int GetMinimumDownDistance() const { return minimum_down_distance_; }

    void SetTrailLineWidth(int width) { trail_line_width_ = width; }
    void SetTrailLineColor(COLORREF color) { trail_line_color_ = color; }

private:
    // Window procedure for subclassed window
    static LRESULT CALLBACK GestureWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Handle mouse messages
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Gesture tracking
    void StartTracking(POINT pt);
    void UpdateTracking(POINT pt);
    void EndTracking(POINT pt);
    void CancelTracking();

    // Gesture detection
    GestureType DetectGesture(POINT start, POINT end) const;

    // Visual feedback
    void DrawGestureTrail();
    void ClearGestureTrail();

    // Forward right-click to the browser (when no gesture detected)
    void ForwardRightClick(POINT pt);

    // Window handle and original proc
    HWND hwnd_ = nullptr;
    WNDPROC original_wndproc_ = nullptr;

    // Gesture state
    bool enabled_ = true;
    bool is_tracking_ = false;
    bool did_drag_ = false;           // True if mouse actually moved during gesture
    bool is_forwarding_event_ = false; // Prevent recursion when forwarding clicks

    // Gesture points
    POINT start_point_ = {0, 0};
    POINT current_point_ = {0, 0};
    std::vector<POINT> trail_points_;  // Points for drawing the trail

    // L-shape detection
    bool has_moved_down_ = false;
    int max_downward_distance_ = 0;

    // Configuration
    int minimum_gesture_distance_ = 50;  // Minimum drag to recognize gesture
    int minimum_down_distance_ = 30;     // Minimum down movement for L-shape
    int drag_threshold_ = 5;             // Movement threshold to consider as drag

    // Visual feedback
    int trail_line_width_ = 3;
    COLORREF trail_line_color_ = RGB(59, 130, 246);  // Accent blue

    // Callback
    GestureCallback on_gesture_;
};

#endif  // PLATFORM_WIN
