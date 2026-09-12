#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace Gdiplus;

namespace {

// Message IDs
constexpr UINT_PTR kFrameTimerId = 1;
constexpr UINT kFrameMs = 16;               // ~60 FPS
constexpr UINT WM_RIPPLE_CLICK = WM_APP + 1;
constexpr UINT WM_TRAYICON     = WM_APP + 2;

// Tray menu IDs
constexpr UINT ID_TRAY_SETTINGS  = 1001;
constexpr UINT ID_TRAY_AUTOSTART = 1002;
constexpr UINT ID_TRAY_EXIT      = 1003;

// Settings control IDs
constexpr int IDC_PRESET_COMBO        = 2001;
constexpr int IDC_BTN_COLOR_LEFT      = 2002;
constexpr int IDC_BTN_COLOR_RIGHT     = 2003;
constexpr int IDC_BTN_COLOR_MID       = 2004;
constexpr int IDC_TRK_RADIUS          = 2005;
constexpr int IDC_LBL_RADIUS          = 2006;
constexpr int IDC_TRK_DURATION        = 2007;
constexpr int IDC_LBL_DURATION        = 2008;
constexpr int IDC_TRK_THICKNESS       = 2009;
constexpr int IDC_LBL_THICKNESS       = 2010;
constexpr int IDC_TRK_RINGS           = 2011;
constexpr int IDC_LBL_RINGS           = 2012;
constexpr int IDC_CHK_CENTERDOT       = 2013;
constexpr int IDC_BTN_SAVE            = 2014;
constexpr int IDC_BTN_RESET           = 2015;
constexpr int IDC_CHK_AMBIENT         = 2016;
constexpr int IDC_TRK_AMBIENT_RADIUS  = 2017;
constexpr int IDC_LBL_AMBIENT_RADIUS  = 2018;
constexpr int IDC_TRK_AMBIENT_ALPHA   = 2019;
constexpr int IDC_LBL_AMBIENT_ALPHA   = 2020;
constexpr int IDC_BTN_COLOR_AMBIENT   = 2021;
constexpr int IDC_TRK_AMBIENT_BAND    = 2022;
constexpr int IDC_LBL_AMBIENT_BAND    = 2023;
constexpr int IDC_TRK_ALPHA           = 2024;
constexpr int IDC_LBL_ALPHA           = 2025;
constexpr int IDC_CHK_TRAIL           = 2026;
constexpr int IDC_BTN_COLOR_TRAIL     = 2027;
constexpr int IDC_TRK_TRAIL_DURATION  = 2028;
constexpr int IDC_LBL_TRAIL_DURATION  = 2029;
constexpr int IDC_TRK_TRAIL_WIDTH     = 2030;
constexpr int IDC_LBL_TRAIL_WIDTH     = 2031;
constexpr int IDC_TRK_TRAIL_ALPHA     = 2032;
constexpr int IDC_LBL_TRAIL_ALPHA     = 2033;

enum StylePreset {
    PRESET_WACOM = 0,
    PRESET_WATER_RIPPLE = 1,
    PRESET_CUSTOM = 2
};

struct AppConfig {
    int stylePreset = PRESET_WACOM;
    COLORREF leftColor = RGB(232, 65, 82);     // Wacom coral red
    COLORREF rightColor = RGB(41, 128, 245);   // Wacom crisp blue
    COLORREF middleColor = RGB(245, 166, 35);  // Warm amber
    int maxRadius = 28;                        // px (Wacom tight radius)
    int durationMs = 300;                      // ms (Fast & snappy)
    int ringThickness = 2;                     // px
    int ringCount = 1;                         // 1: pure Wacom ring, 2-3: ripples
    bool centerDot = true;                     // Subtle center contact flash
    int maxAlpha = 180;                        // Alpha opacity (默认 180 ≈ 70% 透明度，鲜明柔和)

    // Ambient continuous ripple settings (常驻动态纯净线条波纹)
    bool ambientRipple = true;                 // 始终开启
    int ambientRadius = 20;                    // 常驻半径 (默认 20px)
    int ambientThickness = 2;                  // 线条粗细 (默认 2px)
    int ambientBand = 2;                       // 兼容旧 ini
    int ambientAlphaPercent = 30;              // 透明度 0 - 100% (默认 30%)
    int ambientAlpha = 76;                     // 255 * 30% ≈ 76
    COLORREF ambientColor = RGB(145, 145, 145);// 柔和自然灰色 (可自定义颜色)

    // Mouse movement ink ribbon trail settings (鼠标移动水墨流线拖尾)
    bool trailEnabled = true;                  // 默认开启（可随时在设置界面关闭）
    COLORREF trailColor = RGB(41, 128, 245);   // 水墨优雅蓝（支持自定义任意调色）
    int trailDurationMs = 380;                 // 留存时长 150 - 800ms
    int trailWidth = 8;                        // 笔触最大粗细 3 - 18px
    int trailAlphaPercent = 80;                // 透明度 10% - 100%
    int trailAlpha = 204;                      // 255 * 80% ≈ 204
};

AppConfig g_config;
COLORREF g_customColors[16] = {0};

struct Ripple {
    POINT screenPt{};
    int buttonType = 0; // 0=Left, 1=Right, 2=Middle
    ULONGLONG startedAt = 0;
};

struct TrailPoint {
    POINT screenPt{};
    ULONGLONG timeMs = 0;
};

HINSTANCE g_instance = nullptr;
HWND g_overlay = nullptr;
HWND g_settingsWnd = nullptr;
HHOOK g_mouseHook = nullptr;
ULONG_PTR g_gdiplusToken = 0;
HFONT g_uiFont = nullptr;
std::vector<Ripple> g_ripples;
std::vector<TrailPoint> g_trailPoints;
int g_virtualX = 0;
int g_virtualY = 0;
int g_virtualW = 0;
int g_virtualH = 0;

// Utility functions
float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

BYTE alphaByte(float a) {
    a = (std::max)(0.0f, (std::min)(255.0f, a));
    return static_cast<BYTE>(a + 0.5f);
}

std::wstring getIniPath() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        return std::wstring(path) + L"MouseRipple.ini";
    }
    return L"MouseRipple.ini";
}

void loadConfig() {
    std::wstring ini = getIniPath();
    g_config.stylePreset = GetPrivateProfileIntW(L"Config", L"StylePreset", PRESET_WACOM, ini.c_str());
    g_config.leftColor = (COLORREF)GetPrivateProfileIntW(L"Config", L"LeftColor", RGB(232, 65, 82), ini.c_str());
    g_config.rightColor = (COLORREF)GetPrivateProfileIntW(L"Config", L"RightColor", RGB(41, 128, 245), ini.c_str());
    g_config.middleColor = (COLORREF)GetPrivateProfileIntW(L"Config", L"MiddleColor", RGB(245, 166, 35), ini.c_str());
    g_config.maxRadius = GetPrivateProfileIntW(L"Config", L"MaxRadius", 28, ini.c_str());
    g_config.durationMs = GetPrivateProfileIntW(L"Config", L"DurationMs", 300, ini.c_str());
    g_config.ringThickness = GetPrivateProfileIntW(L"Config", L"RingThickness", 2, ini.c_str());
    g_config.ringCount = GetPrivateProfileIntW(L"Config", L"RingCount", 1, ini.c_str());
    g_config.centerDot = GetPrivateProfileIntW(L"Config", L"CenterDot", 1, ini.c_str()) != 0;
    g_config.maxAlpha = GetPrivateProfileIntW(L"Config", L"MaxAlpha", 180, ini.c_str());

    // Ambient ripple configuration (纯净线条圈, 0-100% 透明度)
    g_config.ambientRipple = GetPrivateProfileIntW(L"Config", L"AmbientRipple", 1, ini.c_str()) != 0;
    g_config.ambientRadius = GetPrivateProfileIntW(L"Config", L"AmbientRadius", 20, ini.c_str());
    g_config.ambientThickness = GetPrivateProfileIntW(L"Config", L"AmbientThickness", 0, ini.c_str());
    if (g_config.ambientThickness <= 0) {
        g_config.ambientThickness = (std::max)(1, (std::min)(6, (int)GetPrivateProfileIntW(L"Config", L"AmbientBand", 2, ini.c_str())));
    }
    g_config.ambientBand = g_config.ambientThickness;
    g_config.ambientAlphaPercent = (std::max)(0, (std::min)(100, (int)GetPrivateProfileIntW(L"Config", L"AmbientAlphaPercent", 30, ini.c_str())));
    g_config.ambientAlpha = static_cast<int>(255.0f * (g_config.ambientAlphaPercent / 100.0f));
    g_config.ambientColor = (COLORREF)GetPrivateProfileIntW(L"Config", L"AmbientColor", RGB(145, 145, 145), ini.c_str());

    // Ink Trail configuration (水墨流线拖尾)
    g_config.trailEnabled = GetPrivateProfileIntW(L"Config", L"TrailEnabled", 1, ini.c_str()) != 0;
    g_config.trailColor = (COLORREF)GetPrivateProfileIntW(L"Config", L"TrailColor", RGB(41, 128, 245), ini.c_str());
    g_config.trailDurationMs = GetPrivateProfileIntW(L"Config", L"TrailDurationMs", 380, ini.c_str());
    g_config.trailWidth = GetPrivateProfileIntW(L"Config", L"TrailWidth", 8, ini.c_str());
    g_config.trailAlphaPercent = (std::max)(10, (std::min)(100, (int)GetPrivateProfileIntW(L"Config", L"TrailAlphaPercent", 80, ini.c_str())));
    g_config.trailAlpha = static_cast<int>(255.0f * (g_config.trailAlphaPercent / 100.0f));
}

void saveConfig() {
    std::wstring ini = getIniPath();
    WritePrivateProfileStringW(L"Config", L"StylePreset", std::to_wstring(g_config.stylePreset).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"LeftColor", std::to_wstring(g_config.leftColor).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"RightColor", std::to_wstring(g_config.rightColor).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"MiddleColor", std::to_wstring(g_config.middleColor).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"MaxRadius", std::to_wstring(g_config.maxRadius).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"DurationMs", std::to_wstring(g_config.durationMs).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"RingThickness", std::to_wstring(g_config.ringThickness).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"RingCount", std::to_wstring(g_config.ringCount).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"CenterDot", g_config.centerDot ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Config", L"MaxAlpha", std::to_wstring(g_config.maxAlpha).c_str(), ini.c_str());

    // Ambient ripple save
    WritePrivateProfileStringW(L"Config", L"AmbientRipple", g_config.ambientRipple ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Config", L"AmbientRadius", std::to_wstring(g_config.ambientRadius).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"AmbientThickness", std::to_wstring(g_config.ambientThickness).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"AmbientBand", std::to_wstring(g_config.ambientThickness).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"AmbientAlphaPercent", std::to_wstring(g_config.ambientAlphaPercent).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"AmbientColor", std::to_wstring(g_config.ambientColor).c_str(), ini.c_str());

    // Ink Trail save
    WritePrivateProfileStringW(L"Config", L"TrailEnabled", g_config.trailEnabled ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Config", L"TrailColor", std::to_wstring(g_config.trailColor).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"TrailDurationMs", std::to_wstring(g_config.trailDurationMs).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"TrailWidth", std::to_wstring(g_config.trailWidth).c_str(), ini.c_str());
    WritePrivateProfileStringW(L"Config", L"TrailAlphaPercent", std::to_wstring(g_config.trailAlphaPercent).c_str(), ini.c_str());
}

void applyPreset(int preset) {
    g_config.stylePreset = preset;
    if (preset == PRESET_WACOM) {
        g_config.maxRadius = 28;
        g_config.durationMs = 300;
        g_config.ringThickness = 2;
        g_config.ringCount = 1;
        g_config.centerDot = true;
        g_config.maxAlpha = 180;
        g_config.ambientRipple = true;
        g_config.ambientRadius = 20;
        g_config.ambientThickness = 2;
        g_config.ambientBand = 2;
        g_config.ambientAlphaPercent = 30;
        g_config.ambientAlpha = 76;
        g_config.ambientColor = RGB(145, 145, 145);
        g_config.trailEnabled = true;
        g_config.trailColor = RGB(41, 128, 245);
        g_config.trailDurationMs = 380;
        g_config.trailWidth = 8;
        g_config.trailAlphaPercent = 80;
        g_config.trailAlpha = 204;
    } else if (preset == PRESET_WATER_RIPPLE) {
        g_config.maxRadius = 52;
        g_config.durationMs = 540;
        g_config.ringThickness = 3;
        g_config.ringCount = 3;
        g_config.centerDot = true;
        g_config.maxAlpha = 190;
        g_config.trailEnabled = true;
        g_config.trailColor = RGB(0, 160, 233);
        g_config.trailDurationMs = 450;
        g_config.trailWidth = 10;
        g_config.trailAlphaPercent = 85;
        g_config.trailAlpha = 216;
    }
}

// Registry Autostart Helper
constexpr const wchar_t* kAutoStartKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kAutoStartValue = L"MouseRipple";

bool isAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAutoStartKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegQueryValueExW(hKey, kAutoStartValue, nullptr, &type, nullptr, &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS && size > 0);
    }
    return false;
}

void setAutoStart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAutoStartKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH] = {0};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring quoted = L"\"" + std::wstring(exePath) + L"\" --autostart";
            RegSetValueExW(hKey, kAutoStartValue, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, kAutoStartValue);
        }
        RegCloseKey(hKey);
    }
}

void updateVirtualDesktopMetrics() {
    g_virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_virtualW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_virtualH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

// -------------------------------------------------------------
// Soft Toroidal Wave Profile (基于余弦窗采样插值的超柔和水波，晶莹通透，零硬边，零摩尔纹)
// -------------------------------------------------------------
void drawSoftToroidalWave(Graphics& g, float cx, float cy, float rCrest, float halfBand,
                          float peakAlpha, BYTE r, BYTE gg, BYTE b) {
    if (rCrest <= 0.5f || peakAlpha <= 1.0f || halfBand <= 1.0f) return;

    const float rOuter = rCrest + halfBand;
    const float rInner = (std::max)(0.0f, rCrest - halfBand);
    if (rOuter <= 2.0f) return;

    GraphicsPath path;
    path.AddEllipse(cx - rOuter, cy - rOuter, rOuter * 2.0f, rOuter * 2.0f);
    PathGradientBrush pgb(&path);
    pgb.SetCenterPoint(PointF(cx, cy));

    float posCrest = clamp01(1.0f - (rCrest / rOuter));
    float posInner = clamp01(1.0f - (rInner / rOuter));

    constexpr int kStops = 11;
    REAL positions[kStops];
    Color colors[kStops];

    // 从外边缘 pos=0 (r=rOuter, 透明度=0) 到 波峰 posCrest (r=rCrest, 透明度=peakAlpha)
    for (int i = 0; i <= 5; i++) {
        float t = static_cast<float>(i) / 5.0f;
        positions[i] = posCrest * t;
        float factor = 0.5f * (1.0f - std::cos(3.14159265f * t));
        colors[i] = Color(alphaByte(peakAlpha * factor), r, gg, b);
    }
    // 从波峰 posCrest 到 内边缘 posInner (r=rInner, 透明度=0)
    for (int i = 1; i <= 4; i++) {
        float t = static_cast<float>(i) / 4.0f;
        positions[5 + i] = posCrest + (posInner - posCrest) * t;
        float factor = 0.5f * (1.0f + std::cos(3.14159265f * t));
        colors[5 + i] = Color(alphaByte(peakAlpha * factor), r, gg, b);
    }
    // 内边缘到中心 (全透明)
    positions[10] = 1.0f;
    colors[10] = Color(0, r, gg, b);

    // 单调性保证
    for (int i = 1; i < kStops; ++i) {
        if (positions[i] <= positions[i - 1]) positions[i] = positions[i - 1] + 0.0001f;
    }
    if (positions[kStops - 1] > 1.0f) positions[kStops - 1] = 1.0f;

    pgb.SetInterpolationColors(colors, positions, kStops);
    g.FillEllipse(&pgb, cx - rOuter, cy - rOuter, rOuter * 2.0f, rOuter * 2.0f);
}

// -------------------------------------------------------------
// Calligraphic Ink Ribbon Trail (水墨流线拖尾：单体闭合曲面流，彻底杜绝阶梯/串珠/断折)
// -------------------------------------------------------------
void drawSmoothInkRibbon(Graphics& g, const std::vector<TrailPoint>& points, float totalDurationMs,
                         float baseWidth, int baseAlpha, COLORREF color, int vx, int vy, ULONGLONG now) {
    const int n = static_cast<int>(points.size());
    if (n < 2) return;

    const BYTE r = GetRValue(color);
    const BYTE gg = GetGValue(color);
    const BYTE b = GetBValue(color);

    // 1. 坐标转换与点位存活度计算
    struct PtT { float x, y; float life; };
    std::vector<PtT> rawPts(n);
    for (int i = 0; i < n; ++i) {
        rawPts[i].x = static_cast<float>(points[i].screenPt.x - vx);
        rawPts[i].y = static_cast<float>(points[i].screenPt.y - vy);
        float elapsed = static_cast<float>(now > points[i].timeMs ? now - points[i].timeMs : 0);
        rawPts[i].life = clamp01(1.0f - elapsed / totalDurationMs);
    }

    // 2. 向心样条曲线插值（自适应步长 + 端点自然切线外推，杜绝快速划动末端僵直折线）
    std::vector<PtT> spline;
    spline.reserve(n * 40);

    for (int i = 0; i < n - 1; ++i) {
        PtT p0, p1, p2, p3;
        p1 = rawPts[i];
        p2 = rawPts[i + 1];

        // 端点自然外推，避免一头一尾切线被强制锁定为直弦
        if (i == 0) {
            p0.x = p1.x - (p2.x - p1.x);
            p0.y = p1.y - (p2.y - p1.y);
            p0.life = clamp01(p1.life + (p1.life - p2.life));
        } else {
            p0 = rawPts[i - 1];
        }

        if (i + 2 < n) {
            p3 = rawPts[i + 2];
        } else {
            p3.x = p2.x + (p2.x - p1.x);
            p3.y = p2.y + (p2.y - p1.y);
            p3.life = clamp01(p2.life - (p1.life - p2.life));
        }

        const float dist = std::hypot(p2.x - p1.x, p2.y - p1.y);
        const int steps = (std::max)(4, (std::min)(50, static_cast<int>(dist / 1.5f)));

        for (int s = 0; s < steps; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(steps);
            const float u2 = u * u;
            const float u3 = u2 * u;

            const float x = 0.5f * ((2.0f * p1.x) +
                                  (-p0.x + p2.x) * u +
                                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * u2 +
                                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * u3);

            const float y = 0.5f * ((2.0f * p1.y) +
                                  (-p0.y + p2.y) * u +
                                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * u2 +
                                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * u3);

            const float life = p1.life + (p2.life - p1.life) * u;
            spline.push_back({x, y, clamp01(life)});
        }
    }
    spline.push_back(rawPts.back());

    const int m = static_cast<int>(spline.size());
    if (m < 2) return;

    // 3. 计算连续无阶梯法向轮廓左右边界（宽度纯数学无级衰减，完全消灭阶梯跳变与毛发细尾）
    std::vector<PointF> leftEdge(m);
    std::vector<PointF> rightEdge(m);

    for (int i = 0; i < m; ++i) {
        float dx = 0.0f, dy = 0.0f;
        if (i == 0) {
            dx = spline[1].x - spline[0].x;
            dy = spline[1].y - spline[0].y;
        } else if (i == m - 1) {
            dx = spline[m - 1].x - spline[m - 2].x;
            dy = spline[m - 1].y - spline[m - 2].y;
        } else {
            dx = spline[i + 1].x - spline[i - 1].x;
            dy = spline[i + 1].y - spline[i - 1].y;
        }

        const float len = std::hypot(dx, dy);
        float nx = 0.0f, ny = 0.0f;
        if (len > 0.001f) {
            nx = -dy / len;
            ny = dx / len;
        }

        // 半线宽随存活度无级衰减：尾部收于 0.3px 锋尖，头部丰满
        float hw = (baseWidth * 0.5f) * std::pow(spline[i].life, 0.70f);
        if (hw < 0.3f) hw = 0.3f;

        leftEdge[i]  = PointF(spline[i].x + nx * hw, spline[i].y + ny * hw);
        rightEdge[i] = PointF(spline[i].x - nx * hw, spline[i].y - ny * hw);
    }

    // 4. 构建单体闭合曲面图形路径（FillModeWinding，自相交时平滑实体融合，无孔洞）
    GraphicsPath path;
    path.SetFillMode(FillModeWinding);

    // 左边缘从尾到头
    path.AddLines(leftEdge.data(), m);

    // 头部圆润弧线接合（笔尖）
    const auto& headPt = spline.back();
    const float headR = (baseWidth * 0.5f) * std::pow(headPt.life, 0.70f);
    if (headR > 0.5f) {
        float angleL = std::atan2(leftEdge[m - 1].Y - headPt.y, leftEdge[m - 1].X - headPt.x) * 180.0f / 3.14159265f;
        path.AddArc(headPt.x - headR, headPt.y - headR, headR * 2.0f, headR * 2.0f, angleL, -180.0f);
    }

    // 右边缘从头回尾
    std::vector<PointF> revRight(m);
    for (int i = 0; i < m; ++i) revRight[i] = rightEdge[m - 1 - i];
    path.AddLines(revRight.data(), m);

    // 闭合路径至尾端锋尖
    path.CloseFigure();

    // 5. 单次渲染光栅化（零阶梯跳变、零内部端帽重叠、零串珠、浑然一体）
    const float effectiveAlpha = static_cast<float>(baseAlpha) * std::pow(headPt.life, 0.35f);
    SolidBrush brush(Color(alphaByte(effectiveAlpha), r, gg, b));
    g.FillPath(&brush, &path);

    Pen contourPen(Color(alphaByte(effectiveAlpha), r, gg, b), 1.0f);
    g.DrawPath(&contourPen, &path);
}

// -------------------------------------------------------------
// Ambient Continuous Ripple Animation (常驻单圈纯净线条呼吸圈，无渐变过度)
// -------------------------------------------------------------
void drawAmbientRipple(Graphics& g, ULONGLONG now) {
    if (!g_config.ambientRipple || g_config.ambientAlpha <= 0) return;

    POINT pt{};
    if (!GetCursorPos(&pt)) return;

    const float cx = static_cast<float>(pt.x - g_virtualX);
    const float cy = static_cast<float>(pt.y - g_virtualY);

    const float maxR = static_cast<float>((std::max)(10, g_config.ambientRadius));
    const float maxAlpha = static_cast<float>(g_config.ambientAlpha); // 0 ~ 255 (0% - 100%)

    const BYTE r = GetRValue(g_config.ambientColor);
    const BYTE gg = GetGValue(g_config.ambientColor);
    const BYTE b = GetBValue(g_config.ambientColor);

    // 单圈从容呼吸循环 (1.8s，单圈舒缓，绝不密集喧闹)
    constexpr ULONGLONG kCycleMs = 1800;
    const float t = static_cast<float>(now % kCycleMs) / static_cast<float>(kCycleMs);

    // 舒缓缓动扩张
    const float e = 1.0f - std::pow(1.0f - t, 2.2f);
    const float startR = 3.5f;
    const float curR = startR + (maxR - startR) * e;

    // 钟形透明度淡入淡出 (前 18% 柔和淡入，随后舒缓淡出)
    float a = 0.0f;
    if (t < 0.18f) {
        a = maxAlpha * (t / 0.18f);
    } else {
        a = maxAlpha * std::pow(1.0f - (t - 0.18f) / 0.82f, 1.4f);
    }

    if (a > 1.0f) {
        const float thickness = static_cast<float>((std::max)(1, (std::min)(6, g_config.ambientThickness)));
        if (thickness <= 1.5f) {
            Pen pen(Color(alphaByte(a), r, gg, b), thickness);
            pen.SetAlignment(PenAlignmentCenter);
            g.DrawEllipse(&pen, cx - curR, cy - curR, curR * 2.0f, curR * 2.0f);
        } else {
            // 呼吸圈柔边羽化：核心圈 + 柔和晕光
            Pen penCore(Color(alphaByte(a * 0.75f), r, gg, b), thickness * 0.75f);
            penCore.SetAlignment(PenAlignmentCenter);
            g.DrawEllipse(&penCore, cx - curR, cy - curR, curR * 2.0f, curR * 2.0f);

            Pen penGlow(Color(alphaByte(a * 0.35f), r, gg, b), thickness * 1.5f);
            penGlow.SetAlignment(PenAlignmentCenter);
            g.DrawEllipse(&penGlow, cx - curR, cy - curR, curR * 2.0f, curR * 2.0f);
        }
    }
}

// -------------------------------------------------------------
// Click Ripple Drawing Animation (Wacom & Water Ripple with Gradients)
// -------------------------------------------------------------
void drawRipple(Graphics& g, const Ripple& ripple, ULONGLONG now) {
    const float duration = static_cast<float>((std::max)(100, g_config.durationMs));
    const float elapsed = static_cast<float>(now - ripple.startedAt);
    const float t = clamp01(elapsed / duration);

    COLORREF colorRef = g_config.leftColor;
    if (ripple.buttonType == 1) {
        colorRef = g_config.rightColor;
    } else if (ripple.buttonType == 2) {
        colorRef = g_config.middleColor;
    }

    const BYTE r = GetRValue(colorRef);
    const BYTE gg = GetGValue(colorRef);
    const BYTE b = GetBValue(colorRef);

    const float cx = static_cast<float>(ripple.screenPt.x - g_virtualX);
    const float cy = static_cast<float>(ripple.screenPt.y - g_virtualY);

    const float maxR = static_cast<float>(g_config.maxRadius);
    const float baseThickness = static_cast<float>(g_config.ringThickness);
    const float maxAlpha = static_cast<float>(g_config.maxAlpha);

    // 1. Center tactile micro-dot (soft radial gradient flash)
    if (g_config.centerDot && t < 0.22f) {
        const float q = t / 0.22f;
        const float dotR = 2.0f + 4.0f * (1.0f - std::pow(1.0f - q, 2.0f));
        const float dotA = (maxAlpha * 0.50f) * std::pow(1.0f - q, 1.5f);
        GraphicsPath dotPath;
        dotPath.AddEllipse(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
        PathGradientBrush dotBrush(&dotPath);
        dotBrush.SetCenterPoint(PointF(cx, cy));
        dotBrush.SetCenterColor(Color(alphaByte(dotA), r, gg, b));
        Color outCol(0, r, gg, b);
        int cnt = 1;
        dotBrush.SetSurroundColors(&outCol, &cnt);
        g.FillEllipse(&dotBrush, cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // 2. Wave 1: Primary wide water ripple (Toroidal soft wave)
    const float e1 = 1.0f - std::pow(1.0f - t, 2.8f);
    const float startR = (std::min)(5.0f, maxR * 0.18f);
    const float r1 = startR + (maxR - startR) * e1;
    const float a1 = maxAlpha * std::pow(1.0f - t, 1.25f);
    const float band1 = (std::max)(8.0f, baseThickness * 3.2f + 12.0f * t);

    drawSoftToroidalWave(g, cx, cy, r1, band1, a1, r, gg, b);

    // 3. Multi-ring harmonic waves
    if (g_config.ringCount >= 2 && t > 0.14f) {
        const float q2 = clamp01((t - 0.14f) / 0.86f);
        const float e2 = 1.0f - std::pow(1.0f - q2, 2.6f);
        const float r2 = startR + (maxR * 0.72f - startR) * e2;
        const float a2 = (maxAlpha * 0.55f) * std::pow(1.0f - q2, 1.4f);
        const float band2 = (std::max)(7.0f, band1 * 0.85f);
        drawSoftToroidalWave(g, cx, cy, r2, band2, a2, r, gg, b);
    }

    if (g_config.ringCount >= 3 && t > 0.28f) {
        const float q3 = clamp01((t - 0.28f) / 0.72f);
        const float e3 = 1.0f - std::pow(1.0f - q3, 2.2f);
        const float r3 = startR + (maxR * 0.48f - startR) * e3;
        const float a3 = (maxAlpha * 0.35f) * std::pow(1.0f - q3, 1.5f);
        const float band3 = (std::max)(6.0f, band1 * 0.70f);
        drawSoftToroidalWave(g, cx, cy, r3, band3, a3, r, gg, b);
    }
}

void renderOverlay(HWND hwnd) {
    updateVirtualDesktopMetrics();
    if (g_virtualW <= 0 || g_virtualH <= 0) return;

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_virtualW;
    bmi.bmiHeader.biHeight = -g_virtualH; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memDC, dib);
    if (bits) {
        ZeroMemory(bits, static_cast<SIZE_T>(g_virtualW) * static_cast<SIZE_T>(g_virtualH) * 4u);
    }

    {
        Graphics graphics(memDC);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        graphics.SetCompositingMode(CompositingModeSourceOver);

        const ULONGLONG now = GetTickCount64();

        // 1. Draw ink ribbon trail if enabled
        if (g_config.trailEnabled && g_trailPoints.size() >= 2) {
            drawSmoothInkRibbon(graphics, g_trailPoints,
                                static_cast<float>(g_config.trailDurationMs),
                                static_cast<float>(g_config.trailWidth),
                                g_config.trailAlpha,
                                g_config.trailColor,
                                g_virtualX, g_virtualY, now);
        }

        // 2. Draw ambient continuous gradient water ripple if enabled
        if (g_config.ambientRipple) {
            drawAmbientRipple(graphics, now);
        }

        // 3. Draw click ripples
        for (const auto& ripple : g_ripples) {
            drawRipple(graphics, ripple, now);
        }
    }

    POINT dst{g_virtualX, g_virtualY};
    SIZE size{g_virtualW, g_virtualH};
    POINT src{0, 0};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, &dst, &size, memDC, &src, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBitmap);
    DeleteObject(dib);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void addRipple(POINT screenPt, int buttonType) {
    Ripple ripple;
    ripple.screenPt = screenPt;
    ripple.buttonType = buttonType;
    ripple.startedAt = GetTickCount64();
    g_ripples.push_back(ripple);

    if (g_ripples.size() > 64) {
        g_ripples.erase(g_ripples.begin(), g_ripples.begin() + (g_ripples.size() - 64));
    }
}

LRESULT CALLBACK lowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_overlay) {
        if (wParam == WM_MOUSEMOVE) {
            if (g_config.trailEnabled) {
                const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
                const ULONGLONG now = GetTickCount64();
                if (g_trailPoints.empty()) {
                    g_trailPoints.push_back({info->pt, now});
                } else {
                    const auto& last = g_trailPoints.back();
                    const int dx = info->pt.x - last.screenPt.x;
                    const int dy = info->pt.y - last.screenPt.y;
                    if (dx * dx + dy * dy >= 4) { // 过滤微颤，至少位移 2px
                        g_trailPoints.push_back({info->pt, now});
                        if (g_trailPoints.size() > 250) {
                            g_trailPoints.erase(g_trailPoints.begin());
                        }
                    }
                }
            }
        } else {
            int btn = -1;
            if (wParam == WM_LBUTTONDOWN) btn = 0;
            else if (wParam == WM_RBUTTONDOWN) btn = 1;
            else if (wParam == WM_MBUTTONDOWN) btn = 2;

            if (btn >= 0) {
                const auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
                POINT* p = new POINT(info->pt);
                PostMessage(g_overlay, WM_RIPPLE_CLICK, static_cast<WPARAM>(btn), reinterpret_cast<LPARAM>(p));
            }
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

void addTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpynW(nid.szTip, L"MouseRipple - 鼠标水波纹 (双击打开设置)", ARRAYSIZE(nid.szTip));
    lstrcpynW(nid.szInfoTitle, L"MouseRipple 水波纹已运行", ARRAYSIZE(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, L"鼠标水波纹已生效。双击任务栏托盘图标可调整设置。", ARRAYSIZE(nid.szInfo));
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void removeTrayIcon(HWND hwnd) {
    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void openSettingsWindow();

void showTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenu(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置 (Settings)...");
    SetMenuDefaultItem(menu, ID_TRAY_SETTINGS, FALSE);
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    const UINT autostartFlag = isAutoStartEnabled() ? (MF_STRING | MF_CHECKED) : (MF_STRING | MF_UNCHECKED);
    AppendMenu(menu, autostartFlag, ID_TRAY_AUTOSTART, L"开机自启动 (Start with Windows)");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, ID_TRAY_EXIT, L"退出 (Exit)");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

// -------------------------------------------------------------
// Settings Window Implementation
// -------------------------------------------------------------

void updateSettingsLabels(HWND hwnd) {
    std::wostringstream ssR;
    ssR << L"最大半径: " << g_config.maxRadius << L" px";
    if (g_config.maxRadius <= 32) ssR << L" (Wacom细腻)";
    SetDlgItemTextW(hwnd, IDC_LBL_RADIUS, ssR.str().c_str());

    std::wostringstream ssD;
    ssD << L"动画时长: " << g_config.durationMs << L" ms";
    if (g_config.durationMs <= 320) ssD << L" (灵敏敏捷)";
    SetDlgItemTextW(hwnd, IDC_LBL_DURATION, ssD.str().c_str());

    std::wostringstream ssT;
    ssT << L"线条粗细: " << g_config.ringThickness << L" px";
    SetDlgItemTextW(hwnd, IDC_LBL_THICKNESS, ssT.str().c_str());

    std::wostringstream ssC;
    ssC << L"波纹圈数: " << g_config.ringCount << L" 圈";
    if (g_config.ringCount == 1) ssC << L" (纯净单圈)";
    else if (g_config.ringCount >= 3) ssC << L" (水波涟漪)";
    SetDlgItemTextW(hwnd, IDC_LBL_RINGS, ssC.str().c_str());

    std::wostringstream ssA;
    int alphaPercent = static_cast<int>(g_config.maxAlpha * 100.0f / 255.0f + 0.5f);
    ssA << L"波纹透明度: " << alphaPercent << L" %";
    if (alphaPercent <= 30) ssA << L" (清淡半透)";
    else if (alphaPercent >= 85) ssA << L" (鲜明醒目)";
    SetDlgItemTextW(hwnd, IDC_LBL_ALPHA, ssA.str().c_str());

    // Ambient ripple labels
    std::wostringstream ssAR;
    ssAR << L"常驻线条半径: " << g_config.ambientRadius << L" px";
    SetDlgItemTextW(hwnd, IDC_LBL_AMBIENT_RADIUS, ssAR.str().c_str());

    std::wostringstream ssAA;
    ssAA << L"常驻透明度: " << g_config.ambientAlphaPercent << L" %";
    if (g_config.ambientAlphaPercent == 0) ssAA << L" (0% 完全隐藏)";
    else if (g_config.ambientAlphaPercent <= 30) ssAA << L" (清爽低调)";
    else if (g_config.ambientAlphaPercent >= 90) ssAA << L" (极度醒目)";
    SetDlgItemTextW(hwnd, IDC_LBL_AMBIENT_ALPHA, ssAA.str().c_str());

    std::wostringstream ssAB;
    ssAB << L"常驻线条粗细: " << g_config.ambientThickness << L" px";
    if (g_config.ambientThickness == 1) ssAB << L" (极细简约)";
    else if (g_config.ambientThickness == 2) ssAB << L" (细腻推荐)";
    SetDlgItemTextW(hwnd, IDC_LBL_AMBIENT_BAND, ssAB.str().c_str());

    // Ink ribbon trail labels (书法水墨流线拖尾)
    std::wostringstream ssTD;
    ssTD << L"拖尾留存: " << g_config.trailDurationMs << L" ms";
    if (g_config.trailDurationMs <= 250) ssTD << L" (短促灵敏)";
    else if (g_config.trailDurationMs >= 500) ssTD << L" (行云流水)";
    SetDlgItemTextW(hwnd, IDC_LBL_TRAIL_DURATION, ssTD.str().c_str());

    std::wostringstream ssTW;
    ssTW << L"拖尾粗细: " << g_config.trailWidth << L" px";
    if (g_config.trailWidth <= 5) ssTW << L" (纤细柔和)";
    else if (g_config.trailWidth >= 12) ssTW << L" (浓墨粗犷)";
    SetDlgItemTextW(hwnd, IDC_LBL_TRAIL_WIDTH, ssTW.str().c_str());

    std::wostringstream ssTA;
    ssTA << L"拖尾透明度: " << g_config.trailAlphaPercent << L" %";
    if (g_config.trailAlphaPercent <= 40) ssTA << L" (淡雅水墨)";
    else if (g_config.trailAlphaPercent >= 80) ssTA << L" (饱满醒目)";
    SetDlgItemTextW(hwnd, IDC_LBL_TRAIL_ALPHA, ssTA.str().c_str());
}

void syncSettingsControls(HWND hwnd) {
    SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, g_config.stylePreset, 0);

    SendDlgItemMessageW(hwnd, IDC_TRK_RADIUS, TBM_SETPOS, TRUE, g_config.maxRadius);
    SendDlgItemMessageW(hwnd, IDC_TRK_DURATION, TBM_SETPOS, TRUE, g_config.durationMs);
    SendDlgItemMessageW(hwnd, IDC_TRK_THICKNESS, TBM_SETPOS, TRUE, g_config.ringThickness);
    SendDlgItemMessageW(hwnd, IDC_TRK_RINGS, TBM_SETPOS, TRUE, g_config.ringCount);

    int alphaPercent = static_cast<int>(g_config.maxAlpha * 100.0f / 255.0f + 0.5f);
    SendDlgItemMessageW(hwnd, IDC_TRK_ALPHA, TBM_SETPOS, TRUE, alphaPercent);

    CheckDlgButton(hwnd, IDC_CHK_CENTERDOT, g_config.centerDot ? BST_CHECKED : BST_UNCHECKED);

    // Ambient ripple controls sync
    CheckDlgButton(hwnd, IDC_CHK_AMBIENT, g_config.ambientRipple ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(hwnd, IDC_TRK_AMBIENT_RADIUS, TBM_SETPOS, TRUE, g_config.ambientRadius);
    SendDlgItemMessageW(hwnd, IDC_TRK_AMBIENT_ALPHA, TBM_SETPOS, TRUE, g_config.ambientAlphaPercent);
    SendDlgItemMessageW(hwnd, IDC_TRK_AMBIENT_BAND, TBM_SETPOS, TRUE, g_config.ambientThickness);

    // Trail controls sync
    CheckDlgButton(hwnd, IDC_CHK_TRAIL, g_config.trailEnabled ? BST_CHECKED : BST_UNCHECKED);
    SendDlgItemMessageW(hwnd, IDC_TRK_TRAIL_DURATION, TBM_SETPOS, TRUE, g_config.trailDurationMs);
    SendDlgItemMessageW(hwnd, IDC_TRK_TRAIL_WIDTH, TBM_SETPOS, TRUE, g_config.trailWidth);
    SendDlgItemMessageW(hwnd, IDC_TRK_TRAIL_ALPHA, TBM_SETPOS, TRUE, g_config.trailAlphaPercent);

    updateSettingsLabels(hwnd);

    // Invalidate color preview buttons
    InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_LEFT), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_RIGHT), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_MID), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_AMBIENT), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_TRAIL), nullptr, TRUE);
}

bool pickColor(HWND owner, COLORREF& targetColor) {
    CHOOSECOLOR cc{};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.rgbResult = targetColor;
    cc.lpCustColors = g_customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&cc)) {
        targetColor = cc.rgbResult;
        return true;
    }
    return false;
}

LRESULT CALLBACK settingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        NONCLIENTMETRICS ncm{};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_uiFont = CreateFontIndirect(&ncm.lfMessageFont);
        if (!g_uiFont) {
            g_uiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }

        // 1. Preset GroupBox
        CreateWindowExW(0, L"BUTTON", L" 预设风格 (Preset Style) ",
                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        15, 10, 440, 58, hwnd, nullptr, g_instance, nullptr);

        HWND hCombo = CreateWindowExW(0, L"COMBOBOX", L"",
                                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                                      30, 30, 410, 150, hwnd, (HMENU)(INT_PTR)IDC_PRESET_COMBO, g_instance, nullptr);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"✨ Wacom 细腻笔触风格 (推荐: 小巧/灵动/微点触感)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"🌊 经典水波涟漪风格 (较大半径/多层扩散/悠长)");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"🛠️ 自定义参数 (自由调节)");

        // 2. Color GroupBox
        CreateWindowExW(0, L"BUTTON", L" 点击波纹颜色 (点击色块选色) ",
                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        15, 74, 440, 78, hwnd, nullptr, g_instance, nullptr);

        CreateWindowExW(0, L"STATIC", L"左键颜色:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 94, 65, 20, hwnd, nullptr, g_instance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        98, 92, 40, 24, hwnd, (HMENU)(INT_PTR)IDC_BTN_COLOR_LEFT, g_instance, nullptr);

        CreateWindowExW(0, L"STATIC", L"右键颜色:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        170, 94, 65, 20, hwnd, nullptr, g_instance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        238, 92, 40, 24, hwnd, (HMENU)(INT_PTR)IDC_BTN_COLOR_RIGHT, g_instance, nullptr);

        CreateWindowExW(0, L"STATIC", L"中键颜色:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        310, 94, 65, 20, hwnd, nullptr, g_instance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        378, 92, 40, 24, hwnd, (HMENU)(INT_PTR)IDC_BTN_COLOR_MID, g_instance, nullptr);

        CreateWindowExW(0, L"STATIC", L"支持任意 Windows 自定义调色，各自独立配色",
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 124, 410, 18, hwnd, nullptr, g_instance, nullptr);

        // 3. Parameters GroupBox
        CreateWindowExW(0, L"BUTTON", L" 点击波纹细腻度调节 ",
                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        15, 158, 440, 222, hwnd, nullptr, g_instance, nullptr);

        // Max Radius Trackbar (15 - 90 px)
        CreateWindowExW(0, L"STATIC", L"最大半径:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 178, 220, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_RADIUS, g_instance, nullptr);
        HWND hTrkRadius = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                          25, 196, 420, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_RADIUS, g_instance, nullptr);
        SendMessageW(hTrkRadius, TBM_SETRANGE, TRUE, MAKELPARAM(15, 90));
        SendMessageW(hTrkRadius, TBM_SETTICFREQ, 5, 0);

        // Duration Trackbar (150 - 750 ms)
        CreateWindowExW(0, L"STATIC", L"动画时长:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 226, 220, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_DURATION, g_instance, nullptr);
        HWND hTrkDuration = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                            25, 244, 420, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_DURATION, g_instance, nullptr);
        SendMessageW(hTrkDuration, TBM_SETRANGE, TRUE, MAKELPARAM(150, 750));
        SendMessageW(hTrkDuration, TBM_SETTICFREQ, 50, 0);

        // Thickness Trackbar (1 - 5 px)
        CreateWindowExW(0, L"STATIC", L"线条粗细:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 274, 200, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_THICKNESS, g_instance, nullptr);
        HWND hTrkThickness = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                             WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                             25, 292, 195, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_THICKNESS, g_instance, nullptr);
        SendMessageW(hTrkThickness, TBM_SETRANGE, TRUE, MAKELPARAM(1, 5));

        // Rings Trackbar (1 - 3)
        CreateWindowExW(0, L"STATIC", L"波纹圈数:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        245, 274, 195, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_RINGS, g_instance, nullptr);
        HWND hTrkRings = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                         240, 292, 205, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_RINGS, g_instance, nullptr);
        SendMessageW(hTrkRings, TBM_SETRANGE, TRUE, MAKELPARAM(1, 3));

        // Click Ripple Opacity Trackbar (10 - 100 %)
        CreateWindowExW(0, L"STATIC", L"波纹透明度:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 324, 195, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_ALPHA, g_instance, nullptr);
        HWND hTrkAlpha = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                         25, 342, 195, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_ALPHA, g_instance, nullptr);
        SendMessageW(hTrkAlpha, TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
        SendMessageW(hTrkAlpha, TBM_SETTICFREQ, 10, 0);

        // Center dot checkbox (落笔触感微点)
        CreateWindowExW(0, L"BUTTON", L"开启落笔触点 (Wacom)",
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        240, 344, 205, 24, hwnd, (HMENU)(INT_PTR)IDC_CHK_CENTERDOT, g_instance, nullptr);

        // 4. Ambient Continuous Ripple GroupBox (常驻鼠标动态线条圈)
        CreateWindowExW(0, L"BUTTON", L" 常驻鼠标动态线条圈 (单圈纯净呼吸 / 0-100%透明度) ",
                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        15, 386, 440, 158, hwnd, nullptr, g_instance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"开启常驻动态线条 (跟随鼠标单圈呼吸)",
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        30, 406, 280, 22, hwnd, (HMENU)(INT_PTR)IDC_CHK_AMBIENT, g_instance, nullptr);

        // Ambient Color Setting
        CreateWindowExW(0, L"STATIC", L"颜色:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        320, 408, 38, 20, hwnd, nullptr, g_instance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        360, 405, 42, 22, hwnd, (HMENU)(INT_PTR)IDC_BTN_COLOR_AMBIENT, g_instance, nullptr);

        // Radius & Alpha (0 - 100%)
        CreateWindowExW(0, L"STATIC", L"常驻线条半径:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 432, 200, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_AMBIENT_RADIUS, g_instance, nullptr);
        HWND hTrkAmbientR = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                            25, 450, 195, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_AMBIENT_RADIUS, g_instance, nullptr);
        SendMessageW(hTrkAmbientR, TBM_SETRANGE, TRUE, MAKELPARAM(10, 45));
        SendMessageW(hTrkAmbientR, TBM_SETTICFREQ, 5, 0);

        CreateWindowExW(0, L"STATIC", L"常驻透明度:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        245, 432, 195, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_AMBIENT_ALPHA, g_instance, nullptr);
        HWND hTrkAmbientA = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                            240, 450, 205, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_AMBIENT_ALPHA, g_instance, nullptr);
        SendMessageW(hTrkAmbientA, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(hTrkAmbientA, TBM_SETTICFREQ, 10, 0);

        // Line Thickness (1 - 6 px)
        CreateWindowExW(0, L"STATIC", L"常驻线条粗细:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 482, 380, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_AMBIENT_BAND, g_instance, nullptr);
        HWND hTrkAmbientB = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                            25, 502, 420, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_AMBIENT_BAND, g_instance, nullptr);
        SendMessageW(hTrkAmbientB, TBM_SETRANGE, TRUE, MAKELPARAM(1, 6));
        SendMessageW(hTrkAmbientB, TBM_SETTICFREQ, 1, 0);

        // 5. Ink Ribbon Trail GroupBox (鼠标移动水墨流线拖尾)
        CreateWindowExW(0, L"BUTTON", L" 鼠标移动水墨流线拖尾 (书法笔触 / 颜色可调) ",
                        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        15, 550, 440, 176, hwnd, nullptr, g_instance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"开启水墨流线拖尾 (移动时光滑丝带渐隐)",
                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
                        30, 570, 280, 22, hwnd, (HMENU)(INT_PTR)IDC_CHK_TRAIL, g_instance, nullptr);

        // Trail Color Setting
        CreateWindowExW(0, L"STATIC", L"颜色:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        320, 572, 38, 20, hwnd, nullptr, g_instance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                        360, 569, 42, 22, hwnd, (HMENU)(INT_PTR)IDC_BTN_COLOR_TRAIL, g_instance, nullptr);

        // Trail Duration Trackbar (150 - 800 ms)
        CreateWindowExW(0, L"STATIC", L"拖尾留存:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 596, 380, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_TRAIL_DURATION, g_instance, nullptr);
        HWND hTrkTrailD = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                          25, 614, 420, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_TRAIL_DURATION, g_instance, nullptr);
        SendMessageW(hTrkTrailD, TBM_SETRANGE, TRUE, MAKELPARAM(150, 800));
        SendMessageW(hTrkTrailD, TBM_SETTICFREQ, 50, 0);

        // Trail Width (3 - 18 px) & Alpha (10 - 100 %)
        CreateWindowExW(0, L"STATIC", L"拖尾粗细:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        30, 646, 200, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_TRAIL_WIDTH, g_instance, nullptr);
        HWND hTrkTrailW = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                          25, 664, 195, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_TRAIL_WIDTH, g_instance, nullptr);
        SendMessageW(hTrkTrailW, TBM_SETRANGE, TRUE, MAKELPARAM(3, 18));
        SendMessageW(hTrkTrailW, TBM_SETTICFREQ, 2, 0);

        CreateWindowExW(0, L"STATIC", L"拖尾透明度:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        245, 646, 195, 18, hwnd, (HMENU)(INT_PTR)IDC_LBL_TRAIL_ALPHA, g_instance, nullptr);
        HWND hTrkTrailA = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
                                          240, 664, 205, 28, hwnd, (HMENU)(INT_PTR)IDC_TRK_TRAIL_ALPHA, g_instance, nullptr);
        SendMessageW(hTrkTrailA, TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
        SendMessageW(hTrkTrailA, TBM_SETTICFREQ, 10, 0);

        CreateWindowExW(0, L"STATIC", L"💡 如同书法水墨流畅挥毫，头部圆润、尾部渐细羽化、自然消散",
                        WS_CHILD | WS_VISIBLE | SS_LEFT,
                        25, 698, 430, 18, hwnd, nullptr, g_instance, nullptr);

        // 6. Action Buttons
        CreateWindowExW(0, L"BUTTON", L"恢复默认设置",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                        15, 738, 140, 32, hwnd, (HMENU)(INT_PTR)IDC_BTN_RESET, g_instance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"保存并关闭",
                        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                        315, 738, 140, 32, hwnd, (HMENU)(INT_PTR)IDC_BTN_SAVE, g_instance, nullptr);

        // Apply modern font to all child controls
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lParam) -> BOOL {
            SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
            return TRUE;
        }, (LPARAM)g_uiFont);

        syncSettingsControls(hwnd);
        return 0;
    }

    case WM_DRAWITEM: {
        const auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        COLORREF fillCol = RGB(128, 128, 128);
        if (dis->CtlID == IDC_BTN_COLOR_LEFT) fillCol = g_config.leftColor;
        else if (dis->CtlID == IDC_BTN_COLOR_RIGHT) fillCol = g_config.rightColor;
        else if (dis->CtlID == IDC_BTN_COLOR_MID) fillCol = g_config.middleColor;
        else if (dis->CtlID == IDC_BTN_COLOR_AMBIENT) fillCol = g_config.ambientColor;
        else if (dis->CtlID == IDC_BTN_COLOR_TRAIL) fillCol = g_config.trailColor;

        HBRUSH brush = CreateSolidBrush(fillCol);
        FillRect(dis->hDC, &dis->rcItem, brush);
        DeleteObject(brush);

        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
        HGDIOBJ oldPen = SelectObject(dis->hDC, borderPen);
        SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
        Rectangle(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom);
        SelectObject(dis->hDC, oldPen);
        DeleteObject(borderPen);
        return TRUE;
    }

    case WM_HSCROLL: {
        HWND hTrack = reinterpret_cast<HWND>(lParam);
        const int val = (int)SendMessageW(hTrack, TBM_GETPOS, 0, 0);

        if (hTrack == GetDlgItem(hwnd, IDC_TRK_RADIUS)) {
            g_config.maxRadius = val;
            g_config.stylePreset = PRESET_CUSTOM;
            SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, PRESET_CUSTOM, 0);
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_DURATION)) {
            g_config.durationMs = val;
            g_config.stylePreset = PRESET_CUSTOM;
            SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, PRESET_CUSTOM, 0);
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_THICKNESS)) {
            g_config.ringThickness = val;
            g_config.stylePreset = PRESET_CUSTOM;
            SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, PRESET_CUSTOM, 0);
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_RINGS)) {
            g_config.ringCount = val;
            g_config.stylePreset = PRESET_CUSTOM;
            SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, PRESET_CUSTOM, 0);
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_ALPHA)) {
            g_config.maxAlpha = static_cast<int>(255.0f * (val / 100.0f));
            g_config.stylePreset = PRESET_CUSTOM;
            SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_SETCURSEL, PRESET_CUSTOM, 0);
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_AMBIENT_RADIUS)) {
            g_config.ambientRadius = val;
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_AMBIENT_ALPHA)) {
            g_config.ambientAlphaPercent = val;
            g_config.ambientAlpha = static_cast<int>(255.0f * (val / 100.0f));
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_AMBIENT_BAND)) {
            g_config.ambientThickness = val;
            g_config.ambientBand = val;
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_TRAIL_DURATION)) {
            g_config.trailDurationMs = val;
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_TRAIL_WIDTH)) {
            g_config.trailWidth = val;
        } else if (hTrack == GetDlgItem(hwnd, IDC_TRK_TRAIL_ALPHA)) {
            g_config.trailAlphaPercent = val;
            g_config.trailAlpha = static_cast<int>(255.0f * (val / 100.0f));
        }
        updateSettingsLabels(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (id == IDC_PRESET_COMBO && code == CBN_SELCHANGE) {
            const int sel = (int)SendDlgItemMessageW(hwnd, IDC_PRESET_COMBO, CB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel != PRESET_CUSTOM) {
                applyPreset(sel);
                syncSettingsControls(hwnd);
            }
            return 0;
        }

        if (id == IDC_BTN_COLOR_LEFT) {
            if (pickColor(hwnd, g_config.leftColor)) {
                InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_LEFT), nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_BTN_COLOR_RIGHT) {
            if (pickColor(hwnd, g_config.rightColor)) {
                InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_RIGHT), nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_BTN_COLOR_MID) {
            if (pickColor(hwnd, g_config.middleColor)) {
                InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_MID), nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_BTN_COLOR_AMBIENT) {
            if (pickColor(hwnd, g_config.ambientColor)) {
                InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_AMBIENT), nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_BTN_COLOR_TRAIL) {
            if (pickColor(hwnd, g_config.trailColor)) {
                InvalidateRect(GetDlgItem(hwnd, IDC_BTN_COLOR_TRAIL), nullptr, TRUE);
            }
            return 0;
        }

        if (id == IDC_CHK_CENTERDOT) {
            g_config.centerDot = (IsDlgButtonChecked(hwnd, IDC_CHK_CENTERDOT) == BST_CHECKED);
            return 0;
        }

        if (id == IDC_CHK_AMBIENT) {
            g_config.ambientRipple = (IsDlgButtonChecked(hwnd, IDC_CHK_AMBIENT) == BST_CHECKED);
            return 0;
        }

        if (id == IDC_CHK_TRAIL) {
            g_config.trailEnabled = (IsDlgButtonChecked(hwnd, IDC_CHK_TRAIL) == BST_CHECKED);
            if (!g_config.trailEnabled) {
                g_trailPoints.clear();
            }
            return 0;
        }

        if (id == IDC_BTN_RESET) {
            applyPreset(PRESET_WACOM);
            g_config.leftColor = RGB(232, 65, 82);
            g_config.rightColor = RGB(41, 128, 245);
            g_config.middleColor = RGB(245, 166, 35);
            g_config.ambientColor = RGB(145, 145, 145);
            g_config.ambientBand = 13;
            g_config.trailEnabled = true;
            g_config.trailColor = RGB(41, 128, 245);
            g_config.trailDurationMs = 380;
            g_config.trailWidth = 8;
            g_config.trailAlphaPercent = 80;
            g_config.trailAlpha = static_cast<int>(255.0f * 0.80f);
            syncSettingsControls(hwnd);
            return 0;
        }

        if (id == IDC_BTN_SAVE) {
            saveConfig();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        saveConfig();
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (g_uiFont) {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
        g_settingsWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void openSettingsWindow() {
    if (g_settingsWnd && IsWindow(g_settingsWnd)) {
        ShowWindow(g_settingsWnd, SW_SHOW);
        ShowWindow(g_settingsWnd, SW_RESTORE);
        SetWindowPos(g_settingsWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(g_settingsWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(g_settingsWnd);
        SetActiveWindow(g_settingsWnd);
        return;
    }

    const wchar_t kSettingsClassName[] = L"MouseRippleSettingsWindow";
    static bool s_settingsClassRegistered = false;
    if (!s_settingsClassRegistered) {
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = settingsWndProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = kSettingsClassName;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassEx(&wc);
        s_settingsClassRegistered = true;
    }

    const int w = 485;
    const int h = 825;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_settingsWnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        kSettingsClassName,
        L"MouseRipple - 自定义设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, g_instance, nullptr);

    if (g_settingsWnd) {
        ShowWindow(g_settingsWnd, SW_SHOW);
        UpdateWindow(g_settingsWnd);
        SetWindowPos(g_settingsWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(g_settingsWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(g_settingsWnd);
    }
}

// -------------------------------------------------------------
// Main Overlay Window Implementation
// -------------------------------------------------------------

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        addTrayIcon(hwnd);
        SetTimer(hwnd, kFrameTimerId, kFrameMs, nullptr);
        return 0;

    case WM_RIPPLE_CLICK: {
        POINT* p = reinterpret_cast<POINT*>(lParam);
        if (p) {
            addRipple(*p, static_cast<int>(wParam));
            delete p;
        }
        renderOverlay(hwnd);
        return 0;
    }

    case WM_TIMER:
        if (wParam == kFrameTimerId) {
            const ULONGLONG now = GetTickCount64();
            const ULONGLONG maxDuration = static_cast<ULONGLONG>(g_config.durationMs + 60);
            g_ripples.erase(
                std::remove_if(g_ripples.begin(), g_ripples.end(),
                    [now, maxDuration](const Ripple& r) {
                        return (now - r.startedAt) > maxDuration;
                    }),
                g_ripples.end());

            // Prune expired trail points
            if (!g_trailPoints.empty()) {
                const ULONGLONG trailMaxAge = static_cast<ULONGLONG>(g_config.trailDurationMs);
                g_trailPoints.erase(
                    std::remove_if(g_trailPoints.begin(), g_trailPoints.end(),
                        [now, trailMaxAge](const TrailPoint& tp) {
                            return (now - tp.timeMs) > trailMaxAge;
                        }),
                    g_trailPoints.end());
            }

            static bool s_wasIdle = false;
            const bool isIdle = g_ripples.empty() && g_trailPoints.empty() && !g_config.ambientRipple;
            if (isIdle) {
                if (!s_wasIdle) {
                    renderOverlay(hwnd);
                    s_wasIdle = true;
                }
            } else {
                s_wasIdle = false;
                renderOverlay(hwnd);
            }
        }
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        updateVirtualDesktopMetrics();
        SetWindowPos(hwnd, HWND_TOPMOST,
                     g_virtualX, g_virtualY, g_virtualW, g_virtualH,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        renderOverlay(hwnd);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            showTrayMenu(hwnd);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            openSettingsWindow();
        }
        return 0;

    case WM_COMMAND: {
        const UINT id = LOWORD(wParam);
        if (id == ID_TRAY_SETTINGS) {
            openSettingsWindow();
            return 0;
        }
        if (id == ID_TRAY_AUTOSTART) {
            const bool enabled = isAutoStartEnabled();
            setAutoStart(!enabled);
            return 0;
        }
        if (id == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_DESTROY:
        KillTimer(hwnd, kFrameTimerId);
        removeTrayIcon(hwnd);
        if (g_settingsWnd) {
            DestroyWindow(g_settingsWnd);
            g_settingsWnd = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    g_instance = hInstance;

    // Single instance protection: wake up existing instance and bring settings to front
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"MouseRipple_SingleInstance_Mutex_2026");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hOverlay = FindWindowW(L"MouseRippleOverlayWindow", nullptr);
        HWND hSettings = FindWindowW(L"MouseRippleSettingsWindow", nullptr);
        if (hOverlay || hSettings) {
            DWORD targetPid = 0;
            if (hOverlay) {
                GetWindowThreadProcessId(hOverlay, &targetPid);
            } else if (hSettings) {
                GetWindowThreadProcessId(hSettings, &targetPid);
            }
            if (targetPid) {
                AllowSetForegroundWindow(targetPid);
            }
            if (hOverlay) {
                PostMessageW(hOverlay, WM_COMMAND, ID_TRAY_SETTINGS, 0);
            }
            if (hSettings) {
                ShowWindow(hSettings, SW_SHOW);
                ShowWindow(hSettings, SW_RESTORE);
                SetWindowPos(hSettings, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                SetWindowPos(hSettings, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                SetForegroundWindow(hSettings);
                SetActiveWindow(hSettings);
            }
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
        // If neither overlay nor settings window exists, previous instance died or was orphaned. Proceed!
    }

    // Modern Per-Monitor DPI
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        auto setContext = (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setContext) {
            setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            SetProcessDPIAware();
        }
    } else {
        SetProcessDPIAware();
    }

    // Initialize Common Controls v6
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Load user configuration
    loadConfig();

    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBox(nullptr, L"Could not start GDI+.", L"Mouse Ripple", MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    const wchar_t kClassName[] = L"MouseRippleOverlayWindow";
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = overlayWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wc);

    updateVirtualDesktopMetrics();

    g_overlay = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName,
        L"Mouse Ripple Overlay",
        WS_POPUP,
        g_virtualX, g_virtualY, g_virtualW, g_virtualH,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_overlay) {
        GdiplusShutdown(g_gdiplusToken);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
    SetWindowPos(g_overlay, HWND_TOPMOST,
                 g_virtualX, g_virtualY, g_virtualW, g_virtualH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc, hInstance, 0);
    if (!g_mouseHook) {
        MessageBox(nullptr, L"Could not install mouse hook.", L"Mouse Ripple", MB_ICONERROR);
        DestroyWindow(g_overlay);
        GdiplusShutdown(g_gdiplusToken);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // On manual launch, always open settings GUI so the user clearly sees the software has opened!
    // Only suppress window if started via Windows autostart (--autostart) or explicit background flag (--tray)
    const bool isBackgroundMode = lpCmdLine && (wcsstr(lpCmdLine, L"--autostart") || wcsstr(lpCmdLine, L"--tray") || wcsstr(lpCmdLine, L"--minimized"));
    if (!isBackgroundMode) {
        openSettingsWindow();
    }

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    GdiplusShutdown(g_gdiplusToken);
    if (hMutex) CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
