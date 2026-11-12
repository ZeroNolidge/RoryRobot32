// Tiny anti-aliased 2D renderer into an RGB565 framebuffer.
// Shapes are drawn from signed-distance functions, which gives smooth edges
// for Rory's round eyes and mouth. Plain C++ (no Arduino), so the same code
// also runs on a PC to make preview pictures (see tools/face-preview).
#pragma once
#include <stdint.h>
#include <math.h>

class Raster {
 public:
  uint16_t* px = nullptr;
  int w = 0, h = 0;

  Raster() = default;
  Raster(uint16_t* buf, int width, int height) : px(buf), w(width), h(height) {}

  static uint16_t rgb(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF, g = (hex >> 8) & 0xFF, b = hex & 0xFF;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  }

  static uint16_t mix(uint16_t dst, uint16_t src, float a) {
    if (a >= 0.999f) return src;
    int ia = (int)(a * 32.0f + 0.5f);
    if (ia <= 0) return dst;
    int r = ((dst >> 11) * (32 - ia) + (src >> 11) * ia) >> 5;
    int g = (((dst >> 5) & 0x3F) * (32 - ia) + ((src >> 5) & 0x3F) * ia) >> 5;
    int b = ((dst & 0x1F) * (32 - ia) + (src & 0x1F) * ia) >> 5;
    return (uint16_t)((r << 11) | (g << 5) | b);
  }

  void clear(uint16_t c) {
    for (int i = 0, n = w * h; i < n; i++) px[i] = c;
  }

  void dot(int x, int y, uint16_t c) {
    if (x >= 0 && y >= 0 && x < w && y < h) px[y * w + x] = c;
  }

  // Fill every pixel in the box whose signed distance (pixels, <0 inside) is < 0.5.
  template <class SDF>
  void shade(float bx0, float by0, float bx1, float by1, uint16_t c, float alpha, SDF sdf) {
    if (alpha <= 0.01f) return;
    int x0 = (int)floorf(bx0) - 1, y0 = (int)floorf(by0) - 1;
    int x1 = (int)ceilf(bx1) + 1, y1 = (int)ceilf(by1) + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;
    for (int y = y0; y < y1; y++) {
      uint16_t* row = px + y * w;
      float fy = y + 0.5f;
      for (int x = x0; x < x1; x++) {
        float d = sdf(x + 0.5f, fy);
        if (d >= 0.5f) continue;
        float a = 0.5f - d;
        if (a > 1.0f) a = 1.0f;
        row[x] = mix(row[x], c, a * alpha);
      }
    }
  }

  // ---- shapes -----------------------------------------------------------
  void circle(float cx, float cy, float r, uint16_t c, float alpha = 1) {
    shade(cx - r, cy - r, cx + r, cy + r, c, alpha, [=](float x, float y) {
      return sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r;
    });
  }

  static float ellipseDist(float x, float y, float rx, float ry) {
    float nx = x / (rx * rx), ny = y / (ry * ry);
    float f = (x * x) / (rx * rx) + (y * y) / (ry * ry) - 1.0f;
    float g = 2.0f * sqrtf(nx * nx + ny * ny);
    if (g < 1e-4f) return -(rx < ry ? rx : ry);
    return f / g;
  }

  void ellipse(float cx, float cy, float rx, float ry, uint16_t c, float alpha = 1) {
    if (rx < 0.5f || ry < 0.5f) return;
    shade(cx - rx, cy - ry, cx + rx, cy + ry, c, alpha,
          [=](float x, float y) { return ellipseDist(x - cx, y - cy, rx, ry); });
  }

  // Ellipse cut flat at y = cy (only the lower half), e.g. an open "D" mouth.
  void halfEllipseDown(float cx, float cy, float rx, float ry, uint16_t c, float alpha = 1) {
    if (rx < 0.5f || ry < 0.5f) return;
    shade(cx - rx, cy, cx + rx, cy + ry, c, alpha, [=](float x, float y) {
      float d = ellipseDist(x - cx, y - cy, rx, ry);
      float top = cy - y;
      return d > top ? d : top;
    });
  }

  // Thick line with round ends.
  void capsule(float ax, float ay, float bx, float by, float r, uint16_t c, float alpha = 1) {
    float mnx = (ax < bx ? ax : bx) - r, mxx = (ax > bx ? ax : bx) + r;
    float mny = (ay < by ? ay : by) - r, mxy = (ay > by ? ay : by) + r;
    float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    shade(mnx, mny, mxx, mxy, c, alpha, [=](float x, float y) {
      float t = len2 > 0 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0;
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      float qx = x - (ax + t * dx), qy = y - (ay + t * dy);
      return sqrtf(qx * qx + qy * qy) - r;
    });
  }

  // Thick circular arc with round ends. Angles in degrees, 0 = right,
  // 90 = straight down (screen y grows downward), drawn from a0 clockwise to a1.
  void arc(float cx, float cy, float R, float thick, float a0, float a1, uint16_t c, float alpha = 1) {
    const float D2R = 0.01745329f;
    float span = fmodf(a1 - a0 + 720.0f, 360.0f);
    float hx0 = cx + R * cosf(a0 * D2R), hy0 = cy + R * sinf(a0 * D2R);
    float hx1 = cx + R * cosf(a1 * D2R), hy1 = cy + R * sinf(a1 * D2R);
    float half = thick * 0.5f, ext = R + half;
    shade(cx - ext, cy - ext, cx + ext, cy + ext, c, alpha, [=](float x, float y) {
      float dx = x - cx, dy = y - cy;
      float ang = atan2f(dy, dx) / D2R;
      float rel = fmodf(ang - a0 + 720.0f, 360.0f);
      if (rel <= span) return fabsf(sqrtf(dx * dx + dy * dy) - R) - half;
      float d0 = sqrtf((x - hx0) * (x - hx0) + (y - hy0) * (y - hy0));
      float d1 = sqrtf((x - hx1) * (x - hx1) + (y - hy1) * (y - hy1));
      return (d0 < d1 ? d0 : d1) - half;
    });
  }

  // Four-point twinkle star.
  void star(float cx, float cy, float r, uint16_t c, float alpha = 1) {
    float thin = r * 0.28f;
    shade(cx - r, cy - r, cx + r, cy + r, c, alpha, [=](float x, float y) {
      float ax = fabsf(x - cx), ay = fabsf(y - cy);
      float d1 = (ax / thin + ay / r - 1.0f) * thin;  // vertical spike
      float d2 = (ax / r + ay / thin - 1.0f) * thin;  // horizontal spike
      return d1 < d2 ? d1 : d2;
    });
  }

  // Heart centered at (cx, cy), roughly 2*s wide.
  void heart(float cx, float cy, float s, uint16_t c, float alpha = 1) {
    float r = s * 0.52f, ox = s * 0.48f, oy = -s * 0.30f;
    shade(cx - s, cy - s, cx + s, cy + s * 1.15f, c, alpha, [=](float x, float y) {
      float px_ = x - cx, py = y - cy;
      float c1 = sqrtf((px_ + ox) * (px_ + ox) + (py - oy) * (py - oy)) - r;
      float c2 = sqrtf((px_ - ox) * (px_ - ox) + (py - oy) * (py - oy)) - r;
      // downward-pointing wedge, tangent to both lobes
      float w0 = (ox + r) * 0.92f;
      float k = (fabsf(px_) + (py - oy) * 0.95f - w0) * 0.72f;
      float wedge = k > (oy - py) ? k : (oy - py);
      float d = c1 < c2 ? c1 : c2;
      return d < wedge ? d : wedge;
    });
  }
};
