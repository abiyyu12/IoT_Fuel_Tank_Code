#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <FlowMeter.h>

// Definisi pin
#define MQ2_PIN            35
#define FLAME_SENSOR_PIN   4
#define TRIG_PIN           33
#define ECHO_PIN           34
#define BUZZER_PIN         27
#define RELAY1_PIN_IN_1    21
#define RELAY1_PIN_IN_2    17
#define BUTTON_START       5
#define BUTTON_END         18
#define WATER_FLOW_SENSOR  15

// WiFi & Endpoint
const char* ssid = "ABYAN";
const char* password = "KeluargaIdaman";
const char* saveFlowMeterURL = "https://hare-proud-ghastly.ngrok-free.app/api/sensors/flow-meter";

// Flow meter properties
FlowSensorProperties sensorProperties = {
  30.0f,    // maxFlowRate
  3.75f,   
  {1.0592f,1,1,1,1,1,1,1,1,1}
};


FlowMeter* flowMeter;

bool pulseDetected = false;
bool measuring = false;
unsigned long flowStartTime = 0;
unsigned long flowLastTime = 0;
unsigned long lastTime = 0;
unsigned long lastDuration = 0;
unsigned long period = 1000; // 1 detik
unsigned long timeOut = 3000; // timeout 3 detik

double totalVolume = 0.0;

volatile unsigned long calibrationPulseCount = 0;
bool calibrating = false;


// ISR
IRAM_ATTR void flowInterrupt() {
  flowMeter->count();
  if (calibrating) {
    calibrationPulseCount++;
  }
  if (!pulseDetected) {
    pulseDetected = true;
    flowStartTime = millis();
  }
  flowLastTime = millis();
}


// Setup
void setup() {
  Serial.begin(115200);
  connectToWiFi();

  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY1_PIN_IN_1, OUTPUT);
  pinMode(RELAY1_PIN_IN_2, OUTPUT);
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_END, INPUT_PULLUP);

  digitalWrite(RELAY1_PIN_IN_1, HIGH);
  digitalWrite(RELAY1_PIN_IN_2, HIGH);

  flowMeter = new FlowMeter(digitalPinToInterrupt(WATER_FLOW_SENSOR), sensorProperties, flowInterrupt, FALLING);
  Serial.println("FlowMeter initialized");
}

// WiFi connect
void connectToWiFi() {
  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(ssid, password);
  int maxAttempts = 20;
  while (WiFi.status() != WL_CONNECTED && maxAttempts-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Terhubung ke WiFi!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Gagal terhubung ke WiFi!");
  }
}

// Kirim data ke endpoint
void sendFlowMeterToEndpoint(float literWaterFlow) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(5000);

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["flow_liter"] = literWaterFlow;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.begin(client, saveFlowMeterURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", "b5a3c2f8-9e1d-4d7b-8e6a-f4b5d3a2e9c1");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// Loop
void loop() {
  flowMeter->tick();

  unsigned long currentMillis = millis();
  int stateStart = digitalRead(BUTTON_START);
  int stateEnd = digitalRead(BUTTON_END);

  // Tombol Start
  if (stateStart == LOW && !measuring) {
  measuring = true;
  calibrating = true;
  calibrationPulseCount = 0;
  pulseDetected = false;
  totalVolume = 0.0;

  flowStartTime = currentMillis;
  flowLastTime = currentMillis;
  lastTime = currentMillis;

  digitalWrite(RELAY1_PIN_IN_2, LOW);
  delay(500);
  digitalWrite(RELAY1_PIN_IN_1, LOW);
  Serial.println("Pompa AKTIF - Mulai Pengukuran (Kalibrasi)");
}


  // Tombol End
  if (stateEnd == LOW && measuring) {
  measuring = false;
  calibrating = false;

  // Matikan pompa
  digitalWrite(RELAY1_PIN_IN_1, HIGH);
  delay(200);
  digitalWrite(RELAY1_PIN_IN_2, HIGH);
  Serial.println("Pompa NONAKTIF - Pengukuran Selesai");

  // Ambil total volume
  double finalVolume = flowMeter->getTotalVolume();
  Serial.print("Total Volume: ");
  Serial.print(finalVolume);
  Serial.println(" L");

  Serial.print("Total Pulse Count: ");
  Serial.println(calibrationPulseCount);

  Serial.print("Total Flow Rate: ");
  Serial.println(flowMeter->getTotalFlowrate());

  if (finalVolume > 0) {
    float pulsesPerLiter = calibrationPulseCount / finalVolume;
    float kFactor = (60.0 * pulsesPerLiter) / 1000.0;

    Serial.print("🔍 Estimated pulses/liter: ");
    Serial.println(pulsesPerLiter);
    

    Serial.print("🔧 Estimated K-Factor (L/min): ");
    Serial.println(kFactor, 3);
  } else {
    Serial.println("⚠️ Volume terdeteksi 0 L, kalibrasi tidak valid.");
  }

  // Reset semua variabel
  totalVolume = 0.0;
  calibrationPulseCount = 0;
  flowMeter->setTotalVolume(0);
  flowMeter->reset();
  pulseDetected = false;
  delay(1000);
}



  if (pulseDetected) {
  unsigned long currentTime = millis();
  unsigned long duration = currentTime - lastTime;

  if (currentTime - flowLastTime >= timeOut) {
    lastDuration = (currentTime - flowStartTime - timeOut) / 1000;
    Serial.print("\nLast Total Volume = " + String(flowMeter->getTotalVolume()) + " l; ");
    Serial.println("Last Duration Time = " + String(lastDuration) + " s");

    pulseDetected = false;
    lastDuration = 0;
    Serial.println("Menunggu aliran baru...");
  } else if (duration >= period) {
    if (duration > 0 && duration < 10000) {
      flowMeter->update(duration);

      double currentVolume = flowMeter->getCurrentVolume();
      double currentFlow = flowMeter->getCurrentFlowrate();

      if (!isnan(currentVolume) && !isnan(currentFlow)) {
        Serial.print("Current flow rate: ");
        Serial.print(currentFlow);
        Serial.print(" l/min; Total volume: ");
        Serial.print(flowMeter->getTotalVolume());
        Serial.println(" l");
      } else {
        Serial.println("❌ Terjadi NaN saat pembacaan flow meter!");
      }
      lastTime = currentTime;
    }
  }
}


  delay(50);
}
