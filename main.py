import network
import urequests
import json
import machine
import time

# WiFi Credentials
WIFI_SSID = 'WIFI_SSID'
WIFI_PASSWORD = 'WIFI_PASSWORD'

# API Endpoint
FB_AUTH = 'API_KEY'
FB_PROJECT = 'ESP32'
FB_USER = 'USERNAME'
API_URL = f"https://PROJECT-NAME.firebaseio.com/{FB_PROJECT}/{FB_USER}.json?auth={FB_AUTH}"

# Function to connect to WiFi
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    
    if not wlan.isconnected():
        print("Connecting to WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASSWORD)
        
        timeout = 0
        while not wlan.isconnected() and timeout < 10:  # 10-second timeout
            time.sleep(1)
            timeout += 1
            print(f"Attempt {timeout} to connect to WiFi")
    
    if wlan.isconnected():
        print("Connected to WiFi:", wlan.ifconfig())
        print(f"My ESP32's IP address is: {wlan.ifconfig()}")
    else:
        print("Failed to connect to WiFi.")
        machine.reset()  # Soft reset on connection failure for reliability

# Function to fetch JSON data from API
def fetch_data():
    try:
        print("Fetching data from API...")
        response = urequests.get(API_URL)
        
        if response.status_code == 200:
            data = response.json()
            print("Data fetched successfully.")
            response.close()
            return data
        else:
            print("Failed to fetch data, status code:", response.status_code)
            response.close()
            return None
    except Exception as e:
        print("Exception occurred while fetching data:", e)
        return None

# Function to process data (customize based on your requirements)
def process_data(data):
    if data:
        # Example: Assume data has {"temperature": value, "humidity": value}
        try:
            print(f"{data}")
            new_data = {"message": [time.time(), time.time()]}
            headers = {"Content-Type": "application/json"}
            response = urequests.post(API_URL, data=json.dumps(new_data), headers=headers)
            if response.status_code == 200:
                print("Data posted successfully:", response.json())
            else:
                print("Failed to post data:", response.status_code)
            # Do more processing here as needed
        except KeyError as e:
            print(f"KeyError: Missing key {e} in data")
    else:
        print("No data to process.")

# Main program execution
def main():
    connect_wifi()  # Step 1: Connect to WiFi
    
    if network.WLAN(network.STA_IF).isconnected():  # Ensure WiFi is connected
        while True:
            data = fetch_data()  # Step 2: Fetch JSON data
            process_data(data)  # Step 3: Process the data
            time.sleep(1)
    else:
        print("WiFi not connected. Cannot fetch data.")

# Soft reboot setup
def soft_reboot():
    print("Soft rebooting...")
    machine.reset()

# Entry point
if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("Exception in main program:", e)
        soft_reboot()

