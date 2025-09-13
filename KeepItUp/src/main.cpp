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
// --- REINFORCEMENT LEARNING & MODEL PARAMETERS ---
// =================================================================
const float ALPHA = 0.005;           // Even smaller learning rate for this complex problem
const float GAMMA = 0.98;            // Higher discount factor to encourage long-term planning
float epsilon = 1.0;
const float EPSILON_DECAY = 0.9998;  // Very slow decay
const int NUM_EPISODES = 50000;

const int NUM_ACTIONS = 5; // 0:Nothing, 1:Up, 2:Down, 3:Left, 4:Right
const int NUM_FEATURES = 8;

// Local cache for the weights. This gets synced with Firebase.
float weights[NUM_ACTIONS][NUM_FEATURES];

// Forward Declarations
void updateDisplayStats(int episode, int steps, float epsilon, const char* status_override = nullptr);
float getQValue(float* features_in, int action);
float getMaxQValue(float* features_in);
int chooseAction(float* features_in);
String httpGETRequest(const char* url);
void updateAllWeightsInFirebase();
bool getWeightsFromFirebase();
void connectToWiFi();
void runEpisode(int episode);
void getFeatures(JsonDocument& doc, float* features_out);


// =================================================================
// SETUP
// =================================================================
void setup()
{
    Serial.begin(115200);
    randomSeed(analogRead(0));

    // Init Display
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed")); for(;;);
    }

    connectToWiFi();
    
    // On first boot, try to load weights from Firebase.
    // If it fails (e.g., empty DB), initialize them to zero.
    if (!getWeightsFromFirebase())
    {
        for (int i = 0; i < NUM_ACTIONS; ++i)
        {
            for (int j = 0; j < NUM_FEATURES; ++j)
            {
                weights[i][j] = 0.0;
            }
        }
        updateDisplayStats(0, 0, 0, "Initialized weights");
        delay(1000);
    }
}

// =================================================================
// MAIN LOOP
// =================================================================
void loop()
{
    for (int episode = 1; episode <= NUM_EPISODES; ++episode)
    {
        // At the start of each episode, we ensure our local weights are fresh from Firebase
        getWeightsFromFirebase();

        runEpisode(episode);

        // **OPTIMIZATION**: Push all accumulated changes to Firebase ONCE per episode.
        updateAllWeightsInFirebase();

        // Decay epsilon
        epsilon *= EPSILON_DECAY;
        if (epsilon < 0.05) epsilon = 0.05;
    }
    updateDisplayStats(NUM_EPISODES, 0, 0, "TRAINING FINISHED");
    while(true) { delay(10000); }
}

// =================================================================
// --- CORE RL & SIMULATION LOGIC ---
// =================================================================

/**
 * @brief Runs one full episode of the simulation on the currently selected server.
 */
void runEpisode(int episode)
{
    // 1. Reset the environment
    JsonDocument state_doc;
    String url = "http://" + String(SERVER_IP) + ":5000/reset";
    String payload = httpGETRequest(url.c_str());
    if (payload == "") return;
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
        if (payload == "") break;
        deserializeJson(state_doc, payload);
        float reward = state_doc["reward"];
        done = state_doc["done"];

        // 5. Calculate Q-values for learning
        float current_q = getQValue(features, action);
        float max_q_next_state = 0;
        if (!done) {
            float next_features[NUM_FEATURES];
            getFeatures(state_doc, next_features);
            max_q_next_state = getMaxQValue(next_features);
        }

        // 6. Update the LOCAL weights cache. No network call here.
        float error = (reward + GAMMA * max_q_next_state) - current_q;
        for (int i = 0; i < NUM_FEATURES; ++i)
        {
            weights[action][i] += ALPHA * error * features[i];
        }
        steps++;
        if (steps > 800) done = true; // Timeout to prevent infinite loops
        updateDisplayStats(action, steps, current_q);
    }
    updateDisplayStats(episode, steps, epsilon);
}

/**
 * @brief Extracts and normalizes features from the drone's state.
 */
void getFeatures(JsonDocument& doc, float* features_out)
{
    float x = doc["x"], y = doc["y"], vx = doc["vx"], vy = doc["vy"];
    float target_x = doc["target_x"], target_y = doc["target_y"];

    features_out[0] = (x - target_x) / 400.0; // Normalized X-dist to target
    features_out[1] = (y - target_y) / 300.0; // Normalized Y-dist to target
    features_out[2] = vx / 5.0;              // Normalized VX
    features_out[3] = vy / 5.0;              // Normalized VY
    features_out[4] = y / 300.0;             // Normalized altitude
    features_out[5] = (float)doc["has_package"]; // Has package? (0 or 1)
    features_out[6] = (float)doc["battery"] / 600.0; // Normalized battery
    features_out[7] = 1.0;                   // Bias term
}

// --- Q-VALUE & ACTION SELECTION FUNCTIONS (Identical to previous advanced version) ---
float getQValue(float* features_in, int action);
float getMaxQValue(float* features_in);
int chooseAction(float* features_in);


// =================================================================
// --- FIREBASE COMMUNICATION ---
// =================================================================
/**
 * @brief Fetches the entire weights structure from Firebase and loads it into the local cache.
 * @return True on success, False on failure.
 */
bool getWeightsFromFirebase()
{
    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/drone_weights.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        JsonDocument doc;
        // Increase JSON buffer size for fetching the whole model
        DeserializationError error = deserializeJson(doc, http.getString());
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
            http.end();
            return false;
        }

        if (doc.isNull()) { // Handle case where DB is empty
            http.end();
            return false;
        }
        for (int i = 0; i < NUM_ACTIONS; ++i) {
            for (int j = 0; j < NUM_FEATURES; ++j) {
                weights[i][j] = doc[String(i)][String(j)];
            }
        }
        http.end();
        return true;
    }
    http.end();
    return false;
}

/**
 * @brief (DEPRECATED) Updates a single weight value in Firebase. No longer used.
 */
// void updateWeightInFirebase(int action, int feature_index, float value) { ... }


/**
 * @brief **NEW FUNCTION**
 * Serializes the entire local weights cache to a JSON object and sends it
 * to Firebase in a single PUT request, overwriting the old data.
 */
void updateAllWeightsInFirebase()
{
    JsonDocument doc;

    for (int i = 0; i < NUM_ACTIONS; ++i)
    {
        JsonObject action_obj = doc[String(i)].to<JsonObject>();
        for (int j = 0; j < NUM_FEATURES; ++j)
        {
            action_obj[String(j)] = weights[i][j];
        }
    }

    String payload;
    serializeJson(doc, payload);

    HTTPClient http;
    String url = String(FIREBASE_HOST) + "/drone_weights.json?auth=" + String(FIREBASE_SECRET);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(payload);
    if (httpCode <= 0) {
        Serial.printf("[HTTP] PUT failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}


// =================================================================
// --- UTILITIES (OLED, WiFi, HTTP) ---
// =================================================================
void updateDisplayStats(int episode, int steps, float epsilon, const char* status_override)
{
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
    
    // Progress Bar
    int progress_width = map(episode, 0, NUM_EPISODES, 0, SCREEN_WIDTH - 4);
    display.drawRect(0, 50, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.fillRect(2, 52, progress_width, 8, SSD1306_WHITE);

    display.display();
}

String httpGETRequest(const char* url)
{
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);
    int httpCode = http.GET();
    String payload = (httpCode > 0) ? http.getString() : "";
    if (httpCode <= 0) {
        Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return payload;
}


// --- Stubs for reused functions to keep the file complete ---
float getQValue(float* features_in, int action) { float q = 0.0; for (int i = 0; i < NUM_FEATURES; ++i) q += weights[action][i] * features_in[i]; return q; }
float getMaxQValue(float* features_in) { float max_q = -1e9; for (int i = 0; i < NUM_ACTIONS; ++i) max_q = max(max_q, getQValue(features_in, i)); return max_q; }
int chooseAction(float* features_in) { if ((float)random(1000)/1000.0 < epsilon) return random(0, NUM_ACTIONS); else { int best_action = 0; float max_q = -1e9; for (int i = 0; i < NUM_ACTIONS; ++i) { float q = getQValue(features_in, i); if (q > max_q) { max_q = q; best_action = i; } } return best_action; } }
void connectToWiFi() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting to WiFi...");
    display.display();
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        display.print(".");
        display.display();
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
}

