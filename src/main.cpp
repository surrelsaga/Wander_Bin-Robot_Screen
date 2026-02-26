#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

// --- Color Definitions (16-bit RGB565) ---
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0
#define RED   0xF800

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

// --- Global State Variables ---
String lastScannedItem = "";
bool lastLidIsOpen = false;
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 500;

// --- Robot UI Functions ---
void drawWaitingScreen() {
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(80, 150); // Centered on the landscape screen
  gfx->println("Waiting for trash...");
  gfx->flush(); // Push to screen!
}

// (Keep your drawHappyFace() and drawSadFace() functions here!)

// --- Robot Face Functions (Now with Text!) ---
void drawHappyFace(String itemName) {
  gfx->fillScreen(BLACK); // Wipe the canvas
  
  // Big Green Eyes
  gfx->fillCircle(160, 120, 35, GREEN); 
  gfx->fillCircle(320, 120, 35, GREEN); 
  
  // Happy Mouth 
  gfx->fillRoundRect(140, 200, 200, 60, 30, GREEN);
  
  // Draw the Scanned Item Name
  gfx->setTextColor(GREEN);
  gfx->setTextSize(3);
  gfx->setCursor(20, 280); // Placed at the bottom left
  gfx->println("Item: " + itemName); // Prints "Item: Plastic Bottle"

  gfx->flush(); // Push the drawing to the screen!
}

void drawSadFace(String itemName) {
  gfx->fillScreen(BLACK); // Wipe the canvas
  
  // Glaring Red Eyes
  gfx->fillCircle(160, 120, 35, RED);
  gfx->fillCircle(320, 120, 35, RED);
  
  // Sad/Flat Mouth 
  gfx->fillRect(140, 220, 200, 20, RED);
  
  // Draw the Scanned Item Name
  gfx->setTextColor(RED);
  gfx->setTextSize(3);
  gfx->setCursor(20, 280); // Placed at the bottom left
  gfx->println("Item: " + itemName); // Prints "Item: Styrofoam"

  gfx->flush(); // Push the drawing to the screen!
}

void setup() {
  Serial.begin(115200);

  // Boot up the Canvas and Display
  if (!gfx->begin()) {
    Serial.println("Display init failed!");
  }

  // Rotate the software coordinate system 90 degrees (Landscape)
  gfx->setRotation(1);
  
  // Turn on the screen backlight
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  // Draw connecting state
  gfx->fillScreen(BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 50);
  gfx->println("Connecting to");
  gfx->setCursor(20, 90);
  gfx->println("Robot...");
  gfx->flush();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // THE FIX: Use the new helper function instead of manual text!
  drawWaitingScreen();
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

        // Fixed the duplicate if(!error) here!
        if (!error) {
          // Grab all three pieces of data from the Controller
          bool allowOpen = doc["lidAllowOpen"];
          bool isOpen = doc["lidIsOpen"];
          String currentItem = doc["lastItem"] | ""; // Fallback to empty string if missing

          // --- TRIGGER 1: A New Item was Scanned ---
          if (currentItem != lastScannedItem && currentItem != "") {
            lastScannedItem = currentItem; // Save it so we don't trigger twice

            if (allowOpen == false) {
              // Non-recyclable scanned! Pass the item name to the sad face.
              Serial.println("--> NON-RECYCLABLE SCANNED! Sad face...");
              drawSadFace(currentItem); 
              
              // Hold the sad face for 3 seconds, then automatically reset
              delay(3000); 
              drawWaitingScreen(); 
            }
          }

          // --- TRIGGER 2: The physical lid moved ---
          if (isOpen != lastLidIsOpen) {
            lastLidIsOpen = isOpen;

            if (isOpen && allowOpen) {
              // Lid opened! Pass the SAVED item name to the happy face.
              Serial.println("--> LID OPENED! Happy face...");
              drawHappyFace(lastScannedItem); 
            } else if (!isOpen) {
              // The servo just closed the lid!
              Serial.println("--> LID CLOSED! Resetting...");
              drawWaitingScreen();
            }
          }
        }
      }
      http.end();
    }
  }
}
