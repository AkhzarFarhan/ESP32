#include <WiFi.h>
#include <esp_camera.h>
#include <WebServer.h>

// Replace with your network credentials
const char *ssid = "JioFiber_401_2.4Gz";
const char *password = "Melvin420";

// Set camera configuration (OV7670)
camera_config_t config;

WebServer server(80);  // Declare the server object globally so it's accessible in both setup() and loop()

void setup() {
    Serial.begin(9600);
    delay(1000);  // Wait for Serial Monitor to initialize

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("WiFi connected");

    // Configure camera settings
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 4;   // GPIO4
    config.pin_d1 = 5;   // GPIO5
    config.pin_d2 = 12;  // GPIO12
    config.pin_d3 = 19;  // GPIO13
    config.pin_d4 = 25;  // GPIO14
    config.pin_d5 = 22;  // GPIO15
    config.pin_d6 = 23;   // GPIO2
    config.pin_d7 = 35;   // GPIO0
    config.pin_xclk = 21; // GPIO0 (XCLK)
    config.pin_pclk = 15; // GPIO22
    config.pin_vsync = 27; // GPIO27
    config.pin_href = 32; // GPIO25
    config.pin_sccb_sda = 14; // GPIO26 (SDA)
    config.pin_sccb_scl = 13; // GPIO23 (SCL)
    config.pin_reset = -1;   // No reset pin
    config.pixel_format = PIXFORMAT_JPEG;  // Set JPEG format
    config.frame_size = FRAMESIZE_QVGA;   // Set frame size (QVGA)
    config.fb_count = 2;  // Double buffering (2 frames)

    // Initialize the camera
    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera initialization failed");
        while (1);
    }

    // Start the web server for live video streaming
    server.on("/", HTTP_GET, [&]() {  // Capture 'server' in the lambda
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            server.send(500, "text/plain", "Camera capture failed");
            return;
        }

        // Send the captured image as JPEG
        if (fb->format == PIXFORMAT_JPEG) {
            // Convert the buffer to String and send it as content
            String content = String((char*)fb->buf, fb->len);
            server.send(200, "image/jpeg", content);  // Send JPEG image content
        } else {
            server.send(500, "text/plain", "Unsupported pixel format");
        }

        // Return the framebuffer to free memory
        esp_camera_fb_return(fb);
    });

    server.begin();
    Serial.println("Web server started");
}

void loop() {
    // Handle incoming HTTP requests (like video streaming)
    server.handleClient();
}
