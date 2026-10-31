#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);            // màn hình ngang
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.drawCentreString("Hello CYD!", 160, 100, 4);
}

void loop() {}