#include <M5Atom.h>
#include <WiFi.h>
#include <WebServer.h>

// ===== НАСТРОЙКИ =====
const int   PIN_ADC   = 33;      // сигнал с LM358 на ESP32
const int   ADC_BITS  = 12;      // 0..4095
const float ADC_VREF  = 3.3f;    // диапазон АЦП ESP32 при ADC_11db

const int   BUTTON_PIN = 39;      // nupp, mis käivitab robotit
volatile bool startUR5 = false;

const float K = 999.84f;   // кПа/В
const float B = 0.059f;   

// усреднение АЦП
const int   AVG_SAMPLES = 8;

// ussivõrgu seaded
const char* ssid = "KAPO-TEAM";
const char* password = "66666666";

// HTTP serveri seaded
WebServer server(80); // Server kuulab porti 80-l

// ussivõrgu ühendamine
void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
}

void IRAM_ATTR buttonPressing() {
  startUR5 = !startUR5; 
}

void setup() {
  Serial.begin(115200);
  M5.begin(true, false, true);
  analogSetWidth(ADC_BITS);
  analogSetPinAttenuation(PIN_ADC, ADC_11db);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  
  delay(5000);

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();  // Meie IP aadress
  //connectToWiFi();
  //Serial.println(WiFi.localIP());
  Serial.println(myIP);
  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPressing, FALLING);

  // HTTP päringute töötlemise määramine
  server.on("/pressure", HTTP_GET, []() {
    // Siit võtame viimase mõõdetud rõhu väärtuse
    Serial.println("Someone got pressure");
    if(startUR5){
      float Vadc = readVadc();
      float PkPa = K * Vadc + B;
      // randomPressure on testimiseks, kasutame seda, kui pole võimalust mõõta rõhku
      //float randomDecimal = random(0, 9)/10.0;
      //float randomPressure = random(100, 200)+randomDecimal;
      Serial.println(PkPa);
      server.send(200, "text/plain", String(PkPa));  // Tagastame rõhu väärtuse kPa
    } else {
      server.send(200, "text/plain", String(-1));
    }
  });

  // Serveri käivitamine
  server.begin();
  Serial.println("HTTP server started");
}

float readVadc() {
  long acc = 0;

  // lihtne "pruukimine" valimis
  (void)analogRead(PIN_ADC);
  delayMicroseconds(80);

  for (int i = 0; i < AVG_SAMPLES; ++i) {
    acc += analogRead(PIN_ADC);
    delayMicroseconds(80);
    Serial.println(acc);
  }

  float raw = acc / (float)AVG_SAMPLES;
  float Vadc = raw * ADC_VREF / 4095.0f;
  return Vadc;
}

void loop() {

  server.handleClient();
  
}
