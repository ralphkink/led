#include "pineapple.h"

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static const uint16_t C_BG   = 0x0000;
static const uint16_t C_YEL  = RGB565(250, 196, 40);
static const uint16_t C_GOLD = RGB565(214, 142, 28);
static const uint16_t C_BRN  = RGB565(140,  82, 18);
static const uint16_t C_HI   = RGB565(255, 228, 128);
static const uint16_t C_GRN  = RGB565( 46, 176, 58);
static const uint16_t C_DGRN = RGB565( 22, 120, 34);
static const uint16_t C_LGRN = RGB565( 96, 210, 86);

uint16_t pineapple[PANEL_H][PANEL_W];

// Positive modulo-6 (C's % can return negative values).
static inline int wrap6(int v) { v %= 6; if (v < 0) v += 6; return v; }

static inline void plot(int x, int y, uint16_t c) {
  if (x >= 0 && x < PANEL_W && y >= 0 && y < PANEL_H) pineapple[y][x] = c;
}

// Rasterize one triangular leaf blade from tip (tx,ty) to base center (bx,by).
static void drawLeaf(int tx, int ty, int bx, int by, float halfWidth, uint16_t col) {
  int steps = by - ty;
  if (steps <= 0) return;
  for (int i = 0; i <= steps; i++) {
    float t  = (float)i / steps;
    float cx = tx + (bx - tx) * t;
    float w  = halfWidth * t;
    int   yy = ty + i;
    for (int xx = (int)(cx - w); xx <= (int)(cx + w); xx++) plot(xx, yy, col);
  }
}

void buildPineapple() {
  const int   CX = 32, BODY_CY = 42;       // body center
  const float RX = 14.0f, RY = 18.0f;      // body radii

  // Clear to background.
  for (int y = 0; y < PANEL_H; y++)
    for (int x = 0; x < PANEL_W; x++)
      pineapple[y][x] = C_BG;

  // Body: filled ellipse with a diagonal lattice crosshatch.
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      float dx = (x - CX) / RX, dy = (y - BODY_CY) / RY;
      if (dx * dx + dy * dy <= 1.0f) {
        bool on1 = wrap6(x + y) <= 1;      // first diagonal set
        bool on2 = wrap6(x - y) <= 1;      // second diagonal set
        uint16_t c = C_YEL;
        if (on1 && on2)        c = C_BRN;  // lattice intersection
        else if (on1 || on2)   c = C_GOLD; // lattice line
        int hx = x - (CX - 5), hy = y - (BODY_CY - 8);
        if (hx * hx + hy * hy < 16 && !(on1 && on2)) c = C_HI; // upper-left sheen
        pineapple[y][x] = c;
      }
    }
  }

  // Crown: leaf blades drawn over the body top.
  drawLeaf(32,  8, 32, 26, 4, C_GRN);
  drawLeaf(24, 12, 28, 26, 3, C_DGRN);
  drawLeaf(40, 12, 36, 26, 3, C_DGRN);
  drawLeaf(18, 18, 26, 26, 3, C_LGRN);
  drawLeaf(46, 18, 38, 26, 3, C_LGRN);
  drawLeaf(32,  4, 32, 22, 3, C_LGRN);
}
