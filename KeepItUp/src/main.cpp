#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =================================================================
// --- CONFIGURATION: PLEASE UPDATE THESE VALUES ---
// =================================================================
const char* ssid = "JioFiber_401_2.4Gz";         // Your WiFi network name
const char* password = "Melvin420"; // Your WiFi password
const char* SERVER_IP = "192.168.29.138";      // The IP of your PC or Termux tablet

// --- FIREBASE CONFIG ---
const char* FIREBASE_HOST = "https://openware-ai-default-rtdb.firebaseio.com/ESP32/"; // Your Firebase DB URL
const char* FIREBASE_SECRET = "YOUR_DATABASE_SECRET"; // Your Firebase DB Secret

// =================================================================
// --- RL & MODEL PARAMETERS ---
// =================================================================
const float ALPHA = 0.005;           // Learning Rate
const float GAMMA = 0.98;            // Discount Factor
float epsilon = 1.0;                 // Current exploration rate
const float EPSILON_DECAY = 0.9998;  // Rate at which epsilon decreases
const int NUM_EPISODES = 50000;      // Total training episodes
int current_episode = 1;             // The episode to start from, will be loaded from Firebase

const int NUM_ACTIONS = 5; // 0:Nothing, 1:Up, 2:Down, 3:Left, 4:Right
const int NUM_FEATURES = 8;

// Local cache for the weights. This gets synced with Firebase.
float weights[NUM_ACTIONS][NUM_FEATURES];

// =================================================================
// --- FORWARD DECLARATIONS ---
// =================================================================
void updateDisplayStats(int episode, int steps, float epsilon, const char* status_override = nullptr);
float getQValue(float* features_in, int action);
float getMaxQValue(float* features_in);
int chooseAction(float* features_in);
String httpGETRequest(const char* url);
void updateAllWeightsInFirebase();
bool getWeightsFromFirebase();
void getFeatures(JsonDocument& doc, float* features_out);
void connectToWiFi();
void runEpisode(int episode);
bool getStatsFromFirebase();
void updateStatsInFirebase(int episode, float epsilon);
void updateStateInFirebase(String& jsonPayload, bool is_reset, int last_action = -1);

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup()
{
    Serial.begin(115200);
    randomSeed(analogRead(0));

    // Initialize I2C with explicit pins
    Wire.begin(21, 22); // SDA=21, SCL=22
    
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed")); for(;;);
    }
    
    // Clear display and show initialization message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("ESP32 Starting...");
    display.display();
    delay(1000);

    connectToWiFi();
    
    // --- RESUME LOGIC ---
    updateDisplayStats(0, 0, 0, "Resuming...");
    
    // 1. Try to load previous training progress (episode, epsilon)
    if (getStatsFromFirebase())
    {
        Serial.println("Successfully loaded training stats from Firebase.");
    }
    else
    {
        Serial.println("No stats found. Starting new training session.");
        current_episode = 1;
        epsilon = 1.0;
    }
    
    // 2. Try to load the learned weights
    if (getWeightsFromFirebase())
    {
        Serial.println("Successfully loaded learned weights from Firebase.");
    }
    else
    {
        Serial.println("No weights found. Initializing to zero.");
        for (int i = 0; i < NUM_ACTIONS; ++i)
        {
            for (int j = 0; j < NUM_FEATURES; ++j)
            {
                weights[i][j] = 0.0;
            }
        }
    }
    delay(1000); // Reduced delay from 2000ms
}

// =================================================================
// MAIN LOOP: Contains the training logic
// =================================================================
void loop()
{
    // Test if server is reachable first
    String testUrl = "http://" + String(SERVER_IP) + ":5000/reset";
    String testResponse = httpGETRequest(testUrl.c_str());
    
    if (testResponse == "") {
        // Server not reachable - run in test mode
        Serial.println("Server not reachable - running in display test mode");
        for (int test_episode = 1; test_episode <= 100; ++test_episode) {
            for (int test_steps = 0; test_steps <= 50; test_steps += 5) {
                updateDisplayStats(test_episode, test_steps, epsilon, "Test Mode");
                delay(100); // Update every 100ms for faster display
                epsilon *= 0.999; // Simulate epsilon decay
                if (epsilon < 0.05) epsilon = 0.05;
            }
        }
        updateDisplayStats(100, 50, epsilon, "Test Complete");
        delay(5000); // Reduced delay
        return;
    }
    
    // Normal training mode
    for (int episode = current_episode; episode <= NUM_EPISODES; ++episode)
    {
        // Sync local knowledge with the master copy before starting
        getWeightsFromFirebase();

        runEpisode(episode);

        // Push all accumulated changes to Firebase ONCE per episode for speed
        updateAllWeightsInFirebase();

        // Decay epsilon for the next episode
        epsilon *= EPSILON_DECAY;
        if (epsilon < 0.05) epsilon = 0.05;
        
        // Save our progress so we can resume if rebooted
        updateStatsInFirebase(episode + 1, epsilon);
    }

    updateDisplayStats(NUM_EPISODES, 0, 0, "TRAINING FINISHED");
    while(true) { delay(10000); }
}

// =================================================================
// --- CORE RL & SIMULATION LOGIC ---
// =================================================================
void runEpisode(int episode)
{
    // Update display at start of episode
    updateDisplayStats(episode, 0, epsilon, "Starting episode...");
    
    // 1. Reset the environment
    String url = "http://" + String(SERVER_IP) + ":5000/reset";
    String payload = httpGETRequest(url.c_str());
    if (payload == "") {
        Serial.println("Failed to reset environment - server not responding");
        updateDisplayStats(episode, 0, epsilon, "Server offline");
        delay(1000); // Reduced delay from 5 seconds to 1 second
        return;
    }
    
    // Push the full initial state (with world layout) to Firebase for the visualizer
    updateStateInFirebase(payload, true);

    JsonDocument state_doc;
    deserializeJson(state_doc, payload);

    bool done = false;
    int steps = 0;
    float features[NUM_FEATURES];

    while (!done)
    {
        // 2. Extract features from the current state
        getFeatures(state_doc, features);

        // 3. Choose an action
        int action = chooseAction(features);

        // 4. Perform the action in the environment
        url = "http://" + String(SERVER_IP) + ":5000/step?action=" + String(action);
        payload = httpGETRequest(url.c_str());
        if (payload == "") {
            Serial.println("Failed to perform step - server not responding");
            updateDisplayStats(episode, steps, epsilon, "Connection lost");
            break;
        }
        
        // Push step state (drone/mission info) to Firebase for the visualizer
        updateStateInFirebase(payload, false, action);

        deserializeJson(state_doc, payload);
        float reward = state_doc["reward"];
        done = state_doc["done"];

        // 5. Calculate Q-values for learning
        float current_q = getQValue(features, action);
        float max_q_next_state = 0;
        if (!done)
        {
            float next_features[NUM_FEATURES];
            getFeatures(state_doc, next_features);
            max_q_next_state = getMaxQValue(next_features);
        }

        // 6. Update the LOCAL weights cache
        float error = (reward + GAMMA * max_q_next_state) - current_q;
        for (int i = 0; i < NUM_FEATURES; ++i)
        {
            weights[action][i] += ALPHA * error * features[i];
        }
        
        steps++;
        
        // Update display every 5 steps to show progress (more frequent)
        if (steps % 5 == 0) {
            updateDisplayStats(episode, steps, epsilon);
        }
        
        if (steps > 800) done = true; // Timeout
    }
    updateDisplayStats(episode, steps, epsilon);
}

void getFeatures(JsonDocument& doc, float* features_out)
{
    float x = doc["x"], y = doc["y"], vx = doc["vx"], vy = doc["vy"];
    float target_x = doc["target_x"], target_y = doc["target_y"];

    features_out[0] = (x - target_x) / 400.0;
    features_out[1] = (y - target_y) / 300.0;
    features_out[2] = vx / 5.0;
    features_out[3] = vy / 5.0;
    features_out[4] = y / 300.0;
    features_out[5] = (float)doc["has_package"];
    features_out[6] = (float)doc["battery"] / 600.0;
    features_out[7] = 1.0; // Bias term
}

float getQValue(float* features_in, int action)
{
    float q = 0.0;
    for (int i = 0; i < NUM_FEATURES; ++i) q += weights[action][i] * features_in[i];
    return q;
}

float getMaxQValue(float* features_in)
{
    float max_q = -1e9;
    for (int i = 0; i < NUM_ACTIONS; ++i) max_q = max(max_q, getQValue(features_in, i));
    return max_q;
}

int chooseAction(float* features_in)
{
    if ((float)random(1000)/1000.0 < epsilon)
    {
        return random(0, NUM_ACTIONS);
    }
    else
    {
        int best_action = 0;
        float max_q = -1e9;
        for (int i = 0; i < NUM_ACTIONS; ++i)
        {
            float q = getQValue(features_in, i);
            if (q > max_q)
            {
                max_q = q;
                best_action = i;
            }
        }
        return best_action;
    }
}

// =================================================================
// --- FIREBASE COMMUNICATION ---
// =================================================================
bool getWeightsFromFirebase()
{
    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/drone_weights.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode != 200) { http.end(); return false; }

    JsonDocument doc;
    deserializeJson(doc, http.getString());
    if (doc.isNull()) { http.end(); return false; }

    for (int i = 0; i < NUM_ACTIONS; ++i)
    {
        for (int j = 0; j < NUM_FEATURES; ++j)
        {
            weights[i][j] = doc[i][j];
        }
    }
    http.end();
    return true;
}

bool getStatsFromFirebase()
{
    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/training_stats.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode != 200) { http.end(); return false; }

    JsonDocument doc;
    deserializeJson(doc, http.getString());
    if (doc.isNull()) { http.end(); return false; }

    current_episode = doc["episode"];
    epsilon = doc["epsilon"];
    http.end();
    return true;
}

void updateAllWeightsInFirebase()
{
    JsonDocument doc;
    for (int i = 0; i < NUM_ACTIONS; ++i)
    {
        JsonArray feature_array = doc.add<JsonArray>();
        for (int j = 0; j < NUM_FEATURES; ++j)
        {
            feature_array.add(weights[i][j]);
        }
    }
    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/drone_weights.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.PUT(payload);
    http.end();
}

void updateStatsInFirebase(int episode, float epsilon_val)
{
    JsonDocument doc;
    doc["episode"] = episode;
    doc["epsilon"] = epsilon_val;
    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/training_stats.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.PUT(payload);
    http.end();
}

void updateStateInFirebase(String& jsonPayload, bool is_reset, int last_action)
{
    // Parse the incoming JSON and add the last action
    JsonDocument doc;
    deserializeJson(doc, jsonPayload);
    
    // Add the last action to the state data for the visualizer
    if (last_action >= 0) {
        doc["last_action"] = last_action;
    }
    
    // Serialize back to string
    String enhancedPayload;
    serializeJson(doc, enhancedPayload);
    
    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/simulation_state.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    // A full PUT is fine for both reset and step, as the structure is consistent.
    // The web app will know how to interpret it.
    http.PUT(enhancedPayload);
    http.end();
}

// =================================================================
// --- UTILITIES (OLED, WiFi, HTTP) ---
// =================================================================
void updateDisplayStats(int episode, int steps, float epsilon, const char* status_override)
{
    // Removed Serial.printf for faster execution
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    if (status_override)
    {
        display.println(status_override);
    }
    else
    {
        display.println("Status: Training...");
    }
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
    display.setCursor(0, 15);
    display.printf("Episode: %d\nSteps: %d\nEpsilon: %.4f", episode, steps, epsilon);
    int progress_width = map(episode, 0, NUM_EPISODES, 0, SCREEN_WIDTH - 4);
    display.drawRect(0, 50, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.fillRect(2, 52, progress_width, 8, SSD1306_WHITE);
    display.display();
    
    // Removed Serial.println for faster execution
}

String httpGETRequest(const char* url)
{
    HTTPClient http;
    http.begin(url);
    http.setTimeout(1000); // Reduced timeout from 2000ms to 1000ms for faster failures
    int httpCode = http.GET();
    String payload = (httpCode > 0) ? http.getString() : "";
    if (httpCode <= 0)
    {
        Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return payload;
}

void connectToWiFi()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting to WiFi...");
    display.display();
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        display.print(".");
        display.display();
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
}