#include "time.h"
#include <WiFi.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// Pin definitions
#define LATCH_PIN 12  // ST_CP of 74HC595
#define CLOCK_PIN 13  // SH_CP of 74HC595
#define DATA_PIN 14    // DS of 74HC595
#define HOURS_LED 5   // LED for hours mode
#define MINS_LED 17   // LED for minutes mode
#define SECS_LED 26   // LED for seconds mode
#define MODE_BTN 27   // Mode button pin

// Time constants
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;  // Change this based on your timezone
const int daylightOffset_sec = 0;

// Display mode enumeration
enum DisplayMode {
HOURS,
MINUTES,
SECONDS
};

// Global variables
DisplayMode currentMode = HOURS;
bool btnPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Auto-cycling variables
unsigned long lastCycleTime = 0;
const unsigned long cycleInterval = 30000; // 30 seconds in milliseconds
void setupWiFi() {
Serial.print("Connecting to WiFi");
WiFi.begin(ssid, password);
int timeout = 0;
while (WiFi.status() != WL_CONNECTED && timeout < 20) {
delay(500);
Serial.print(".");
timeout++;
}
if (WiFi.status() == WL_CONNECTED) {
Serial.println("\nWiFi connected");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());
} else {
Serial.println("\nWiFi connection failed!");
}
}
void setup() {
Serial.begin(115200);
Serial.println("Nixie Clock Starting...");

// Initialize pins
pinMode(LATCH_PIN, OUTPUT);
pinMode(CLOCK_PIN, OUTPUT);
pinMode(DATA_PIN, OUTPUT);
pinMode(MODE_BTN, INPUT_PULLUP);
pinMode(HOURS_LED, OUTPUT);
pinMode(MINS_LED, OUTPUT);
pinMode(SECS_LED, OUTPUT);

// Set initial LED states
updateModeLEDs();

// Initialize auto-cycle timer
lastCycleTime = millis();

// Connect to WiFi and get time
setupWiFi();
setupTime();

// Disconnect WiFi to save power
WiFi.disconnect(true);
WiFi.mode(WIFI_OFF);
updateDisplay();
}
void setupTime() {
if (WiFi.status() == WL_CONNECTED) {
configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
struct tm timeinfo;
int attempts = 0;
while(!getLocalTime(&timeinfo) && attempts < 10) {
  Serial.println("Failed to obtain time... retrying");
  delay(500);
  attempts++;
}

if (attempts < 10) {
  Serial.println("Time set successfully");
} else {
  Serial.println("Failed to set time from NTP");
}
} else {
Serial.println("No WiFi connection - using default time");
}
}
void updateModeLEDs() {
digitalWrite(HOURS_LED, currentMode == HOURS);
digitalWrite(MINS_LED, currentMode == MINUTES);
digitalWrite(SECS_LED, currentMode == SECONDS);
}
void displayDigits(byte leftDigit, byte rightDigit) {
digitalWrite(LATCH_PIN, LOW);
leftDigit &= 0x0F;
rightDigit &= 0x0F;
byte shiftData = (leftDigit << 4) | rightDigit;
shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, shiftData);
digitalWrite(LATCH_PIN, HIGH);
}
void cycleMode() {
// Cycle through modes: HOURS -> MINUTES -> SECONDS -> HOURS
switch(currentMode) {
case HOURS:
currentMode = MINUTES;
break;
case MINUTES:
currentMode = SECONDS;
break;
case SECONDS:
currentMode = HOURS;
break;
}
updateModeLEDs();
Serial.print("Auto-cycled to: ");
Serial.println(currentMode == HOURS ? "Hours" : (currentMode == MINUTES ? "Minutes" : "Seconds"));
}
void handleAutoCycle() {
unsigned long currentTime = millis();

// Check if 30 seconds have passed
if (currentTime - lastCycleTime >= cycleInterval) {
cycleMode();
lastCycleTime = currentTime; // Reset the timer
}
}
void handleModeButton() {
bool reading = digitalRead(MODE_BTN);
unsigned long currentTime = millis();
if ((currentTime - lastDebounceTime) > debounceDelay) {
if (!reading && !btnPressed) {

// Manual button press - cycle mode
cycleMode();
  // Reset auto-cycle timer when button is pressed
  lastCycleTime = currentTime;
  
  btnPressed = true;
  lastDebounceTime = currentTime;
}
else if (reading && btnPressed) {
  btnPressed = false;
  lastDebounceTime = currentTime;
}
}
}
void updateDisplay() {
struct tm timeinfo;

// Initialize digits to 0 in case of error
byte leftDigit = 0;
byte rightDigit = 0;
if(!getLocalTime(&timeinfo)) {
displayDigits(0, 0);  // Display 00 in case of error
return;
}
// Set digits based on current mode
switch(currentMode) {
case HOURS:
leftDigit = timeinfo.tm_hour / 10;
rightDigit = timeinfo.tm_hour % 10;
break;
case MINUTES:
  leftDigit = timeinfo.tm_min / 10;
  rightDigit = timeinfo.tm_min % 10;
  break;
  
case SECONDS:
  leftDigit = timeinfo.tm_sec / 10;
  rightDigit = timeinfo.tm_sec % 10;
  break;
  
default:
  // In case of invalid mode, show 00
  leftDigit = 0;
  rightDigit = 0;
  break;
}
displayDigits(leftDigit, rightDigit);
}
void loop() {
handleAutoCycle();  // Check if it's time to auto-cycle
handleModeButton(); // Handle manual button press
updateDisplay();    // Update the display
delay(50);
}