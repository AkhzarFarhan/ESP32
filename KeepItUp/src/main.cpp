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
const char* server_ip = "192.168.29.138";      // The IP of your PC or Termux tablet

// =================================================================
// --- REINFORCEMENT LEARNING PARAMETERS ---
// =================================================================
const float ALPHA = 0.01;            // Learning Rate needs to be smaller for function approximation
const float GAMMA = 0.95;            // Discount Factor
float epsilon = 1.0;                 // Exploration Rate
const float EPSILON_DECAY = 0.9995;  // Slower decay for a more complex problem
const int NUM_EPISODES = 20000;      // This problem needs much more training

// =================================================================
// --- FUNCTION APPROXIMATION SETUP ---
// =================================================================
const int NUM_ACTIONS = 4; // 0:Nothing, 1:Thrust, 2:Rotate Left, 3:Rotate Right
const int NUM_FEATURES = 7; // The number of features we define below

// The "Brain": A set of weights, one for each feature, for each possible action.
// Instead of a giant Q-table, we only need this small array.
float weights[NUM_ACTIONS][NUM_FEATURES];

// Global state variables
float features[NUM_FEATURES];

// =================================================================
// FORWARD DECLARATION for helper function
// =================================================================
void updateDisplayStats(int episode, int steps, float epsilon, const char* status);
String httpGETRequest(const char* url);
void connectToWiFi();
void getFeatures(JsonDocument& doc, float* features_out);
float getQValue(float* features_in, int action);
float getMaxQValue(float* features_in);
int chooseAction(float* features_in);


// =================================================================
// MAIN LOOP: Contains the training logic
// =================================================================
void loop()
{
    updateDisplayStats(0, 0, epsilon, "Starting...");
    delay(2000);

    for (int episode = 1; episode <= NUM_EPISODES; ++episode)
    {
        // 1. Reset the environment
        JsonDocument state_doc;
        String url = "http://" + String(server_ip) + ":5000/reset";
        String payload = httpGETRequest(url.c_str());
        if (payload == "") { continue; }
        deserializeJson(state_doc, payload);

        bool done = false;
        int steps = 0;

        // --- Run one full episode ---
        while (!done)
        {
            // 2. Get features for the current state
            getFeatures(state_doc, features);

            // 3. Choose Action based on current features and weights
            int action = chooseAction(features);

            // 4. Take action
            url = "http://" + String(server_ip) + ":5000/step?action=" + String(action);
            payload = httpGETRequest(url.c_str());
            if (payload == "") { break; }
            
            deserializeJson(state_doc, payload);
            float reward = state_doc["reward"];
            done = state_doc["done"];

            // 5. Calculate the Q-value for the action we just took
            float current_q = getQValue(features, action);

            // 6. Find the maximum Q-value for the *next* state
            float max_q_next_state = 0;
            if (!done)
            {
                float next_features[NUM_FEATURES];
                getFeatures(state_doc, next_features);
                max_q_next_state = getMaxQValue(next_features);
            }

            // 7. Update the weights using Gradient Descent (The "Learning" Step!)
            float error = (reward + GAMMA * max_q_next_state) - current_q;
            for (int i = 0; i < NUM_FEATURES; ++i)
            {
                weights[action][i] += ALPHA * error * features[i];
            }
            steps++;
        }

        epsilon *= EPSILON_DECAY;
        if (epsilon < 0.01) epsilon = 0.01;

        updateDisplayStats(episode, steps, epsilon, "Training...");
    }

    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println("TRAINING");
    display.println("FINISHED");
    display.display();

    while(true) { delay(10000); }
}

// =================================================================
// HELPER FUNCTIONS
// =================================================================

/**
 * @brief Updates the OLED display with the current training statistics.
 */
void updateDisplayStats(int episode, int steps, float epsilon, const char* status)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    
    display.println(status);
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

    display.setCursor(0, 15);
    display.printf("Episode: %d/%d\n", episode, NUM_EPISODES);
    
    display.setCursor(0, 25);
    display.printf("Steps: %d\n", steps);
    
    display.setCursor(0, 35);
    display.printf("Epsilon: %.4f\n", epsilon);
    
    // Progress Bar
    int progress_width = map(episode, 0, NUM_EPISODES, 0, SCREEN_WIDTH - 4);
    display.drawRect(0, 50, SCREEN_WIDTH, 12, SSD1306_WHITE);
    display.fillRect(2, 52, progress_width, 8, SSD1306_WHITE);

    display.display();
}

/**
 * @brief Extracts and normalizes features from the server's state JSON.
 * Normalizing (scaling values to be around -1 to 1) helps learning stability.
 */
void getFeatures(JsonDocument& doc, float* features_out)
{
    float x = doc["x"];
    float y = doc["y"];
    float vx = doc["vx"];
    float vy = doc["vy"];
    float angle = doc["angle"];
    float pad_x = doc["pad_x"];

    // Feature 0: X-distance to pad (normalized)
    features_out[0] = (x - pad_x) / 400.0;
    // Feature 1: Y-distance to ground (normalized)
    features_out[1] = y / 300.0;
    // Feature 2: Horizontal velocity (normalized)
    features_out[2] = vx / 5.0;
    // Feature 3: Vertical velocity (normalized)
    features_out[3] = vy / 5.0;
    // Feature 4: Angle (already in a good range)
    features_out[4] = angle;
    // Feature 5: Is the lander directly above the pad?
    features_out[5] = (abs(x - pad_x) < 20) ? 1.0 : 0.0;
    // Feature 6: Bias/Constant term (always 1.0) - very important!
    features_out[6] = 1.0;
}

/**
 * @brief Calculates the Q-value for a state-action pair using the linear model.
 */
float getQValue(float* features_in, int action)
{
    float q_value = 0.0;
    for (int i = 0; i < NUM_FEATURES; ++i)
    {
        q_value += weights[action][i] * features_in[i];
    }
    return q_value;
}

/**
 * @brief Finds the maximum Q-value among all possible actions for a given state.
 */
float getMaxQValue(float* features_in)
{
    float max_q = -1e9; // Large negative number
    for (int i = 0; i < NUM_ACTIONS; ++i)
    {
        max_q = max(max_q, getQValue(features_in, i));
    }
    return max_q;
}


/**
 * @brief Chooses an action using the epsilon-greedy strategy.
 */
int chooseAction(float* features_in)
{
    if ((float)random(1000) / 1000.0 < epsilon)
    {
        return random(0, NUM_ACTIONS); // Explore
    }
    else
    {
        // Exploit: find the action with the highest Q-value
        int best_action = 0;
        float max_q = -1e9;
        for (int i = 0; i < NUM_ACTIONS; ++i)
        {
            float current_q = getQValue(features_in, i);
            if (current_q > max_q)
            {
                max_q = current_q;
                best_action = i;
            }
        }
        return best_action;
    }
}

// --- WiFi and HTTP Communication ---
void connectToWiFi()
{
    display.clearDisplay();
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
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.printf("IP:\n%s\n", WiFi.localIP().toString().c_str());
    display.printf("Server:\n%s\n", server_ip);
    display.display();
    delay(2500);
}

String httpGETRequest(const char* url)
{
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000);
    int httpCode = http.GET();
    String payload = (httpCode > 0) ? http.getString() : "";
    if (httpCode <= 0) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextSize(1);
        display.println("HTTP GET FAILED");
        display.println(http.errorToString(httpCode).c_str());
        display.display();
        delay(1000);
    }
    http.end();
    return payload;
}

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup()
{
    Serial.begin(115200); // Keep serial for initial debugging
    randomSeed(analogRead(0));

    // Initialize OLED Display
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("OLED Init OK");
    display.display();
    delay(1000);

    // Initialize all weights to zero
    for (int i = 0; i < NUM_ACTIONS; ++i)
    {
        for (int j = 0; j < NUM_FEATURES; ++j)
        {
            weights[i][j] = 0.0;
        }
    }

    connectToWiFi();
}