#pragma once

#ifdef PLATFORM_WIN

#include <windows.h>
#include <string>

// Design System for OrbFox on Windows
// Mirrors the macOS design system for visual consistency

namespace DesignSystem {

// ============================================================
// Colors (dark theme matching macOS)
// ============================================================

// Background colors
inline COLORREF GetBackgroundColor() { return RGB(26, 26, 26); }         // #1a1a1a
inline COLORREF GetSurfaceColor() { return RGB(38, 38, 38); }            // #262626
inline COLORREF GetSurfaceHoverColor() { return RGB(51, 51, 51); }       // #333333
inline COLORREF GetSurfaceActiveColor() { return RGB(64, 64, 64); }      // #404040

// Text colors
inline COLORREF GetTextPrimaryColor() { return RGB(255, 255, 255); }     // #ffffff
inline COLORREF GetTextSecondaryColor() { return RGB(153, 153, 153); }   // #999999
inline COLORREF GetTextTertiaryColor() { return RGB(102, 102, 102); }    // #666666

// Accent colors
inline COLORREF GetAccentColor() { return RGB(59, 130, 246); }           // #3b82f6 (blue)
inline COLORREF GetAccentHoverColor() { return RGB(96, 165, 250); }      // #60a5fa
inline COLORREF GetAccentActiveColor() { return RGB(37, 99, 235); }      // #2563eb

// Semantic colors
inline COLORREF GetSuccessColor() { return RGB(34, 197, 94); }           // #22c55e
inline COLORREF GetWarningColor() { return RGB(234, 179, 8); }           // #eab308
inline COLORREF GetErrorColor() { return RGB(239, 68, 68); }             // #ef4444

// Border colors
inline COLORREF GetBorderColor() { return RGB(64, 64, 64); }             // #404040
inline COLORREF GetBorderSubtleColor() { return RGB(51, 51, 51); }       // #333333

// Workspace colors (matching macOS palette)
inline COLORREF GetWorkspaceColor(int index) {
    static const COLORREF colors[] = {
        RGB(59, 130, 246),   // Blue
        RGB(168, 85, 247),   // Purple
        RGB(236, 72, 153),   // Pink
        RGB(239, 68, 68),    // Red
        RGB(249, 115, 22),   // Orange
        RGB(234, 179, 8),    // Yellow
        RGB(34, 197, 94),    // Green
        RGB(20, 184, 166),   // Teal
    };
    return colors[index % 8];
}

// ============================================================
// Typography
// ============================================================

inline int GetFontSizeSmall() { return 11; }
inline int GetFontSizeBody() { return 13; }
inline int GetFontSizeLarge() { return 15; }
inline int GetFontSizeTitle() { return 17; }

inline const wchar_t* GetFontFamily() { return L"Segoe UI"; }

// Create a font with the specified size and weight
inline HFONT MakeFont(int size, int weight = FW_NORMAL) {
    return ::CreateFontW(
        -MulDiv(size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, GetFontFamily()
    );
}

// ============================================================
// Spacing (4pt grid, matching macOS)
// ============================================================

inline int GetSpacingXS() { return 4; }
inline int GetSpacingSM() { return 8; }
inline int GetSpacingMD() { return 12; }
inline int GetSpacingLG() { return 16; }
inline int GetSpacingXL() { return 24; }

// ============================================================
// Layout
// ============================================================

inline int GetCornerRadiusSmall() { return 4; }
inline int GetCornerRadiusMedium() { return 6; }
inline int GetCornerRadiusLarge() { return 8; }

inline int GetIconSizeSmall() { return 16; }
inline int GetIconSizeMedium() { return 20; }
inline int GetIconSizeLarge() { return 24; }

inline int GetButtonHeight() { return 32; }
inline int GetInputHeight() { return 36; }

// ============================================================
// Helper functions
// ============================================================

// Create a solid brush from COLORREF
inline HBRUSH CreateBrush(COLORREF color) {
    return CreateSolidBrush(color);
}

// Draw rounded rectangle
inline void DrawRoundedRect(HDC hdc, const RECT& rect, int radius, COLORREF fillColor, COLORREF borderColor = 0) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = borderColor ? CreatePen(PS_SOLID, 1, borderColor) : (HPEN)GetStockObject(NULL_PEN);

    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    if (borderColor) DeleteObject(pen);
}

// Draw text with specified color
inline void DrawTextWithColor(HDC hdc, const std::wstring& text, RECT& rect, COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text.c_str(), -1, &rect, format);
}

// Convert UTF-8 string to wide string
inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

// Convert wide string to UTF-8
inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

}  // namespace DesignSystem

#endif  // PLATFORM_WIN
