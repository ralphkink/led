#include "strawberry.h"

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static const uint16_t C_BG   = 0x0000;
static const uint16_t C_GRN  = RGB565( 46, 176, 58);
static const uint16_t C_DGRN = RGB565( 22, 120, 34);
static const uint16_t C_LGRN = RGB565( 96, 210, 86);
static const uint16_t C_RED  = RGB565(220,  30, 40);
static const uint16_t C_DRED = RGB565(150,  14, 26);
static const uint16_t C_PINK = RGB565(255, 150, 140);
static const uint16_t C_SEED = RGB565(250, 212, 96);

uint16_t strawberry[PANEL_H][PANEL_W];

static inline void plotS(int x, int y, uint16_t c) {
  if (x >= 0 && x < PANEL_W && y >= 0 && y < PANEL_H) strawberry[y][x] = c;
}

static void drawLeafS(int tx, int ty, int bx, int by, float halfWidth, uint16_t col) {
  int steps = by - ty; if (steps <= 0) return;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps, cx = tx + (bx - tx) * t, w = halfWidth * t;
    int yy = ty + i;
    for (int xx = (int)(cx - w); xx <= (int)(cx + w); xx++) plotS(xx, yy, col);
  }
}

static float strawHalfWidth(int y) {
  const int   Y0 = 20, Y1 = 60;
  const float MAXW = 17.0f, WB = 12.0f, SHOULDER = 0.34f;  // SHOULDER raised 0.14 -> 0.34
  const float CAPT0 = 1.0f - WB / (Y1 - Y0);   // start of rounded cap (= 0.70)
  if (y < Y0 || y > Y1) return -1.0f;
  float t = (float)(y - Y0) / (Y1 - Y0);
  if (t < SHOULDER) {                            // rounded top shoulder (now larger)
    float k = (SHOULDER - t) / SHOULDER;
    return MAXW * sqrtf(fmaxf(0.0f, 1.0f - k * k));
  }
  if (t < CAPT0) {                               // gentle taper toward the cap
    float u = (t - SHOULDER) / (CAPT0 - SHOULDER);
    return MAXW + (WB - MAXW) * u;
  }
  float s = (t - CAPT0) / (1.0f - CAPT0);        // rounded semicircular bottom
  return WB * sqrtf(fmaxf(0.0f, 1.0f - s * s));
}

void buildStrawberry() {
  const int CX = 32, Y0 = 20, Y1 = 60;

  for (int y = 0; y < PANEL_H; y++)
    for (int x = 0; x < PANEL_W; x++)
      strawberry[y][x] = C_BG;

  // Body with edge shading and an upper-left highlight.
  for (int y = 0; y < PANEL_H; y++) {
    float hw = strawHalfWidth(y); if (hw < 0) continue;
    for (int x = 0; x < PANEL_W; x++) {
      float d = fabsf((float)(x - CX));
      if (d <= hw) {
        uint16_t c = C_RED;
        if (d > hw - 2.0f) c = C_DRED;
        int hx = x - (CX - 6), hy = y - (Y0 + 8);
        if (hx * hx + hy * hy < 20) c = C_PINK;
        strawberry[y][x] = c;
      }
    }
  }

  // Seeds on a staggered lattice.
  int row = 0;
  for (int yy = Y0 + 4; yy < Y1 - 2; yy += 5, row++) {
    int off = (row & 1) ? 3 : 0;
    for (int xx = CX - 16 + off; xx <= CX + 16; xx += 7) {
      float hw = strawHalfWidth(yy);
      if (hw > 0 && fabsf((float)(xx - CX)) < hw - 2.0f) {
        plotS(xx, yy,     C_SEED);
        plotS(xx, yy - 1, C_SEED);
      }
    }
  }

  // Calyx (green crown).
  drawLeafS(32,  6, 32, 22, 4, C_GRN);
  drawLeafS(22,  9, 28, 21, 3, C_DGRN);
  drawLeafS(42,  9, 36, 21, 3, C_DGRN);
  drawLeafS(16, 14, 26, 21, 3, C_LGRN);
  drawLeafS(48, 14, 38, 21, 3, C_LGRN);
  drawLeafS(32,  3, 32, 18, 3, C_LGRN);
}
