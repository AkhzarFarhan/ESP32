#include <WiFi.h>
#include <Camera.h>

// OV7670 Pin Configurations
#define D0 32
#define D1 33
#define D2 34
#define D3 35
#define D4 36
#define D5 39
#define D6 38
#define D7 37

#define XCLK 21
#define PCLK 22
#define VSYNC 19
#define HSYNC 18

#define SCL 22
#define SDA 23

Camera camera;

void setup()
{
    Serial.begin(115200);
    Serial.println("Initializing OV7670...");

    // Initialize the camera
    if (!camera.init(240, 320, D0, D1, D2, D3, D4, D5, D6, D7, VSYNC, HSYNC, PCLK, XCLK))
    {
        Serial.println("Camera initialization failed!");
        while (true); // Halt execution
    }
    Serial.println("Camera initialized successfully.");
}

void loop()
{
  // Capture an image
  const uint8_t* frame = camera.capture();
  if (frame)
  {
      Serial.println("Frame captured.");
      
      // Example: Process the image or save to SD card (to be implemented)
      // e.g., saveToSDCard(frame, camera.width(), camera.height());

      delay(1000); // Wait before capturing the next frame
  }
  else
  {
      Serial.println("Failed to capture frame.");
  }
}
