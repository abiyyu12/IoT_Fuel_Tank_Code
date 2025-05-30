#define RELAY1_PIN_IN_1    21  
#define RELAY1_PIN_IN_2    17
#define BUTTON_START       5
#define BUTTON_END         18
#define WATER_FLOW_SENSOR  15

volatile unsigned long pulse = 0;         // variable untuk menghitung jumlah pulse
unsigned long intrpMillis = 0;
unsigned long currentMillis;
const int pulseTimeOut = 3000;            // waktu tunggu sebelum perhitungan total jumlah pulse
bool counterRun = 0; 

IRAM_ATTR void handleInterrupt() {
   pulse++;
   intrpMillis = currentMillis;
}

void setup() {
  Serial.begin(115200);

  pinMode(WATER_FLOW_SENSOR, INPUT); 
  attachInterrupt(digitalPinToInterrupt(WATER_FLOW_SENSOR), handleInterrupt, FALLING);

  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_END, INPUT_PULLUP);

  pinMode(RELAY1_PIN_IN_1, OUTPUT);
  pinMode(RELAY1_PIN_IN_2, OUTPUT);

  // Pastikan pompa mati saat startup
  digitalWrite(RELAY1_PIN_IN_1, HIGH);
  digitalWrite(RELAY1_PIN_IN_2, HIGH);

  delay(200);
  Serial.println("\nWaiting for pulse in....\n");
}

void loop() {
  int stateStart = digitalRead(BUTTON_START);
  int stateEnd   = digitalRead(BUTTON_END);

  currentMillis = millis();

  if (stateStart == LOW) {
    // Tombol start ditekan → relay ON
    digitalWrite(RELAY1_PIN_IN_2, LOW);
    delay(100);
    digitalWrite(RELAY1_PIN_IN_1, LOW);
    Serial.println("Relay ON (START button pressed)");
    delay(300);  // debounce
  }

  if (stateEnd == LOW) {
    // Tombol end ditekan → relay OFF
    digitalWrite(RELAY1_PIN_IN_1, HIGH);
    delay(100);
    digitalWrite(RELAY1_PIN_IN_2, HIGH);
    Serial.println("Relay OFF (END button pressed)");
    delay(300);  // debounce
  }
  currentMillis = millis();
    
   // jika pulse sudah tidak 0 dan waktu timeout sudah tercapai
   // maka hitung jumlah pulse yang masuk
   if((pulse>0) && (currentMillis-intrpMillis >= pulseTimeOut)) {
      Serial.println("\nPulse ending");
      Serial.println("Pulse count = "+String(pulse));

      // Tampilkan nilai K-Factor jika cairan yang dimasukkan 1L
      // Hanya valid jika flowmeter AICHI yang digunakan
      // Flowmeter lain menyesuaikan 
      if (pulse >= 150 && pulse <=160) {
         // variable pulse dengan tipe data int diperlakukan sebagai float
         // agar hasil perhitungan nya menghasilkan angka desimal   
         // dengan printf seperti ini, perintah nya cukup 1 baris saja    
         Serial.printf ("K-Factor = %.2f\n",(float)pulse/60);         
      }     
      delay(100);
      Serial.println("\n...Waiting for pulse...\n");

      // reset pulse counter, reset flag counterRun
      pulse = 0;
      counterRun = 0;      
   } 
   // selama pulsa terdeteksi       
   // Saat pulse lebih besar dari 0
   // dieksekusi hanya sekali karena menggunakan else if   
   else if(pulse>0) {
      counterRun = 1;
      Serial.print("Start counting");
   }
}
