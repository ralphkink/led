#include <Arduino.h>
#include <WiFi.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "wifi_config.h"

const int ledPin = 2;

#define PANEL_W 64
#define PANEL_H 64

#include "static_pics/face_bitmap.h"
#include "util/memory_display.h"
#include "static_pics/pineapple.h"
#include "static_pics/strawberry.h"
#include "util/asb_utils.h"
#include <mbedtls/base64.h>

MatrixPanel_I2S_DMA *display = nullptr;
static bool pictureSet = false;

// ---- Sends the bitmap to the display ----
void sendBitmap() {
  for (int y = 0; y < PANEL_H; y++)
    for (int x = 0; x < PANEL_W; x++)
      display->drawPixel(x, y, pineapple[y][x]);
}

// ---- Decodes a Base64 RGB565 image (64x64x2 = 8192 bytes) onto the panel ----
static void displayBase64Image(const String& b64) {
  const size_t imgBytes = PANEL_W * PANEL_H * 2;
  uint8_t* buf = new uint8_t[imgBytes];
  size_t outLen = 0;
  int ret = mbedtls_base64_decode(buf, imgBytes, &outLen,
                                   (const uint8_t*)b64.c_str(), b64.length());
  if (ret == 0 && outLen == imgBytes) {
    for (int y = 0; y < PANEL_H; y++)
      for (int x = 0; x < PANEL_W; x++) {
        int i = (y * PANEL_W + x) * 2;
        uint16_t pixel = ((uint16_t)buf[i] << 8) | buf[i + 1];
        display->drawPixel(x, y, pixel);
      }
  }
  delete[] buf;
}

// ---- Generic sender: pushes any 64x64 RGB565 buffer to the panel ----
void sendBuffer(uint16_t (*buf)[PANEL_W]) {
  for (int y = 0; y < PANEL_H; y++)
    for (int x = 0; x < PANEL_W; x++)
      display->drawPixel(x, y, buf[y][x]);
}

// ---- Connects to WiFi and shows status on the panel ----
void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");
 
  pinMode(ledPin, OUTPUT);
  HUB75_I2S_CFG mxconfig(64, 64, 1);   // width, height, chain length
  mxconfig.gpio.e = 32;                // required for 64x64
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A; // uncomment if screen stays blank

  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();
  display->setBrightness8(120);         // 0–255; orig 90
  display->clearScreen();

  connectWiFi(display);

  display->setTextSize(1);
  display->setTextColor(display->color565(255, 0, 0));
  display->setCursor(4, 20);
  buildPineapple();                     // create the bitmap
  buildStrawberry();
  buildFace();
}


void loop() {
  int timing = 500;

  display->clearScreen(); 
  display->setTextColor(display->color565(255, 0, 0));
  display->setCursor(4, 20);
  display->setTextSize(3);
  display->print("Hi!");
  digitalWrite(ledPin, HIGH);
  delay(timing);
  display->clearScreen();   
  display->setTextColor(display->color565(0, 255, 0));
  display->setCursor(4, 20);
  display->setTextSize(1);
  display->print("How are");
  display->setTextColor(display->color565(0, 0, 255));
  display->setCursor(4, 30);
  display->print("you?");
  digitalWrite(ledPin, LOW);
  delay(timing);
  // put your main code here, to run repeatedly:
  //sendBuffer(strawberry);                 // push it to the panel
  //delay(timing);
  //sendBuffer(pineapple);                 // push it to the panel
  //delay(timing);
  sendBuffer(face);                 // push it to the panel
  delay(timing);
  showMemory(display);                 // display heap usage stats
  delay(timing+500);

  // ---- Inner loop: hold on ASB-driven picture until "Unset" ----
  do {
    Serial.println("Entering ASB message loop...");
    Serial.flush();
    AsbMessage* msg = readAsbMessage();
    Serial.println("After readAsbMessage...");
    Serial.flush();
    if (msg != nullptr) {
      Serial.printf("Received ASB message: cmd=%s pic_len=%d\n",
                    msg->cmd.c_str(), msg->pic.length());
      Serial.flush();
      if (msg->cmd == "SetPic") {
        displayBase64Image(msg->pic);
        pictureSet = true;
      } else if (msg->cmd == "Unset") {
        pictureSet = false;
      }
      delete msg;
    }
    if (pictureSet) delay(1000);
  } while (pictureSet);

  /*
  display->begin();
  display->setBrightness8(255);         // 0–255; orig 90
  display->clearScreen();
  display->fillScreenRGB888(255,255,255);    // all pixels white
  delay(timing+5000);
  */
}

