#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <FlowMeter.h>
#include <NewPing.h>
#include <math.h>

#define MQ2_PIN 35 // Analog PIn
#define FLAME_SENSOR_PIN 4
#define BUZZER_PIN  27      // Pin buzzer (ubah sesuai kebutuhan)
#define RELAY1_PIN_IN_1    23  
#define RELAY1_PIN_IN_2    22
#define RELAY1_PIN_IN_3    21  
#define RELAY1_PIN_IN_4    17
#define BUTTON_START       5
#define BUTTON_END         18
#define WATER_FLOW_SENSOR  15
// Definisi pin
#define TRIGGER_PIN 33 // pin trigger HC-SR04 ke D5
#define ECHO_PIN    34 // pin echo HC-SR04 ke D18
#define MAX_DISTANCE 300 // Maksimal jarak pengukuran (dalam cm)


// Dimensi tangki
const float tinggi_aquarium = 26.0; // cm
const float panjang = 35.0;         // cm
const float lebar = 21.5;           // cm

bool IS_FLAME_DETECTED = false;
bool IS_SMOKE_DETECTED = false;


const char* ssid = "ABYAN";
const char* password = "KeluargaIdaman";

// Enpoint To Save Data
const char* saveSmokeSensorURL = "https://hare-proud-ghastly.ngrok-free.app/api/sensors/smoke";
const char* saveFlowMeterURL = "https://hare-proud-ghastly.ngrok-free.app/api/sensors/flow-meter";
const char* saveVolumeTankURL = "https://hare-proud-ghastly.ngrok-free.app/api/sensors/volume";
const char* saveFlameSensorURL= "https://hare-proud-ghastly.ngrok-free.app/api/sensors/flame";

// Flow meter properties
FlowSensorProperties sensorProperties = {
  30.0f,    // maxFlowRate
  2.9,      // K-Factor Pulse
  {0.8197, 0.7692, 0.8955, 0.5556, 1, 1, 1, 1, 1, 1}
};

FlowMeter* flowMeter;

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

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
const float offset = 2.0; // Koreksi selisih pengukuran (2 cm)


unsigned long lastSendTimeVolumeTank = 0; // Waktu terakhir kirim data untuk volume air dalam tangki
const unsigned long intervalSendVolumeTank = 15000; // Interval kirim data (15 detik) untuk volume air dalam tangki


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


// Fungsi utama
void setup() {
  Serial.begin(115200);
  connectToWiFi();
  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Konfigurasi PWM untuk buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  //setup relay
  pinMode(RELAY1_PIN_IN_1, OUTPUT);
  pinMode(RELAY1_PIN_IN_2, OUTPUT);
  pinMode(RELAY1_PIN_IN_3, OUTPUT);
  pinMode(RELAY1_PIN_IN_4, OUTPUT);

  // setup button
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_END, INPUT_PULLUP);
  // Set relay awal OFF (Pastikan pompa mati saat startup)
  digitalWrite(RELAY1_PIN_IN_1, HIGH);
  digitalWrite(RELAY1_PIN_IN_2, LOW);
  digitalWrite(RELAY1_PIN_IN_3, HIGH);
  digitalWrite(RELAY1_PIN_IN_4, HIGH);
  // Setup Flow Meter
  flowMeter = new FlowMeter(digitalPinToInterrupt(WATER_FLOW_SENSOR), sensorProperties, flowInterrupt, FALLING);
}

// Fungsi untuk menghubungkan ESP32 ke WiFi
void connectToWiFi() {
  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(ssid, password);
  int maxAttempts = 20; // Batas percobaan koneksi WiFi

  while (WiFi.status() != WL_CONNECTED && maxAttempts-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Terhubung ke WiFi!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Gagal terhubung ke WiFi! Periksa kredensial atau jaringan.");
  }
}

// Fungsi untuk mengirim data sensor asap ke server
void sendSmokeSensorToEndpoint(bool smokeDetected) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["smoke_detected"] = smokeDetected;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.begin(client, saveSmokeSensorURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", "b5a3c2f8-9e1d-4d7b-8e6a-f4b5d3a2e9c1"); // Ganti dengan API key Anda

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan.");
  }
  http.end();
}

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
  http.addHeader("X-API-KEY", "b5a3c2f8-9e1d-4d7b-8e6a-f4b5d3a2e9c1"); // Ganti dengan API key Anda

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan.");
  }
  http.end();
}

void sendVolumeTankToEndpoint(float currentVolumeLiter) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["current_volume"] = currentVolumeLiter;
  jsonDoc["max_tank"] = 24;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.begin(client, saveVolumeTankURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", "b5a3c2f8-9e1d-4d7b-8e6a-f4b5d3a2e9c1"); // Ganti dengan API key Anda

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan.");
  }
  http.end();
}

void sendFlameSensorToEndpoint(bool isFlameDetected) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Tidak terhubung ke WiFi, data tidak dikirim.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);

  StaticJsonDocument<200> jsonDoc;
  jsonDoc["flame_detected"] = isFlameDetected;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.begin(client, saveFlameSensorURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", "b5a3c2f8-9e1d-4d7b-8e6a-f4b5d3a2e9c1"); // Ganti dengan API key Anda

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.print("✅ HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("❌ Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Gagal mengirim data! Periksa URL atau jaringan.");
  }
  http.end();
}

void loop() {
  // sensor API
  bool flameStatus = digitalRead(FLAME_SENSOR_PIN);  // Membaca sensor api
  if (flameStatus == 0 && !IS_FLAME_DETECTED) {  
      Serial.println("🔥 Api Terdeteksi! Buzzer Aktif!");
      // sendFlameSensorToEndpoint(true);
      digitalWrite(BUZZER_PIN, HIGH);
      IS_FLAME_DETECTED = true;
  } else if (flameStatus == 1 && IS_FLAME_DETECTED) {
      Serial.println("✅ Tidak ada api. Buzzer Mati.");
      // sendFlameSensorToEndpoint(false);
      digitalWrite(BUZZER_PIN, LOW);
      IS_FLAME_DETECTED = false;
  }
  // // // SMOKE DETECTED
  bool smokeStatus = digitalRead(MQ2_PIN);
  if (smokeStatus == 0 && !IS_SMOKE_DETECTED) {  
      Serial.println("Asap Terdeteksi ! Buzzer Aktif!");
      // sendSmokeSensorToEndpoint(true);
      digitalWrite(BUZZER_PIN, HIGH);
      IS_SMOKE_DETECTED = true;
  } else if (smokeStatus == 1 && IS_SMOKE_DETECTED) {
      Serial.println("✅ Tidak ada Asap. Buzzer Mati.");
      // sendSmokeSensorToEndpoint(false);
      digitalWrite(BUZZER_PIN, LOW);
      IS_SMOKE_DETECTED = false;
  }
  flowMeter->tick();

  unsigned long currentMillis = millis();
  int stateStart = digitalRead(BUTTON_START);
  int stateEnd = digitalRead(BUTTON_END);

  // Turn On Pump and solenoid vale
  if(stateStart == LOW && !measuring) {
    measuring = true;
    calibrating = true;
    calibrationPulseCount = 0;
    pulseDetected = false;
    totalVolume = 0.0;

    flowStartTime = currentMillis;
    flowLastTime = currentMillis;
   lastTime = currentMillis;
    digitalWrite(RELAY1_PIN_IN_3, LOW);
    delay(200);
    digitalWrite(RELAY1_PIN_IN_4, LOW);
    Serial.println("Pompa AKTIF - Mulai Pengukuran (Kalibrasi)");
  }


  // // Tombol End
  if (stateEnd == LOW && measuring) {
    measuring = false;
    calibrating = false;
    // Matikan pompa
    digitalWrite(RELAY1_PIN_IN_3, HIGH);
    delay(500);
    digitalWrite(RELAY1_PIN_IN_4, HIGH);
    Serial.println("Pompa NONAKTIF - Pengukuran Selesai");

    // Ambil total volume
    double finalVolume = flowMeter->getTotalVolume();
    Serial.print("Total Volume: ");
    Serial.print(finalVolume);
    Serial.println(" L");

    sendFlowMeterToEndpoint(finalVolume);

    Serial.print("Total Pulse Count: ");
    Serial.println(calibrationPulseCount);

    Serial.print("Total Flow Rate: ");
    Serial.println(flowMeter->getTotalFlowrate());
    // Reset semua variabel
    totalVolume = 0.0;
    flowMeter->setTotalVolume(0);
    flowMeter->reset();
    pulseDetected = false;
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
 

  float jarak = sonar.ping_cm(); // Membaca rata-rata jarak
  jarak+=offset;

  if (jarak < 0) jarak = 0; // Cegah nilai negatif
  
  float tinggi_air = tinggi_aquarium - jarak; // Hitung tinggi air
  if (tinggi_air < 0) tinggi_air = 0; // Cegah nilai negatif lagi
  Serial.print("Jarak: ");
  Serial.println(tinggi_air);
//  Hitung volume air dalam cm³
  float volume_cm3 = panjang * lebar * tinggi_air; 

  // Ubah ke liter
  float volume_liter = volume_cm3 / 1000.0;

  int volume_liter_int = round(volume_liter);

  // Tinggal ganti sesuai dengan pin
  if(tinggi_air < 7){
    digitalWrite(RELAY1_PIN_IN_2, HIGH);
    delay(2000);
    digitalWrite(RELAY1_PIN_IN_1, LOW);
  } else if(tinggi_air > 9){
    digitalWrite(RELAY1_PIN_IN_2, LOW);
    delay(2000);
    digitalWrite(RELAY1_PIN_IN_1, HIGH);
  }
  unsigned long currentMillisVolumeTank = millis();
  Serial.println(jarak);
  if (currentMillisVolumeTank - lastSendTimeVolumeTank  >= intervalSendVolumeTank) {
    lastSendTimeVolumeTank = currentMillisVolumeTank;
    // sendVolumeTankToEndpoint(volume_liter_int);
  }
  delay(50);
  yield();
}
