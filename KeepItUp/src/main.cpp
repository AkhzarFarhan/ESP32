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
const char* server_ip = "192.168.29.120";      // The IP of your PC or Termux tablet

// =================================================================
// --- REINFORCEMENT LEARNING (Q-LEARNING) PARAMETERS ---
// =================================================================
const float ALPHA = 0.1;           // Learning Rate: How much we update Q-values based on new info.
const float GAMMA = 0.9;           // Discount Factor: How much we value future rewards.
float epsilon = 1.0;               // Exploration Rate: Starts at 100% (full exploration).
const float EPSILON_DECAY = 0.999; // How quickly epsilon shrinks (e.g., 0.999 = slow decay).
const int NUM_EPISODES = 5000;     // Total number of training attempts.

// =================================================================
// --- STATE AND ACTION SPACE DEFINITION ---
// =================================================================
// We must "discretize" the continuous values from the server into buckets.
// This allows us to create a finite-sized Q-table.
const int POS_BUCKETS = 10; // Number of buckets for position
const int VEL_BUCKETS = 10; // Number of buckets for velocity
const int NUM_ACTIONS = 2;  // 0: Do Nothing, 1: Thrust
const int STATE_SPACE_SIZE = POS_BUCKETS * VEL_BUCKETS;

// The Q-Table: This is the "brain" of our agent.
// It stores the expected future rewards for an action in a given state.
// Q_table[state_index][action]
float Q_table[STATE_SPACE_SIZE][NUM_ACTIONS];

// =================================================================
// --- AIRTIME TRACKING AND DISPLAY VARIABLES ---
// =================================================================
unsigned long episode_start_time = 0;   // Time when episode started
unsigned long longest_airtime = 0;      // Longest time object stayed in air (milliseconds)
unsigned long current_episode_time = 0; // Current episode duration
int display_line = 0;                   // Current line for scrolling display
String log_buffer[8];                   // Buffer for display lines
bool display_initialized = false;

// =================================================================
// --- DISPLAY AND LOGGING FUNCTIONS ---
// =================================================================
void displayLog(String message);
void updateDisplay();
void updateAirtimeDisplay();

// =================================================================
// SETUP: Runs once on boot
// =================================================================
void setup()
{
    Serial.begin(115200);
    randomSeed(analogRead(0)); // Seed the random number generator

    // Initialize OLED Display
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Don't proceed, loop forever
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display_initialized = true;
    
    // Initialize log buffer
    for (int i = 0; i < 8; i++) {
        log_buffer[i] = "";
    }
    
    displayLog("OLED Display Ready");
    displayLog("Initializing Q-Table...");

    // Initialize Q-Table with all zeros
    for (int i = 0; i < STATE_SPACE_SIZE; ++i)
    {
        for (int j = 0; j < NUM_ACTIONS; ++j)
        {
            Q_table[i][j] = 0.0;
        }
    }

    displayLog("Q-Table Initialized");
    displayLog("Connecting to WiFi...");

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        displayLog(".");
    }
    
    displayLog("WiFi Connected!");
    displayLog("ESP32 IP: " + WiFi.localIP().toString());
    displayLog("Server IP: " + String(server_ip));
    delay(2000); // Show connection info for 2 seconds
}


// =================================================================
// HELPER FUNCTIONS
// =================================================================

/**
 * @brief Converts continuous position and velocity into a single discrete state index for the Q-table.
 * @param pos The current position (height) from the server.
 * @param vel The current velocity from the server.
 * @return An integer index representing the current state.
 */
int getStateIndex(float pos, float vel)
{
    // Clamp values to be within expected ranges to prevent errors
    pos = constrain(pos, 0.0, 100.0);
    vel = constrain(vel, -10.0, 10.0); // Assuming velocity won't exceed this range much
    
    // Convert position [0, 100] into an integer bucket [0, POS_BUCKETS-1]
    int pos_bucket = (int)(pos / (100.0 / POS_BUCKETS));
    
    // Convert velocity [-10, 10] into an integer bucket [0, VEL_BUCKETS-1]
    // We add 10 to make the range [0, 20] before calculating the bucket
    int vel_bucket = (int)((vel + 10.0) / (20.0 / VEL_BUCKETS));

    // Ensure buckets are within bounds, especially for edge cases
    if (pos_bucket >= POS_BUCKETS) pos_bucket = POS_BUCKETS - 1;
    if (vel_bucket >= VEL_BUCKETS) vel_bucket = VEL_BUCKETS - 1;

    // Combine the two bucket indices into a single unique index
    return pos_bucket * VEL_BUCKETS + vel_bucket;
}

/**
 * @brief Chooses an action based on the epsilon-greedy strategy.
 * @param state_index The current discrete state of the agent.
 * @return An action (0 or 1).
 */
int chooseAction(int state_index)
{
    float random_val = (float)random(100) / 100.0;
    if (random_val < epsilon)
    {
        // Explore: choose a random action
        return random(0, 2);
    }
    else
    {
        // Exploit: choose the best known action from the Q-table
        if (Q_table[state_index][0] > Q_table[state_index][1])
        {
            return 0; // Best action is "Do Nothing"
        }
        else
        {
            return 1; // Best action is "Thrust"
        }
    }
}

/**
 * @brief Performs an HTTP GET request to the specified URL.
 * @param url The URL to send the GET request to.
 * @return A String containing the server's response payload. Returns empty on failure.
 */
String httpGETRequest(const char* url)
{
    HTTPClient http;
    http.begin(url);
    http.setTimeout(2000); // Set a timeout of 2 seconds
    int httpCode = http.GET();

    String payload = "";
    if (httpCode > 0)
    {
        payload = http.getString();
    }
    else
    {
        Serial.printf("[HTTP] GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    return payload;
}

// =================================================================
// --- DISPLAY AND LOGGING FUNCTIONS IMPLEMENTATION ---
// =================================================================

/**
 * @brief Adds a message to the display log buffer and updates the screen
 * @param message The message to display
 */
void displayLog(String message)
{
    if (!display_initialized) return;
    
    // Shift existing messages up
    for (int i = 0; i < 7; i++) {
        log_buffer[i] = log_buffer[i + 1];
    }
    
    // Add new message at the bottom
    log_buffer[7] = message;
    
    // Update display
    updateDisplay();
    
    // Also print to Serial for debugging
    Serial.println(message);
}

/**
 * @brief Updates the OLED display with current log buffer
 */
void updateDisplay()
{
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Display log messages
    for (int i = 0; i < 8; i++) {
        display.setCursor(0, i * 8);
        display.println(log_buffer[i]);
    }
    
    display.display();
}

/**
 * @brief Updates the display with current airtime information
 */
void updateAirtimeDisplay()
{
    if (!display_initialized) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Show current episode time
    display.setCursor(0, 0);
    display.println("Q-Learning KeepItUp");
    display.setCursor(0, 10);
    display.println("Current Time:");
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println(String(current_episode_time / 1000.0, 1) + "s");
    
    // Show best time
    display.setTextSize(1);
    display.setCursor(0, 40);
    display.println("Best Record:");
    display.setCursor(0, 50);
    display.setTextSize(2);
    display.println(String(longest_airtime / 1000.0, 1) + "s");
    
    display.display();
}

// =================================================================
// MAIN LOOP: Contains the training logic
// =================================================================
void loop()
{
    Serial.println("\n--- Starting Training ---");

    for (int episode = 1; episode <= NUM_EPISODES; ++episode)
    {
        // 1. Reset the environment by calling the server
        JsonDocument doc; // For parsing JSON from server
        String url = "http://" + String(server_ip) + ":5000/reset";
        String payload = httpGETRequest(url.c_str());
        if (payload == "")
        {
            Serial.println("Reset failed. Skipping episode.");
            delay(1000);
            continue;
        }

        deserializeJson(doc, payload);
        float position = doc["position"];
        float velocity = doc["velocity"];
        bool done = false;
        int steps = 0;

        episode_start_time = millis(); // Start timing the episode
        
        // --- Run one full episode until it ends (fall or timeout) ---
        while (!done)
        {
            // 2. Get current state index from continuous physical values
            int state_index = getStateIndex(position, velocity);

            // 3. Choose an Action using the Epsilon-Greedy Strategy
            int action = chooseAction(state_index);

            // 4. Take the action by calling the server's /step endpoint
            url = "http://" + String(server_ip) + ":5000/step?action=" + String(action);
            payload = httpGETRequest(url.c_str());
            if (payload == "")
            {
                displayLog("Step failed. Ending episode.");
                break;
            }

            deserializeJson(doc, payload);
            float new_position = doc["position"];
            float new_velocity = doc["velocity"];
            float reward = doc["reward"];
            done = doc["done"];

            // 5. Get the new state index based on the outcome of our action
            int new_state_index = getStateIndex(new_position, new_velocity);

            // 6. Update the Q-Table using the Bellman Equation (The "Learning" Step!)
            float max_q_next_state = max(Q_table[new_state_index][0], Q_table[new_state_index][1]);
            
            Q_table[state_index][action] = Q_table[state_index][action] + 
                ALPHA * (reward + GAMMA * max_q_next_state - Q_table[state_index][action]);

            // 7. Update current state for the next step of the episode
            position = new_position;
            velocity = new_velocity;
            steps++;
            
            // 8. Update display with current episode info
            current_episode_time = millis() - episode_start_time;
            updateAirtimeDisplay();
        }
        
        // Calculate episode duration and update record
        current_episode_time = millis() - episode_start_time;
        if (current_episode_time > longest_airtime) {
            longest_airtime = current_episode_time;
            displayLog("NEW RECORD: " + String(longest_airtime / 1000.0, 2) + "s");
        }

        // Decay epsilon after each episode to shift from exploration to exploitation
        epsilon *= EPSILON_DECAY;
        if (epsilon < 0.01) {
            epsilon = 0.01; // Set a minimum exploration rate
        }

        // Log episode completion
        String episodeInfo = "Episode " + String(episode) + " - " + String(steps) + " steps";
        displayLog(episodeInfo);
        displayLog("Time: " + String(current_episode_time / 1000.0, 2) + "s");
        Serial.printf("Episode %d finished after %d steps. Epsilon: %.4f\n", episode, steps, epsilon);
    }

    displayLog("Training Complete!");
    displayLog("Best: " + String(longest_airtime / 1000.0, 2) + "s");
    
    // Stop the loop after training is complete
    while(true) {
        updateAirtimeDisplay();
        delay(1000); 
    }
}
