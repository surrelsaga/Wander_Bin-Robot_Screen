#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

// --- Color Definitions (16-bit RGB565) ---
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0

// --- Guition JC3248W535 Hardware Setup ---
#define GFX_BL 1

// 1. Define the physical QSPI pins
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* cs */, 47 /* sck */, 21 /* d0 */, 48 /* d1 */, 40 /* d2 */, 39 /* d3 */);

// 2. Define the Screen Driver (IPS set to false to fix inverted colors)
Arduino_GFX *output_display = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, false /* IPS */, 320, 480);

// 3. Create the invisible memory Canvas (Keep original 320x480 memory size)
Arduino_GFX *gfx = new Arduino_Canvas(320 /* width */, 480 /* height */, output_display);

// --- Robot Network Details ---
const char* ssid = "WanderBin-Robot";
const char* password = "wanderbinpass";
const char* statusUrl = "http://192.168.4.1/status";

bool lastLidState = false;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 500;

void setup() {
  Serial.begin(115200);

  // Boot up the Canvas and Display
  if (!gfx->begin()) {
    Serial.println("Display init failed!");
  }

  // THE FIX: Rotate the software coordinate system 90 degrees (Landscape)
  gfx->setRotation(1);
  
  // Turn on the screen backlight
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  // Draw to the invisible canvas
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 50);
  gfx->println("Connecting to");
  gfx->setCursor(20, 90);
  gfx->println("Robot...");
  
  // THE FIX: Push the canvas to the physical screen!
  gfx->flush();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Draw success state to the invisible canvas
  gfx->fillScreen(BLACK);
  gfx->setCursor(20, 50);
  gfx->println("Connected!");
  gfx->setCursor(20, 90);
  gfx->println("Waiting for trash...");
  
  // THE FIX: Push to screen!
  gfx->flush();
}

void loop() {
  if (millis() - lastCheckTime >= checkInterval) {
    lastCheckTime = millis();

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(statusUrl);
      int httpCode = http.GET();

      if (httpCode == 200) {
        String payload = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          bool lidIsOpen = doc["lidIsOpen"];

          // Only redraw if state actually changed
          if (lidIsOpen != lastLidState) {
            Serial.println("--> STATE CHANGED! Redrawing screen...");
            lastLidState = lidIsOpen;
            
            // Wipe the invisible canvas clean
            gfx->fillScreen(BLACK); 

            if (lidIsOpen) {
              gfx->setTextColor(GREEN);
              gfx->setTextSize(4);
              gfx->setCursor(30, 100);
              gfx->println("HELLO WORLD!");
            } else {
              gfx->setTextColor(WHITE);
              gfx->setTextSize(3);
              gfx->setCursor(20, 100);
              gfx->println("Waiting for trash...");
            }
            
            // THE FIX: Shove the updated canvas to the physical screen!
            gfx->flush();
          }
        }
      }
      http.end();
    }
  }
}
