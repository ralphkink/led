#include "memory_display.h"

void showMemory(MatrixPanel_I2S_DMA *display) {
  uint32_t total = ESP.getHeapSize();
  uint32_t freeB = ESP.getFreeHeap();
  uint32_t used  = total - freeB;
  uint8_t  pct   = (uint8_t)((uint64_t)used * 100 / total);

  display->clearScreen();
  display->setTextWrap(false);
  display->setTextSize(1);

  display->setTextColor(display->color565(255, 200, 0));
  display->setCursor(2, 2);
  display->print("HEAP KB");

  display->setTextColor(display->color565(255, 80, 80));
  display->setCursor(2, 14);
  display->printf("U:%lu", (unsigned long)(used  / 1024));

  display->setTextColor(display->color565(80, 255, 120));
  display->setCursor(2, 24);
  display->printf("F:%lu", (unsigned long)(freeB / 1024));

  display->setTextColor(display->color565(120, 180, 255));
  display->setCursor(2, 34);
  display->printf("T:%lu", (unsigned long)(total / 1024));

  int barW = (pct * 60) / 100;
  display->drawRect(1, 45, 62, 9, display->color565(70, 70, 70));
  display->fillRect(2, 46, barW, 7, display->color565(255, 140, 0));

  display->setTextColor(display->color565(255, 255, 255));
  display->setCursor(2, 56);
  display->printf("%u%%", pct);
}
