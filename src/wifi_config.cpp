#include <Arduino.h>
#include <WiFi.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "wifi_config.h"


void connectWiFi(MatrixPanel_I2S_DMA *display) {
  display->clearScreen();
  display->setTextWrap(false);
  display->setTextSize(1);
  display->setTextColor(display->color565(255, 200, 0));
  display->setCursor(2, 2);
  display->print("WiFi");
  display->setCursor(2, 12);
  display->print("connecting");

  delay(2000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait up to 10 s (5 × 2 000 ms); refresh status on panel each attempt.
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    // Overwrite just the status row so the header stays visible.
    display->fillRect(0, 22, display->width(), 10, display->color565(0, 0, 0));
    display->setTextColor(display->color565(160, 160, 160));
    display->setCursor(2, 22);
    display->printf("st:%d #%d", (int)WiFi.status(), attempts);
    delay(2000);
    attempts++;
  }

  display->clearScreen();
  if (WiFi.status() == WL_CONNECTED) {
    display->setTextColor(display->color565(0, 255, 80));
    display->setCursor(2, 2);
    display->print("WiFi OK");

    // Show IP address split across two lines (fits 6-px font).
    String ip = WiFi.localIP().toString();
    int dot = ip.lastIndexOf('.');
    display->setTextColor(display->color565(180, 220, 255));
    display->setCursor(2, 14);
    display->print(ip.substring(0, dot + 1));   // e.g. "192.168.1."
    display->setCursor(2, 24);
    display->print(ip.substring(dot + 1));       // e.g. "42"

    display->setTextColor(display->color565(120, 120, 120));
    display->setCursor(2, 36);
    display->printf("RSSI %d", WiFi.RSSI());
    delay(3000);
  } else {
    display->setTextColor(display->color565(255, 40, 40));
    display->setCursor(2, 2);
    display->print("WiFi");
    display->setCursor(2, 12);
    display->print("FAILED");
    delay(2000);
  }
}
