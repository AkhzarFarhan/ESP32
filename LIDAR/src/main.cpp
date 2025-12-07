#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <VL53L0X.h>

// =================================================================
// --- OLED Display Configuration (from KeepItUp reference) ---
// =================================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =================================================================
// --- WiFi & Firebase Configuration (from KeepItUp reference) ---
// =================================================================
const char* ssid = "JioFiber_401_2.4Gz";         // Your WiFi network name
const char* password = "Melvin420";              // Your WiFi password

// Firebase Configuration
const char* FIREBASE_HOST = "https://openware-ai-default-rtdb.firebaseio.com/ESP32/LIDAR";
const char* FIREBASE_SECRET = "YOUR_DATABASE_SECRET"; // Your Firebase DB Secret

// =================================================================
// --- VL53L0X Laser Distance Sensor ---
// =================================================================
VL53L0X lox;

// =================================================================
// --- Averaging Variables ---
// =================================================================
unsigned long lastLogTime = 0;
const unsigned long LOG_INTERVAL = 1000; // Log to Firebase every 1 second
uint32_t distanceSum = 0;
uint16_t readingCount = 0;
uint16_t lastAvgDistance = 0;

// =================================================================
// --- Forward Declarations ---
// =================================================================
void initOLED();
void initVL53L0X();
void connectToWiFi();
void displayDistance(uint16_t distance_mm, uint16_t avg_distance, bool valid, bool wifiConnected);
void displayError(const char* message);
void logToFirebase(uint16_t avg_distance);

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("ESP32 VL53L0X LIDAR Distance Sensor");
    Serial.println("====================================");
    
    // Initialize I2C with explicit pins (SDA=21, SCL=22 for ESP32)
    Wire.begin(21, 22);
    
    // Initialize OLED Display
    initOLED();
    
    // Connect to WiFi
    connectToWiFi();
    
    // Initialize VL53L0X Sensor
    initVL53L0X();
    
    lastLogTime = millis();
}

// =================================================================
// MAIN LOOP: Continuously read and display distance
// =================================================================
void loop() {
    uint16_t distance = lox.readRangeContinuousMillimeters();
    bool validReading = false;
    
    if (!lox.timeoutOccurred()) {
        // Valid reading if distance < 8190 (which indicates out of range)
        if (distance < 8190) {
            validReading = true;
            
            // Accumulate for averaging
            distanceSum += distance;
            readingCount++;
            
            // Print to Serial
            Serial.print("Distance: ");
            Serial.print(distance);
            Serial.println(" mm");
        } else {
            Serial.println("Out of range!");
        }
    } else {
        Serial.println("Sensor timeout!");
    }
    
    // Check if 1 second has passed - time to log to Firebase
    unsigned long currentTime = millis();
    if (currentTime - lastLogTime >= LOG_INTERVAL) {
        if (readingCount > 0) {
            lastAvgDistance = distanceSum / readingCount;
            
            Serial.print("Average Distance (1s): ");
            Serial.print(lastAvgDistance);
            Serial.print(" mm (");
            Serial.print(readingCount);
            Serial.println(" readings)");
            
            // Log to Firebase
            logToFirebase(lastAvgDistance);
        }
        
        // Reset for next interval
        distanceSum = 0;
        readingCount = 0;
        lastLogTime = currentTime;
    }
    
    // Display current reading and last average
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    displayDistance(distance, lastAvgDistance, validReading, wifiConnected);
    
    delay(50); // Small delay between readings
}

// =================================================================
// --- INITIALIZATION FUNCTIONS ---
// =================================================================
void initOLED() {
    Serial.println("Initializing OLED display...");
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed!"));
        for(;;); // Don't proceed, loop forever
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("LIDAR Sensor");
    display.println("Starting...");
    display.display();
    
    Serial.println("OLED initialized successfully!");
    delay(1000);
}

void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting to WiFi...");
    display.println(ssid);
    display.display();
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        display.print(".");
        display.display();
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("WiFi Connected!");
        display.print("IP: ");
        display.println(WiFi.localIP());
        display.display();
    } else {
        Serial.println("\nWiFi Failed! Running offline.");
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("WiFi Failed!");
        display.println("Running offline...");
        display.display();
    }
    delay(1500);
}

void initVL53L0X() {
    Serial.println("Initializing VL53L0X sensor...");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Init VL53L0X...");
    display.display();
    
    lox.setTimeout(500);
    if (!lox.init()) {
        Serial.println(F("Failed to boot VL53L0X sensor!"));
        displayError("VL53L0X FAILED!");
        for(;;); // Don't proceed, loop forever
    }
    
    // =================================================================
    // LONG RANGE MODE CONFIGURATION (up to 2 meters)
    // =================================================================
    // Lower the return signal rate limit (default is 0.25 MCPS)
    lox.setSignalRateLimit(0.1);
    
    // Increase laser pulse periods (default is 14 and 10 PCLKs)
    lox.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    lox.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    
    // Increase timing budget for better accuracy at long range (default ~33ms)
    lox.setMeasurementTimingBudget(200000); // 200ms for better long-range accuracy
    
    Serial.println("Long range mode enabled (up to 2m)");
    
    // Start continuous ranging measurements
    lox.startContinuous();
    
    Serial.println("VL53L0X initialized successfully!");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("VL53L0X Ready!");
    display.println("Long Range Mode");
    display.display();
    delay(1000);
}

// =================================================================
// --- DISPLAY FUNCTIONS ---
// =================================================================
void displayDistance(uint16_t distance_mm, uint16_t avg_distance, bool valid, bool wifiConnected) {
    display.clearDisplay();
    
    // Title with WiFi status
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("LIDAR Sensor ");
    display.println(wifiConnected ? "[WiFi]" : "[OFF]");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
    
    if (valid) {
        // Display current distance in large font
        display.setTextSize(2);
        display.setCursor(0, 14);
        display.print(distance_mm);
        display.println(" mm");
        
        // Show average distance
        display.setTextSize(1);
        display.setCursor(0, 35);
        display.print("Avg: ");
        display.print(avg_distance);
        display.print(" mm (");
        display.print(avg_distance / 10.0, 1);
        display.println(" cm)");
        
        // Show status bar (visual representation)
        // VL53L0X range is typically 30-2000mm
        int bar_width = map(constrain(distance_mm, 30, 2000), 30, 2000, 0, SCREEN_WIDTH - 4);
        display.drawRect(0, 48, SCREEN_WIDTH, 8, SSD1306_WHITE);
        display.fillRect(2, 50, bar_width, 4, SSD1306_WHITE);
        
        // Firebase status
        display.setCursor(0, 58);
        display.print("Firebase: Logging...");
    } else {
        // Out of range message
        display.setTextSize(2);
        display.setCursor(10, 20);
        display.println("OUT OF");
        display.setCursor(10, 38);
        display.println("RANGE");
    }
    
    display.display();
}

void displayError(const char* message) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ERROR:");
    display.println(message);
    display.display();
}

// =================================================================
// --- FIREBASE LOGGING ---
// =================================================================
void logToFirebase(uint16_t avg_distance) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected - skipping Firebase log");
        return;
    }
    
    HTTPClient http;
    
    // Create JSON payload
    JsonDocument doc;
    doc["distance_mm"] = avg_distance;
    doc["distance_cm"] = avg_distance / 10.0;
    doc["timestamp"] = millis();
    
    String payload;
    serializeJson(doc, payload);
    
    // Send to Firebase
    String url = String(FIREBASE_HOST) + "/latest.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.PUT(payload);
    
    if (httpCode > 0) {
        Serial.print("Firebase update: ");
        Serial.println(httpCode == 200 ? "Success" : String(httpCode));
    } else {
        Serial.print("Firebase error: ");
        Serial.println(http.errorToString(httpCode));
    }
    
    http.end();
    
    // Also log to history with timestamp
    String historyUrl = String(FIREBASE_HOST) + "/history.json?auth=" + String(FIREBASE_SECRET);
    http.begin(historyUrl);
    http.addHeader("Content-Type", "application/json");
    http.POST(payload); // POST adds a new entry with unique key
    http.end();
}