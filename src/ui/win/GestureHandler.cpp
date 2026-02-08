#ifdef PLATFORM_WIN

#include "GestureHandler.h"
#include "DesignSystem.h"
#include "settings_storage.h"
#include "browser_client.h"

#include <windows.h>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <cmath>
#include <algorithm>

// Property name for storing GestureHandler* on the window
static const wchar_t* kGestureHandlerProp = L"OrbFoxGestureHandler";

GestureHandler::GestureHandler() {
    // Use accent color from design system
    trail_line_color_ = DesignSystem::GetAccentColor();
}

GestureHandler::~GestureHandler() {
    Detach();
}

bool GestureHandler::Attach(HWND hwnd) {
    if (!hwnd || hwnd_ != nullptr) {
        return false;
    }

    hwnd_ = hwnd;

    // Store this pointer on the window
    SetPropW(hwnd_, kGestureHandlerProp, reinterpret_cast<HANDLE>(this));

    // Subclass the window to intercept messages
    original_wndproc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(GestureWndProc))
    );

    if (!original_wndproc_) {
        RemovePropW(hwnd_, kGestureHandlerProp);
        hwnd_ = nullptr;
        return false;
    }

    return true;
}

void GestureHandler::Detach() {
    if (hwnd_ && original_wndproc_) {
        // Restore original window procedure
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc_));
        RemovePropW(hwnd_, kGestureHandlerProp);

        // Clean up any active trail
        ClearGestureTrail();
    }

    hwnd_ = nullptr;
    original_wndproc_ = nullptr;
}

LRESULT CALLBACK GestureHandler::GestureWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GestureHandler* handler = reinterpret_cast<GestureHandler*>(
        GetPropW(hwnd, kGestureHandlerProp)
    );

    if (handler) {
        LRESULT result = handler->HandleMessage(msg, wParam, lParam);
        // Non-zero means we handled it
        if (result != 0) {
            return result;
        }
        // Call original proc for unhandled messages
        return CallWindowProcW(handler->original_wndproc_, hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT GestureHandler::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // Don't intercept our own forwarded events
    if (is_forwarding_event_) {
        return 0;  // Let original proc handle it
    }

    // Check both our enabled flag and settings storage
    Settings settings = SettingsStorage::GetInstance().Get();
    if (!enabled_ || !settings.gestures_enabled) {
        return 0;  // Let original proc handle it
    }

    switch (msg) {
        case WM_RBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            StartTracking(pt);
            // Suppress CEF context menu during gesture tracking
            SuppressContextMenu(true);
            return 1;  // Handled
        }

        case WM_MOUSEMOVE: {
            if (is_tracking_) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                UpdateTracking(pt);
                return 1;  // Handled
            }
            break;
        }

        case WM_RBUTTONUP: {
            if (is_tracking_) {
                POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                EndTracking(pt);
                return 1;  // Handled
            }
            break;
        }

        case WM_CAPTURECHANGED: {
            // Lost capture - cancel any active gesture
            if (is_tracking_) {
                CancelTracking();
            }
            break;
        }

        case WM_CONTEXTMENU: {
            // Suppress context menu during gesture tracking or if we just completed a gesture
            if (is_tracking_) {
                return 1;  // Suppress
            }
            break;
        }
    }

    return 0;  // Let original proc handle it
}

void GestureHandler::StartTracking(POINT pt) {
    is_tracking_ = true;
    did_drag_ = false;
    has_moved_down_ = false;
    max_downward_distance_ = 0;

    start_point_ = pt;
    current_point_ = pt;

    trail_points_.clear();
    trail_points_.push_back(pt);

    // Capture the mouse to track movement outside the window
    SetCapture(hwnd_);
}

void GestureHandler::UpdateTracking(POINT pt) {
    if (!is_tracking_) return;

    current_point_ = pt;

    // Calculate total distance moved
    int dx = pt.x - start_point_.x;
    int dy = pt.y - start_point_.y;
    double distance = std::sqrt(static_cast<double>(dx * dx + dy * dy));

    if (distance > drag_threshold_) {
        did_drag_ = true;
    }

    // Track downward movement for L-shape detection
    // In Windows, Y increases downward, so downward = positive dy
    int downward_distance = pt.y - start_point_.y;
    if (downward_distance > max_downward_distance_) {
        max_downward_distance_ = downward_distance;
    }

    if (max_downward_distance_ >= minimum_down_distance_) {
        has_moved_down_ = true;
    }

    // Add point to trail for drawing
    trail_points_.push_back(pt);

    // Draw the gesture trail
    DrawGestureTrail();
}

void GestureHandler::EndTracking(POINT pt) {
    if (!is_tracking_) return;

    is_tracking_ = false;

    // Release mouse capture
    ReleaseCapture();

    // Re-enable context menu
    SuppressContextMenu(false);

    // Clear the visual trail
    ClearGestureTrail();

    // Detect gesture
    GestureType gesture = DetectGesture(start_point_, pt);

    if (gesture != GestureType::None) {
        // Invoke callback
        if (on_gesture_) {
            on_gesture_(gesture);
        }
    } else if (!did_drag_) {
        // No drag occurred - this was just a right-click
        // Forward to browser for context menu
        ForwardRightClick(pt);
    }
    // If drag occurred but no gesture recognized, do nothing (user was just exploring)
}

void GestureHandler::CancelTracking() {
    if (!is_tracking_) return;

    is_tracking_ = false;
    ReleaseCapture();
    SuppressContextMenu(false);
    ClearGestureTrail();
}

GestureType GestureHandler::DetectGesture(POINT start, POINT end) const {
    // Get current gesture settings
    Settings settings = SettingsStorage::GetInstance().Get();

    int dx = end.x - start.x;
    int dy = end.y - start.y;

    int horizontal_distance = std::abs(dx);
    int vertical_distance = std::abs(dy);

    // Check for L-shape gesture: down then right
    // We need: moved down significantly at some point, AND ended up to the right
    if (has_moved_down_ && dx >= minimum_gesture_distance_ && settings.gesture_close_tab_enabled) {
        return GestureType::LShape;
    }

    // Check for reverse L-shape gesture: down then left
    // We need: moved down significantly at some point, AND ended up to the left
    if (has_moved_down_ && dx <= -minimum_gesture_distance_ && settings.gesture_reopen_tab_enabled) {
        return GestureType::ReverseLShape;
    }

    // Check for horizontal gestures (must be primarily horizontal)
    if (horizontal_distance >= static_cast<int>(minimum_gesture_distance_) &&
        horizontal_distance > vertical_distance * 1.5) {
        if (dx < 0 && settings.gesture_back_enabled) {
            return GestureType::Left;  // Dragged left -> Back
        }
        if (dx > 0 && settings.gesture_forward_enabled) {
            return GestureType::Right;  // Dragged right -> Forward
        }
    }

    return GestureType::None;
}

void GestureHandler::DrawGestureTrail() {
    if (trail_points_.size() < 2) return;

    HDC hdc = GetDC(hwnd_);
    if (!hdc) return;

    // Create pen for drawing the trail
    HPEN pen = CreatePen(PS_SOLID, trail_line_width_, trail_line_color_);
    HPEN old_pen = static_cast<HPEN>(SelectObject(hdc, pen));

    // Set drawing mode for visible trail
    int old_mode = SetROP2(hdc, R2_COPYPEN);

    // Draw all trail segments
    MoveToEx(hdc, trail_points_[0].x, trail_points_[0].y, nullptr);
    for (size_t i = 1; i < trail_points_.size(); ++i) {
        LineTo(hdc, trail_points_[i].x, trail_points_[i].y);
    }

    // Draw a small circle at the current point to indicate gesture endpoint
    int radius = trail_line_width_ + 2;
    HBRUSH brush = CreateSolidBrush(trail_line_color_);
    HBRUSH old_brush = static_cast<HBRUSH>(SelectObject(hdc, brush));
    Ellipse(hdc,
            current_point_.x - radius, current_point_.y - radius,
            current_point_.x + radius, current_point_.y + radius);

    // Clean up
    SetROP2(hdc, old_mode);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);

    ReleaseDC(hwnd_, hdc);
}

void GestureHandler::ClearGestureTrail() {
    if (trail_points_.empty()) return;

    // Invalidate the region covered by the trail to trigger repaint
    if (hwnd_ && !trail_points_.empty()) {
        // Calculate bounding rect of trail with some padding
        int min_x = trail_points_[0].x;
        int max_x = trail_points_[0].x;
        int min_y = trail_points_[0].y;
        int max_y = trail_points_[0].y;

        for (const auto& pt : trail_points_) {
            min_x = (std::min)(min_x, static_cast<int>(pt.x));
            max_x = (std::max)(max_x, static_cast<int>(pt.x));
            min_y = (std::min)(min_y, static_cast<int>(pt.y));
            max_y = (std::max)(max_y, static_cast<int>(pt.y));
        }

        // Add padding for line width and endpoint circle
        int padding = trail_line_width_ + 5;
        RECT rect = {
            min_x - padding,
            min_y - padding,
            max_x + padding,
            max_y + padding
        };

        // Request repaint of the trail area
        InvalidateRect(hwnd_, &rect, TRUE);
    }

    trail_points_.clear();
}

void GestureHandler::ForwardRightClick(POINT pt) {
    if (!hwnd_) return;

    // Set flag to prevent our WndProc from intercepting the forwarded messages
    is_forwarding_event_ = true;

    // Convert to screen coordinates for the context menu message
    POINT screen_pt = pt;
    ClientToScreen(hwnd_, &screen_pt);

    // Use SendMessage for synchronous delivery so our flag works correctly
    // This ensures CEF receives the right-click and can show its context menu
    SendMessageW(hwnd_, WM_RBUTTONDOWN, MK_RBUTTON,
                 MAKELPARAM(pt.x, pt.y));

    SendMessageW(hwnd_, WM_RBUTTONUP, 0,
                 MAKELPARAM(pt.x, pt.y));

    // Reset flag after synchronous messages are processed
    is_forwarding_event_ = false;
}

#endif  // PLATFORM_WIN
