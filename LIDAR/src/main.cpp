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
// --- Timing & Buffering Configuration ---
// =================================================================
// Display update interval (real-time feel)
const unsigned long DISPLAY_INTERVAL = 100;      // Update display every 100ms
unsigned long lastDisplayTime = 0;

// Firebase bulk upload interval
const unsigned long FIREBASE_INTERVAL = 10000;   // Upload to Firebase every 10 seconds
unsigned long lastFirebaseTime = 0;

// Buffer for storing readings (10 seconds worth of data)
const int MAX_BUFFER_SIZE = 100;                 // Store up to 100 readings
uint16_t distanceBuffer[MAX_BUFFER_SIZE];
unsigned long timestampBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

// Current reading stats
uint16_t currentDistance = 0;
uint16_t lastValidDistance = 0;
bool lastReadingValid = false;

// 1-second averaging for display
uint32_t secondSum = 0;
uint16_t secondCount = 0;
uint16_t lastSecondAvg = 0;
unsigned long lastSecondTime = 0;

// Firebase upload status
bool firebaseUploading = false;
unsigned long lastFirebaseSuccess = 0;

// =================================================================
// --- Forward Declarations ---
// =================================================================
void initOLED();
void initVL53L0X();
void connectToWiFi();
void displayDistance(uint16_t distance_mm, uint16_t avg_distance, bool valid, bool wifiConnected, int bufferCount);
void displayError(const char* message);
void uploadToFirebaseAsync();
void addToBuffer(uint16_t distance);

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("ESP32 VL53L0X LIDAR Distance Sensor");
    Serial.println("====================================");
    Serial.println("Optimized with buffered Firebase uploads");
    
    // Initialize I2C with explicit pins (SDA=21, SCL=22 for ESP32)
    Wire.begin(21, 22);
    
    // Initialize OLED Display
    initOLED();
    
    // Connect to WiFi
    connectToWiFi();
    
    // Initialize VL53L0X Sensor
    initVL53L0X();
    
    unsigned long now = millis();
    lastDisplayTime = now;
    lastFirebaseTime = now;
    lastSecondTime = now;
}

// =================================================================
// MAIN LOOP: Fast sensor reading with buffered Firebase uploads
// =================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // --- 1. READ SENSOR (as fast as possible) ---
    uint16_t distance = lox.readRangeContinuousMillimeters();
    
    if (!lox.timeoutOccurred() && distance < 8190) {
        lastReadingValid = true;
        currentDistance = distance;
        lastValidDistance = distance;
        
        // Accumulate for 1-second average (for display)
        secondSum += distance;
        secondCount++;
        
        // Add to buffer for Firebase
        addToBuffer(distance);
    } else {
        lastReadingValid = false;
    }
    
    // --- 2. UPDATE 1-SECOND AVERAGE (for display) ---
    if (currentTime - lastSecondTime >= 1000) {
        if (secondCount > 0) {
            lastSecondAvg = secondSum / secondCount;
            Serial.print("1s Avg: ");
            Serial.print(lastSecondAvg);
            Serial.print(" mm (");
            Serial.print(secondCount);
            Serial.println(" readings)");
        }
        secondSum = 0;
        secondCount = 0;
        lastSecondTime = currentTime;
    }
    
    // --- 3. UPDATE DISPLAY (every 100ms for smooth updates) ---
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        displayDistance(currentDistance, lastSecondAvg, lastReadingValid, wifiConnected, bufferIndex);
        lastDisplayTime = currentTime;
    }
    
    // --- 4. UPLOAD TO FIREBASE (every 10 seconds, bulk upload) ---
    if (currentTime - lastFirebaseTime >= FIREBASE_INTERVAL) {
        if (bufferIndex > 0 && WiFi.status() == WL_CONNECTED) {
            Serial.print("Uploading ");
            Serial.print(bufferIndex);
            Serial.println(" readings to Firebase...");
            uploadToFirebaseAsync();
        }
        lastFirebaseTime = currentTime;
    }
    
    // Minimal delay - sensor timing budget handles the rest
    delay(10);
}

// =================================================================
// --- BUFFER MANAGEMENT ---
// =================================================================
void addToBuffer(uint16_t distance) {
    if (bufferIndex < MAX_BUFFER_SIZE) {
        distanceBuffer[bufferIndex] = distance;
        timestampBuffer[bufferIndex] = millis();
        bufferIndex++;
    }
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
    // HIGH SPEED + LONG RANGE MODE (balanced for responsiveness)
    // =================================================================
    // Lower the return signal rate limit for long range detection
    lox.setSignalRateLimit(0.1);
    
    // Increase laser pulse periods for better long-range detection
    lox.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    lox.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    
    // FASTER timing budget: 50ms instead of 200ms
    // Good balance between speed (~20 readings/sec) and accuracy
    lox.setMeasurementTimingBudget(50000); // 50ms for faster readings
    
    Serial.println("High-speed long range mode enabled");
    
    // Start continuous ranging measurements
    lox.startContinuous();
    
    Serial.println("VL53L0X initialized successfully!");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("VL53L0X Ready!");
    display.println("Fast + Long Range");
    display.display();
    delay(1000);
}

// =================================================================
// --- DISPLAY FUNCTIONS ---
// =================================================================
void displayDistance(uint16_t distance_mm, uint16_t avg_distance, bool valid, bool wifiConnected, int bufferCount) {
    display.clearDisplay();
    
    // Title with WiFi status
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("LIDAR ");
    display.print(wifiConnected ? "[WiFi]" : "[OFF]");
    display.print(" Buf:");
    display.println(bufferCount);
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
        int bar_width = map(constrain(distance_mm, 30, 2000), 30, 2000, 0, SCREEN_WIDTH - 4);
        display.drawRect(0, 48, SCREEN_WIDTH, 8, SSD1306_WHITE);
        display.fillRect(2, 50, bar_width, 4, SSD1306_WHITE);
        
        // Upload progress (buffer fills every 10 seconds)
        display.setCursor(0, 58);
        int progress = (bufferCount * 100) / MAX_BUFFER_SIZE;
        display.print("Next upload: ");
        display.print(progress);
        display.println("%");
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
// --- FIREBASE BULK UPLOAD ---
// =================================================================
void uploadToFirebaseAsync() {
    if (WiFi.status() != WL_CONNECTED || bufferIndex == 0) {
        return;
    }
    
    unsigned long startTime = millis();
    
    // Calculate statistics from buffer
    uint32_t sum = 0;
    uint16_t minDist = 65535;
    uint16_t maxDist = 0;
    
    for (int i = 0; i < bufferIndex; i++) {
        sum += distanceBuffer[i];
        if (distanceBuffer[i] < minDist) minDist = distanceBuffer[i];
        if (distanceBuffer[i] > maxDist) maxDist = distanceBuffer[i];
    }
    
    uint16_t avgDist = sum / bufferIndex;
    
    HTTPClient http;
    http.setTimeout(5000); // 5 second timeout
    
    // --- 1. Update "latest" with current stats ---
    JsonDocument latestDoc;
    latestDoc["distance_mm"] = distanceBuffer[bufferIndex - 1]; // Most recent
    latestDoc["distance_cm"] = distanceBuffer[bufferIndex - 1] / 10.0;
    latestDoc["avg_mm"] = avgDist;
    latestDoc["min_mm"] = minDist;
    latestDoc["max_mm"] = maxDist;
    latestDoc["readings"] = bufferIndex;
    latestDoc["timestamp"] = millis();
    
    String latestPayload;
    serializeJson(latestDoc, latestPayload);
    
    String url = String(FIREBASE_HOST) + "/latest.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(latestPayload);
    http.end();
    
    if (httpCode == 200) {
        Serial.println("Latest data uploaded successfully");
    } else {
        Serial.print("Latest upload failed: ");
        Serial.println(httpCode);
    }
    
    // --- 2. Add batch to history ---
    JsonDocument batchDoc;
    batchDoc["timestamp"] = millis();
    batchDoc["count"] = bufferIndex;
    batchDoc["avg_mm"] = avgDist;
    batchDoc["min_mm"] = minDist;
    batchDoc["max_mm"] = maxDist;
    
    // Add sample of readings (first, middle, last to save space)
    JsonArray samples = batchDoc["samples"].to<JsonArray>();
    samples.add(distanceBuffer[0]);
    samples.add(distanceBuffer[bufferIndex / 2]);
    samples.add(distanceBuffer[bufferIndex - 1]);
    
    String batchPayload;
    serializeJson(batchDoc, batchPayload);
    
    String historyUrl = String(FIREBASE_HOST) + "/history.json?auth=" + String(FIREBASE_SECRET);
    http.begin(historyUrl);
    http.addHeader("Content-Type", "application/json");
    http.POST(batchPayload);
    http.end();
    
    // Clear buffer after successful upload
    bufferIndex = 0;
    
    unsigned long uploadTime = millis() - startTime;
    Serial.print("Firebase bulk upload completed in ");
    Serial.print(uploadTime);
    Serial.println("ms");
    
    lastFirebaseSuccess = millis();
}