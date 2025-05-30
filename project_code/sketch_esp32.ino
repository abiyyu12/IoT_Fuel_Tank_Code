#include <WiFi.h> // Library for WiFi connectivity
#include <WiFiClientSecure.h> // Library for secure WiFi client
#include <HTTPClient.h> // Library for HTTP requests
#include <ArduinoJson.h> // Library for handling JSON
#include <Arduino.h> // Core Arduino library
#include <FlowMeter.h> // Library for flow meter functionality
#include <NewPing.h> // Library for ultrasonic distance measurement
#include <math.h> // Math library for mathematical functions

// Pin definitions
#define MQ2_PIN 35 // Analog pin for smoke sensor
#define FLAME_SENSOR_PIN 4 // Digital pin for flame sensor
#define BUZZER_PIN  27 // Pin for buzzer (change as needed)
#define RELAY1_PIN_IN_1 23 // Relay pin 1
#define RELAY1_PIN_IN_2 22 // Relay pin 2
#define RELAY1_PIN_IN_3 21 // Relay pin 3
#define RELAY1_PIN_IN_4 17 // Relay pin 4
#define BUTTON_START 5 // Pin for start button
#define BUTTON_END 18 // Pin for end button
#define FLOW_SENSOR 15 // Pin for flow sensor
#define TRIGGER_PIN 33 // Trigger pin for HC-SR04 ultrasonic sensor
#define ECHO_PIN 34 // Echo pin for HC-SR04 ultrasonic sensor
#define MAX_DISTANCE 300 // Maximum distance for ultrasonic measurement (in cm)

// Daily tank dimensions
const float tinggi_aquarium = 26.0; // Height of the aquarium in cm
const float panjang = 35.0; // Length of the aquarium in cm
const float lebar = 21.5; // Width of the aquarium in cm

// Flags for detecting flame and smoke
bool IS_FLAME_DETECTED = false; // Flag for flame detection
bool IS_SMOKE_DETECTED = false; // Flag for smoke detection

// WiFi credentials
const char* ssid = "ABYAN"; // WiFi SSID
const char* password = "KeluargaIdaman"; // WiFi password

// Endpoints to save data
const char* saveSmokeSensorURL = "https://"; // URL for smoke sensor data
const char* saveFlowMeterURL = "https://"; // URL for flow meter data
const char* saveVolumeTankURL = "https://"; // URL for tank volume data
const char* saveFlameSensorURL= "https://"; // URL for flame sensor data

// Flow meter properties
FlowSensorProperties sensorProperties = {
  30.0f, // Maximum flow rate
  2.9,   // K-Factor Pulse
  {0.8197, 0.7692, 0.8955, 0.5556, 0.8209, 0.9091} // M-Factor updated
};


FlowMeter* flowMeter; // Pointer to FlowMeter object

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // Create NewPing object for ultrasonic sensor

// Variables for flow measurement
bool pulseDetected = false; // Flag for pulse detection
bool measuring = false; // Flag for measuring state
unsigned long flowStartTime = 0; // Start time for flow measurement
unsigned long flowLastTime = 0; // Last time flow was detected
unsigned long lastTime = 0; // Last time measurement was taken
unsigned long lastDuration = 0; // Duration of last measurement


double totalVolume = 0.0; // Total volume of water measured

bool calibrating = false; // Flag for calibration state
const float offset = 2.0; // Correction offset for measurement (2 cm)

// Variables for sending tank volume data
unsigned long lastSendTimeVolumeTank = 0; // Last time data was sent for tank volume
const unsigned long intervalSendVolumeTank = 15000; // Interval for sending tank volume data (15 seconds)

// Interrupt Service Routine for flow measurement
IRAM_ATTR void flowInterrupt() {
  flowMeter->count(); // Count the pulse from the flow meter
  if (!pulseDetected) { // If pulse not detected yet
    pulseDetected = true; // Set pulse detected flag
    flowStartTime = millis(); // Record the start time
  }
  flowLastTime = millis(); // Update the last time flow was detected
}

// First State Fow FLow Sensor Will Not Offset.
bool isFirstRun = false;

// Main setup function
void setup() {
  Serial.begin(115200); // Start serial communication at 115200 baud rate
  connectToWiFi(); // Connect to WiFi
  pinMode(MQ2_PIN, INPUT); // Set smoke sensor pin as input
  pinMode(FLAME_SENSOR_PIN, INPUT); // Set flame sensor pin as input
  pinMode(TRIGGER_PIN, OUTPUT); // Set trigger pin for ultrasonic sensor as output
  pinMode(ECHO_PIN, INPUT); // Set echo pin for ultrasonic sensor as input

  // Configure PWM for buzzer
  pinMode(BUZZER_PIN, OUTPUT); // Set buzzer pin as output

  // Setup relay pins
  pinMode(RELAY1_PIN_IN_1, OUTPUT); // Set relay pin 1 as output
  pinMode(RELAY1_PIN_IN_2, OUTPUT); // Set relay pin 2 as output
  pinMode(RELAY1_PIN_IN_3, OUTPUT); // Set relay pin 3 as output
  pinMode(RELAY1_PIN_IN_4, OUTPUT); // Set relay pin 4 as output

  // Setup button pins
  pinMode(BUTTON_START, INPUT_PULLUP); // Set start button pin as input with pull-up resistor
  pinMode(BUTTON_END, INPUT_PULLUP); // Set end button pin as input with pull-up resistor

  // Set initial state of relays (ensure pump is off at startup)
  digitalWrite(RELAY1_PIN_IN_1, HIGH); // Turn off relay 1
  digitalWrite(RELAY1_PIN_IN_2, LOW); // Turn on relay 2
  digitalWrite(RELAY1_PIN_IN_3, HIGH); // Turn off relay 3
  digitalWrite(RELAY1_PIN_IN_4, HIGH); // Turn off relay 4

  // Setup Flow Meter
  flowMeter = new FlowMeter(digitalPinToInterrupt(FLOW_SENSOR), sensorProperties, flowInterrupt, FALLING); // Initialize flow meter
}

// Function to connect ESP32 to WiFi
void connectToWiFi() {
  Serial.print("Menghubungkan ke WiFi"); // Print connecting message
  WiFi.begin(ssid, password); // Start WiFi connection
  int maxAttempts = 20; // Maximum attempts to connect to WiFi

  // Attempt to connect to WiFi
  while (WiFi.status() != WL_CONNECTED && maxAttempts-- > 0) {
    delay(500); // Wait for 500 ms
    Serial.print("."); // Print dot for each attempt
  }

  // Check if connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Terhubung ke WiFi!"); // Print success message
    Serial.print("📡 IP Address: "); // Print IP address message
    Serial.println(WiFi.localIP()); // Print local IP address
  } else {
    Serial.println("\n❌ Gagal terhubung ke WiFi! Periksa kredensial atau jaringan."); // Print failure message
  }
}

// Function to send smoke sensor data to server
void sendSmokeSensorToEndpoint(bool smokeDetected) {
  if (WiFi.status() != WL_CONNECTED) { // Check if connected to WiFi
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim."); // Print error message
    return; // Exit function
  }

  WiFiClientSecure client; // Create secure WiFi client
  client.setInsecure(); // Set client to insecure mode

  HTTPClient http; // Create HTTP client
  http.setTimeout(5000); // Set timeout for HTTP requests

  StaticJsonDocument<200> jsonDoc; // Create JSON document
  jsonDoc["smoke_detected"] = smokeDetected; // Add smoke detection status to JSON

  String jsonString; // String to hold JSON data
  serializeJson(jsonDoc, jsonString); // Serialize JSON document to string

  http.begin(client, saveSmokeSensorURL); // Begin HTTP request
  http.addHeader("Content-Type", "application/json"); // Set content type header
  http.addHeader("X-API-KEY", "xxxx"); // Set API key header

  int httpResponseCode = http.POST(jsonString); // Send POST request

  // Check HTTP response code
  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: "); // Print success message
    Serial.println(httpResponseCode); // Print response code
  } else {
    Serial.print("❌ Error code: "); // Print error message
    Serial.println(httpResponseCode); // Print error code
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan."); // Print failure message
  }
  http.end(); // End HTTP request
}

// Function to send flow meter data to server
void sendFlowMeterToEndpoint(float literWaterFlow) {
  if (WiFi.status() != WL_CONNECTED) { // Check if connected to WiFi
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim."); // Print error message
    return; // Exit function
  }

  WiFiClientSecure client; // Create secure WiFi client
  client.setInsecure(); // Set client to insecure mode

  HTTPClient http; // Create HTTP client
  http.setTimeout(5000); // Set timeout for HTTP requests

  StaticJsonDocument<200> jsonDoc; // Create JSON document
  jsonDoc["flow_liter"] = literWaterFlow; // Add flow data to JSON

  String jsonString; // String to hold JSON data
  serializeJson(jsonDoc, jsonString); // Serialize JSON document to string

  http.begin(client, saveFlowMeterURL); // Begin HTTP request
  http.addHeader("Content-Type", "application/json"); // Set content type header
  http.addHeader("X-API-KEY", "x"); // Set API key header

  int httpResponseCode = http.POST(jsonString); // Send POST request

  // Check HTTP response code
  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: "); // Print success message
    Serial.println(httpResponseCode); // Print response code
  } else {
    Serial.print("❌ Error code: "); // Print error message
    Serial.println(httpResponseCode); // Print error code
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan."); // Print failure message
  }
  http.end(); // End HTTP request
}

// Function to send tank volume data to server
void sendVolumeTankToEndpoint(float currentVolumeLiter) {
  if (WiFi.status() != WL_CONNECTED) { // Check if connected to WiFi
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim."); // Print error message
    return; // Exit function
  }

  WiFiClientSecure client; // Create secure WiFi client
  client.setInsecure(); // Set client to insecure mode

  HTTPClient http; // Create HTTP client
  http.setTimeout(5000); // Set timeout for HTTP requests

  StaticJsonDocument<200> jsonDoc; // Create JSON document
  jsonDoc["current_volume"] = currentVolumeLiter; // Add current volume to JSON
  jsonDoc["max_tank"] = 24; // Add maximum tank capacity to JSON

  String jsonString; // String to hold JSON data
  serializeJson(jsonDoc, jsonString); // Serialize JSON document to string

  http.begin(client, saveVolumeTankURL); // Begin HTTP request
  http.addHeader("Content-Type", "application/json"); // Set content type header
  http.addHeader("X-API-KEY", "xxxx"); // Set API key header

  int httpResponseCode = http.POST(jsonString); // Send POST request

  // Check HTTP response code
  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: "); // Print success message
    Serial.println(httpResponseCode); // Print response code
  } else {
    Serial.print("❌ Error code: "); // Print error message
    Serial.println(httpResponseCode); // Print error code
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan."); // Print failure message
  }
  http.end(); // End HTTP request
}

// Function to send flame sensor data to server
void sendFlameSensorToEndpoint(bool isFlameDetected) {
  if (WiFi.status() != WL_CONNECTED) { // Check if connected to WiFi
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim."); // Print error message
    return; // Exit function
  }

  WiFiClientSecure client; // Create secure WiFi client
  client.setInsecure(); // Set client to insecure mode

  HTTPClient http; // Create HTTP client
  http.setTimeout(5000); // Set timeout for HTTP requests

  StaticJsonDocument<200> jsonDoc; // Create JSON document
  jsonDoc["flame_detected"] = isFlameDetected; // Add flame detection status to JSON

  String jsonString; // String to hold JSON data
  serializeJson(jsonDoc, jsonString); // Serialize JSON document to string

  http.begin(client, saveFlameSensorURL); // Begin HTTP request
  http.addHeader("Content-Type", "application/json"); // Set content type header
  http.addHeader("X-API-KEY", "xxx"); // Set API key header

  int httpResponseCode = http.POST(jsonString); // Send POST request

  // Check HTTP response code
  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: "); // Print success message
    Serial.println(httpResponseCode); // Print response code
  } else {
    Serial.print("❌ Error code: "); // Print error message
    Serial.println(httpResponseCode); // Print error code
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan."); // Print failure message
  }
  http.end(); // End HTTP request
}

// Main loop function
void loop() {
  // Read flame sensor status
  bool flameStatus = digitalRead(FLAME_SENSOR_PIN); // Read flame sensor
  if (flameStatus == 0 && !IS_FLAME_DETECTED) { // If flame is detected
      Serial.println("🔥 Api Terdeteksi! Buzzer Aktif!"); // Print flame detected message
      sendFlameSensorToEndpoint(true); // Uncomment to send data to server
      digitalWrite(BUZZER_PIN, HIGH); // Activate buzzer
      IS_FLAME_DETECTED = true; // Set flame detected flag
  } else if (flameStatus == 1 && IS_FLAME_DETECTED) { // If no flame detected
      Serial.println("✅ Tidak ada api. Buzzer Mati."); // Print no flame message
      sendFlameSensorToEndpoint(false); // Uncomment to send data to server
      digitalWrite(BUZZER_PIN, LOW); // Deactivate buzzer
      IS_FLAME_DETECTED = false; // Reset flame detected flag
  }

  // Read smoke sensor status
  bool smokeStatus = digitalRead(MQ2_PIN); // Read smoke sensor
  if (smokeStatus == 0 && !IS_SMOKE_DETECTED) { // If smoke is detected
      Serial.println("Asap Terdeteksi ! Buzzer Aktif!"); // Print smoke detected message
      sendSmokeSensorToEndpoint(true); // Uncomment to send data to server
      digitalWrite(BUZZER_PIN, HIGH); // Activate buzzer
      IS_SMOKE_DETECTED = true; // Set smoke detected flag
  } else if (smokeStatus == 1 && IS_SMOKE_DETECTED) { // If no smoke detected
      Serial.println("✅ Tidak ada Asap. Buzzer Mati."); // Print no smoke message
      sendSmokeSensorToEndpoint(false); // Uncomment to send data to server
      digitalWrite(BUZZER_PIN, LOW); // Deactivate buzzer
      IS_SMOKE_DETECTED = false; // Reset smoke detected flag
  }

  flowMeter->tick(); // Update flow meter

  unsigned long currentMillis = millis(); // Get current time in milliseconds
  int stateStart = digitalRead(BUTTON_START); // Read start button state
  int stateEnd = digitalRead(BUTTON_END); // Read end button state

  // Turn On Pump and solenoid valve
  if(stateStart == LOW && !measuring) { // If start button is pressed and not measuring
    measuring = true; // Set measuring flag
    calibrating = true; // Set calibrating flag
    pulseDetected = false; // Reset pulse detected flag
    totalVolume = 0.0; // Reset total volume

    flowStartTime = currentMillis; // Record flow start time
    flowLastTime = currentMillis; // Record last flow time
    lastTime = currentMillis; // Record last measurement time
    digitalWrite(RELAY1_PIN_IN_3, LOW); // Activate relay for pump
    delay(200); // Wait for 200 ms
    digitalWrite(RELAY1_PIN_IN_4, HIGH); // Activate relay for solenoid valve
    Serial.println("Pompa AKTIF - Mulai Pengukuran (Kalibrasi)"); // Print pump active message
  }

  // End measurement
  if (stateEnd == LOW && measuring) { // If end button is pressed and measuring
    measuring = false; // Reset measuring flag
    calibrating = false; // Reset calibrating flag
    // Turn off pump
    digitalWrite(RELAY1_PIN_IN_3, HIGH); // Deactivate relay for pump
    delay(500); // Wait for 500 ms
    digitalWrite(RELAY1_PIN_IN_4, LOW); // Deactivate relay for solenoid valve
    Serial.println("Pompa NONAKTIF - Pengukuran Selesai"); // Print pump inactive message

    // Get total volume measured
    double finalVolume = flowMeter->getTotalVolume(); // Retrieve total volume from flow meter
    finalVolume -= 0.140;
    Serial.print("Total Volume: "); // Print total volume message
    Serial.print(finalVolume); // Print total volume value
    Serial.println(" L"); // Print unit (liters)
    sendFlowMeterToEndpoint(finalVolume); // Send total volume to server
    // Reset all variables
    totalVolume = 0.0; // Reset total volume
    flowMeter->setTotalVolume(0); // Reset flow meter total volume
    flowMeter->reset(); // Reset flow meter
    pulseDetected = false; // Reset pulse detected flag
  }

  // ================= Hitung Flow Meter ==========================
  if (pulseDetected) { // If a pulse has been detected
    Serial.print(" l/min; Total volume: "); // Print total volume message
    Serial.print(flowMeter->getTotalVolume()); // Print total volume value
    Serial.println(" l"); // Print unit (liters)
  }

  // Read distance from ultrasonic sensor
  float jarak = sonar.ping_cm(); // Read average distance in cm
  jarak += offset; // Apply offset correction

  if (jarak < 0) jarak = 0; // Prevent negative values

  float tinggi_air = tinggi_aquarium - jarak; // Calculate water height in the tank
  if (tinggi_air < 0) tinggi_air = 0; // Prevent negative height

  Serial.print("Jarak: "); // Print distance message
  Serial.println(tinggi_air); // Print water height

  // Calculate water volume in cm³
  float volume_cm3 = panjang * lebar * tinggi_air; // Calculate volume in cubic centimeters

  // Convert to liters
  float volume_liter = volume_cm3 / 1000.0; // Convert volume to liters

  int volume_liter_int = round(volume_liter); // Round volume to nearest integer

  // Control relays based on water height
  if (tinggi_air < 7) { // If water height is below 7 cm
    digitalWrite(RELAY1_PIN_IN_2, HIGH); // Activate relay for filling water
    delay(2000); // Wait for 2 seconds
    digitalWrite(RELAY1_PIN_IN_1, LOW); // Activate relay for pump
  } else if (tinggi_air > 9) { // If water height is above 9 cm
    digitalWrite(RELAY1_PIN_IN_2, LOW); // Deactivate relay for filling water
    delay(2000); // Wait for 2 seconds
    digitalWrite(RELAY1_PIN_IN_1, HIGH); // Deactivate relay for pump
  }

  // Check if it's time to send tank volume data
  unsigned long currentMillisVolumeTank = millis(); // Get current time in milliseconds
  if (currentMillisVolumeTank - lastSendTimeVolumeTank >= intervalSendVolumeTank) { // If interval has passed
    lastSendTimeVolumeTank = currentMillisVolumeTank; // Update last send time
    sendVolumeTankToEndpoint(volume_liter_int); // Uncomment to send volume data to server
  }

  delay(500); // Delay for 50 ms
}
