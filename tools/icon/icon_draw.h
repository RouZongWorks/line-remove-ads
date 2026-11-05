// Vector drawing of the application icon: a green LINE-style chat bubble with a red
// "prohibited" sign. Used by make_icon.cpp to generate assets/icon.ico.
#pragma once
#include <algorithm>
using std::min;
using std::max;
#include <windows.h>
#include <gdiplus.h>

// Draws the icon into a square 32bpp ARGB bitmap of any size (drawing is resolution independent).
inline void DrawAppIcon(Gdiplus::Bitmap& bmp) {
    using namespace Gdiplus;
    const float S = 256.0f;                              // logical drawing space
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    g.ScaleTransform((float)bmp.GetWidth() / S, (float)bmp.GetHeight() / S);
    g.Clear(Color(0, 0, 0, 0));

    // Green rounded square.
    GraphicsPath sq;
    const float r = 52;
    sq.AddArc(0.f, 0.f, r, r, 180, 90);
    sq.AddArc(S - r, 0.f, r, r, 270, 90);
    sq.AddArc(S - r, S - r, r, r, 0, 90);
    sq.AddArc(0.f, S - r, r, r, 90, 90);
    sq.CloseFigure();
    SolidBrush green(Color(255, 6, 199, 85));
    g.FillPath(&green, &sq);

    // White speech bubble with a tail.
    SolidBrush white(Color(255, 255, 255, 255));
    g.FillEllipse(&white, 36, 44, 184, 128);
    Point tail[3] = { Point(86, 150), Point(78, 204), Point(130, 168) };
    g.FillPolygon(&white, tail, 3);

    // "LINE" label.
    FontFamily family(L"Arial");
    Font font(&family, 58, FontStyleBold, UnitPixel);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(L"LINE", -1, &font, RectF(30, 44, 196, 112), &format, &green);

    // Prohibited sign: a white halo for contrast, then the red ring and slash on top.
    const float cx = 128, cy = 128, radius = 96;
    const float d = radius * 0.7071f;
    Pen halo(Color(255, 255, 255, 255), 36);
    g.DrawEllipse(&halo, cx - radius, cy - radius, radius * 2, radius * 2);
    g.DrawLine(&halo, cx - d, cy - d, cx + d, cy + d);
    Pen red(Color(255, 225, 29, 40), 22);
    red.SetStartCap(LineCapRound);
    red.SetEndCap(LineCapRound);
    g.DrawEllipse(&red, cx - radius, cy - radius, radius * 2, radius * 2);
    g.DrawLine(&red, cx - d, cy - d, cx + d, cy + d);
}
