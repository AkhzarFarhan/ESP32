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

// Buffer for storing 1-SECOND AVERAGES (not raw readings)
// This ensures each second has equal weight in the final average
const int MAX_SECOND_SAMPLES = 10;               // Store 10 seconds of 1-second averages
uint16_t secondAvgBuffer[MAX_SECOND_SAMPLES];    // 1-second averages
uint16_t secondMinBuffer[MAX_SECOND_SAMPLES];    // 1-second minimums
uint16_t secondMaxBuffer[MAX_SECOND_SAMPLES];    // 1-second maximums
int secondBufferIndex = 0;

// Running 10-second ABSOLUTE min/max (updated every second, not averaged)
uint16_t running10sMin = 65535;
uint16_t running10sMax = 0;

// Current reading stats
uint16_t currentDistance = 0;
uint16_t lastValidDistance = 0;
bool lastReadingValid = false;

// 1-second accumulation for averaging
uint32_t secondSum = 0;
uint16_t secondCount = 0;
uint16_t lastSecondAvg = 0;
uint16_t secondMin = 65535;
uint16_t secondMax = 0;
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
void displayDistance(uint16_t distance_mm, uint16_t avg_dist, uint16_t min_dist, uint16_t max_dist, uint16_t min10s, uint16_t max10s, bool valid, bool wifiConnected, int bufferCount);
void displayError(const char* message);
void uploadToFirebaseAsync();

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
// MAIN LOOP: Fast sensor reading with 1-second averaging
// =================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // --- 1. READ SENSOR (as fast as possible) ---
    uint16_t distance = lox.readRangeContinuousMillimeters();
    
    if (!lox.timeoutOccurred() && distance < 8190) {
        lastReadingValid = true;
        currentDistance = distance;
        lastValidDistance = distance;
        
        // Accumulate for 1-second stats
        secondSum += distance;
        secondCount++;
        
        // Track min/max within this second
        if (distance < secondMin) secondMin = distance;
        if (distance > secondMax) secondMax = distance;
    } else {
        lastReadingValid = false;
    }
    
    // --- 2. STORE 1-SECOND AVERAGE TO BUFFER ---
    if (currentTime - lastSecondTime >= 1000) {
        if (secondCount > 0) {
            lastSecondAvg = secondSum / secondCount;
            
            // Store this second's stats in buffer (for Firebase)
            if (secondBufferIndex < MAX_SECOND_SAMPLES) {
                secondAvgBuffer[secondBufferIndex] = lastSecondAvg;
                secondMinBuffer[secondBufferIndex] = secondMin;
                secondMaxBuffer[secondBufferIndex] = secondMax;
                secondBufferIndex++;
            }
            
            // Update RUNNING 10-second ABSOLUTE min/max (not averaged!)
            if (secondMin < running10sMin) running10sMin = secondMin;
            if (secondMax > running10sMax) running10sMax = secondMax;
            
            Serial.print("1s Avg: ");
            Serial.print(lastSecondAvg);
            Serial.print(" mm | Min: ");
            Serial.print(secondMin);
            Serial.print(" | Max: ");
            Serial.print(secondMax);
            Serial.print(" | 10s Min: ");
            Serial.print(running10sMin);
            Serial.print(" | 10s Max: ");
            Serial.print(running10sMax);
            Serial.print(" (");
            Serial.print(secondCount);
            Serial.println(" readings)");
        }
        
        // Reset for next second
        secondSum = 0;
        secondCount = 0;
        secondMin = 65535;
        secondMax = 0;
        lastSecondTime = currentTime;
    }
    
    // --- 3. UPDATE DISPLAY (every 100ms for smooth updates) ---
    if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);
        // Pass current 1-second stats (use stored values if second just reset)
        uint16_t dispMin = (secondMin == 65535) ? lastSecondAvg : secondMin;
        uint16_t dispMax = (secondMax == 0) ? lastSecondAvg : secondMax;
        // Pass running 10-second ABSOLUTE min/max
        uint16_t disp10sMin = (running10sMin == 65535) ? dispMin : running10sMin;
        uint16_t disp10sMax = (running10sMax == 0) ? dispMax : running10sMax;
        displayDistance(currentDistance, lastSecondAvg, dispMin, dispMax, disp10sMin, disp10sMax, lastReadingValid, wifiConnected, secondBufferIndex);
        lastDisplayTime = currentTime;
    }
    
    // --- 4. UPLOAD TO FIREBASE (every 10 seconds, bulk upload) ---
    if (currentTime - lastFirebaseTime >= FIREBASE_INTERVAL) {
        if (secondBufferIndex > 0 && WiFi.status() == WL_CONNECTED) {
            Serial.print("Uploading ");
            Serial.print(secondBufferIndex);
            Serial.println(" second-averages to Firebase...");
            uploadToFirebaseAsync();
        }
        lastFirebaseTime = currentTime;
    }
    
    // Minimal delay - sensor timing budget handles the rest
    delay(10);
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
void displayDistance(uint16_t distance_mm, uint16_t avg_dist, uint16_t min_dist, uint16_t max_dist, uint16_t min10s, uint16_t max10s, bool valid, bool wifiConnected, int bufferCount) {
    display.clearDisplay();
    
    // Title with WiFi status
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("LIDAR ");
    display.print(wifiConnected ? "[OK]" : "[--]");
    display.print(" T-");
    display.print(10 - bufferCount);
    display.println("s");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
    
    if (valid) {
        // Display current distance in large font
        display.setTextSize(2);
        display.setCursor(0, 13);
        display.print(distance_mm);
        display.println(" mm");
        
        // Show 1-second stats: Avg | Min | Max
        display.setTextSize(1);
        display.setCursor(0, 32);
        display.print("1s A:");
        display.print(avg_dist);
        display.print(" m:");
        display.print(min_dist);
        display.print(" M:");
        display.println(max_dist);
        
        // Show 10-second ABSOLUTE min/max
        display.setCursor(0, 42);
        display.print("10s m:");
        display.print(min10s);
        display.print(" M:");
        display.println(max10s);
        
        // Show status bar (visual representation)
        int bar_width = map(constrain(distance_mm, 30, 2000), 30, 2000, 0, SCREEN_WIDTH - 4);
        display.drawRect(0, 52, SCREEN_WIDTH, 6, SSD1306_WHITE);
        display.fillRect(2, 54, bar_width, 2, SSD1306_WHITE);
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
// --- FIREBASE BULK UPLOAD (using 1-second averages) ---
// =================================================================
void uploadToFirebaseAsync() {
    if (WiFi.status() != WL_CONNECTED || secondBufferIndex == 0) {
        return;
    }
    
    unsigned long startTime = millis();
    
    // Calculate AVERAGE from 1-SECOND data
    uint32_t sum = 0;
    for (int i = 0; i < secondBufferIndex; i++) {
        sum += secondAvgBuffer[i];
    }
    uint16_t avgDist = sum / secondBufferIndex;
    
    // Use running ABSOLUTE min/max (NOT averaged!)
    // These track the true minimum and maximum across entire 10-second window
    uint16_t overallMin = running10sMin;
    uint16_t overallMax = running10sMax;
    
    HTTPClient http;
    http.setTimeout(5000);
    
    // --- 1. Update "latest" with comprehensive stats ---
    JsonDocument latestDoc;
    
    // Current reading (most recent 1-sec values)
    latestDoc["distance_mm"] = secondAvgBuffer[secondBufferIndex - 1];
    latestDoc["distance_cm"] = secondAvgBuffer[secondBufferIndex - 1] / 10.0;
    
    // 1-second stats (most recent second)
    JsonObject oneSecond = latestDoc["one_second"].to<JsonObject>();
    oneSecond["avg_mm"] = secondAvgBuffer[secondBufferIndex - 1];
    oneSecond["min_mm"] = secondMinBuffer[secondBufferIndex - 1];
    oneSecond["max_mm"] = secondMaxBuffer[secondBufferIndex - 1];
    
    // 10-second stats (all seconds combined)
    JsonObject tenSecond = latestDoc["ten_second"].to<JsonObject>();
    tenSecond["avg_mm"] = avgDist;
    tenSecond["min_mm"] = overallMin;
    tenSecond["max_mm"] = overallMax;
    tenSecond["seconds"] = secondBufferIndex;
    
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
    
    // --- 2. Add batch to history with ALL 1-second data ---
    JsonDocument batchDoc;
    batchDoc["timestamp"] = millis();
    batchDoc["seconds"] = secondBufferIndex;
    
    // 10-second summary
    batchDoc["avg_mm"] = avgDist;
    batchDoc["min_mm"] = overallMin;
    batchDoc["max_mm"] = overallMax;
    
    // All 1-second averages
    JsonArray avgArray = batchDoc["second_avgs"].to<JsonArray>();
    JsonArray minArray = batchDoc["second_mins"].to<JsonArray>();
    JsonArray maxArray = batchDoc["second_maxs"].to<JsonArray>();
    
    for (int i = 0; i < secondBufferIndex; i++) {
        avgArray.add(secondAvgBuffer[i]);
        minArray.add(secondMinBuffer[i]);
        maxArray.add(secondMaxBuffer[i]);
    }
    
    String batchPayload;
    serializeJson(batchDoc, batchPayload);
    
    String historyUrl = String(FIREBASE_HOST) + "/history.json?auth=" + String(FIREBASE_SECRET);
    http.begin(historyUrl);
    http.addHeader("Content-Type", "application/json");
    http.POST(batchPayload);
    http.end();
    
    // Clear buffer after successful upload
    secondBufferIndex = 0;
    
    // Reset running 10-second min/max for next window
    running10sMin = 65535;
    running10sMax = 0;
    
    unsigned long uploadTime = millis() - startTime;
    Serial.print("Firebase bulk upload completed in ");
    Serial.print(uploadTime);
    Serial.println("ms");
    
    lastFirebaseSuccess = millis();
}