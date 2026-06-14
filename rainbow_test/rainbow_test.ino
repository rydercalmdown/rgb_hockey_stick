// Minimal rainbow test — flash this to verify LEDs are working.
// ESP8266, WS2812B strip, 100 LEDs on GPIO 4 (D2).

#include <Adafruit_NeoPixel.h>

#define LED_PIN   4
#define LED_COUNT 100

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.setBrightness(80);
  strip.show();
}

void loop() {
  for (long hue = 0; hue < 5 * 65536L; hue += 256) {
    for (int i = 0; i < LED_COUNT; i++) {
      int pixelHue = hue + (i * 65536L / LED_COUNT);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(3);
  }
}
