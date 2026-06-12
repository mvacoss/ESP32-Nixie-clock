#include "time.h"
#include <WiFi.h>

// ==========================================
// 1. USER SETTINGS & TIMING VARIABLES
// ==========================================
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Automatic Summer/Winter Time Configuration
// "CET-1CEST,M3.5.0,M10.5.0/3" is the standard string for Central Europe (e.g., Germany, Czechia, Poland, etc.)
// - CET-1: Central European Time is UTC+1
// - CEST: Central European Summer Time
// - M3.5.0: Switches to Summer Time on the Last (5) Sunday (0) of March (3)
// - M10.5.0/3: Switches back to Winter Time on the Last (5) Sunday (0) of October (10) at 3:00 AM
const char* TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3"; 

// Display Timings (Milliseconds)
unsigned long PRIMARY_HOLD_TIME = 15000; // Time to stay on the main chosen mode (30s)
unsigned long SWEEP_HOLD_TIME   = 4000;  // Time to show each "peek" mode (2s)
unsigned long STARTUP_ANIM_SPD  = 70;   // Speed of the "Slot Machine" startup (200ms)

// System Intervals
unsigned long SYNC_INTERVAL = 6 * 60 * 60 * 1000; // NTP Sync every 6 hours

// Hardware Pins
#define LATCH_PIN 12 
#define CLOCK_PIN 13 
#define DATA_PIN 14  
#define HOURS_LED 5 
#define MINS_LED 17 
#define SECS_LED 26 
#define MODE_BTN 27 

// ==========================================
// 2. GLOBAL STATE VARIABLES
// ==========================================
enum DisplayMode { HOURS, MINUTES, SECONDS };
DisplayMode primaryMode   = HOURS; // Mode selected by the user
DisplayMode activeDisplay = HOURS; // Mode currently shown on tubes

unsigned long lastSyncTime      = 0;
unsigned long lastCycleStepTime = 0;
unsigned long lastAnimUpdate    = 0;
int cycleStep = 0; // 0=Primary, 1=Peek1, 2=Peek2
bool isSyncing = true;
int animDigit  = 0;

// ==========================================
// 3. CORE FUNCTIONS
// ==========================================

void displayDigits(byte leftDigit, byte rightDigit) {
  digitalWrite(LATCH_PIN, LOW);
  // Pack two 4-bit BCD digits into one 8-bit byte for the shift register
  byte shiftData = ((leftDigit & 0x0F) << 4) | (rightDigit & 0x0F);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, shiftData);
  digitalWrite(LATCH_PIN, HIGH);
}

void updateLEDs(DisplayMode mode) {
  digitalWrite(HOURS_LED, mode == HOURS);
  digitalWrite(MINS_LED, mode == MINUTES);
  digitalWrite(SECS_LED, mode == SECONDS);
}

void turnOffAllLEDs() {
  digitalWrite(HOURS_LED, LOW);
  digitalWrite(MINS_LED, LOW);
  digitalWrite(SECS_LED, LOW);
}

void updateDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  byte left = 0, right = 0;
  switch (activeDisplay) {
    case HOURS:   left = timeinfo.tm_hour / 10; right = timeinfo.tm_hour % 10; break;
    case MINUTES: left = timeinfo.tm_min / 10;  right = timeinfo.tm_min % 10;  break;
    case SECONDS: left = timeinfo.tm_sec / 10;  right = timeinfo.tm_sec % 10;  break;
  }
  displayDigits(left, right);
}

void connectAndSync() {
  Serial.println("\nWaking WiFi for sync...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for connection (max 15 seconds)
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TIME_ZONE, "pool.ntp.org");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println("Sync Successful. Shutting down WiFi.");
    }
  }
  
  // Turn off radio to save power and reduce interference
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void handleButton() {
  if (digitalRead(MODE_BTN) == LOW) {
    delay(50); // Debounce delay
    if (digitalRead(MODE_BTN) == LOW) {
      // Advance the primary mode immediately on press
      primaryMode = (DisplayMode)((primaryMode + 1) % 3);
      
      // Reset the cycling logic parameters instantly
      cycleStep = 0; 
      activeDisplay = primaryMode;
      lastCycleStepTime = millis();
      
      // Update both the LEDs AND the Nixie tubes immediately 
      updateLEDs(activeDisplay);
      updateDisplay(); 
      
      // Block loop progression without halting background updates while held
      while(digitalRead(MODE_BTN) == LOW) { 
        updateDisplay(); 
        yield(); 
      }
    }
  }
}

void handleCycling() {
  unsigned long now = millis();
  unsigned long waitTime = (cycleStep == 0) ? PRIMARY_HOLD_TIME : SWEEP_HOLD_TIME;

  if (now - lastCycleStepTime >= waitTime) {
    cycleStep = (cycleStep + 1) % 3;
    lastCycleStepTime = now;

    // Determine the next mode in the sequence relative to the user's primary choice
    if (cycleStep == 0) {
      activeDisplay = primaryMode;
    } else if (cycleStep == 1) {
      activeDisplay = (DisplayMode)((primaryMode + 1) % 3);
    } else {
      activeDisplay = (DisplayMode)((primaryMode + 2) % 3);
    }
    updateLEDs(activeDisplay);
  }
}

// ==========================================
// 4. MAIN SETUP & LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(HOURS_LED, OUTPUT);
  pinMode(MINS_LED, OUTPUT);
  pinMode(SECS_LED, OUTPUT);

  // Explicitly ensure all indicator LEDs are dark on boot
  turnOffAllLEDs();

  WiFi.begin(ssid, password);
  Serial.println("Connecting WiFi...");

  // STARTUP ANIMATION: Runs until the first successful sync
  while (isSyncing) {
    if (millis() - lastAnimUpdate > STARTUP_ANIM_SPD) {
      displayDigits(animDigit, animDigit);
      animDigit = (animDigit + 1) % 10;
      lastAnimUpdate = millis();
    }

    if (WiFi.status() == WL_CONNECTED) {
      configTzTime(TIME_ZONE, "pool.ntp.org");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        isSyncing = false;
        lastSyncTime = millis();
        lastCycleStepTime = millis();
        
        // Success! Turn off WiFi immediately
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println("\nInitial Sync Done. WiFi OFF.");
        
        // Only turn on the indicator LED now that time is valid and showing
        updateLEDs(activeDisplay);
      }
    }
    yield();
  }
}

void loop() {
  handleButton();   // Check for user input (Instant switch)
  handleCycling();  // Manage the 30s / 2s / 2s sequence
  updateDisplay();  // Update the tubes

  // Periodic Re-Sync Logic
  if (millis() - lastSyncTime > SYNC_INTERVAL) {
    connectAndSync();
    lastSyncTime = millis();
  }
  
  delay(50); // Stability delay
}
