#include <M5Atom.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <ESPping.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <CircularBuffer.hpp>

// ========== CIRCULAR BUFFER FOR SENSOR DATA ==========
struct SensorReading {
  unsigned long timestamp;
  bool valveOpen;
  float pressure;
};

CircularBuffer<SensorReading, 500> sensorBuffer;
bool bufferHasUnsentData = false;

// ========== CONFIGURATION ==========
Preferences prefs;
AsyncWebServer server(80);
Ticker rebootTicker;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// TEST MODE
bool testMode = false;
int testVolume = 2;
int testDataIndex = 0;

// 🔥 MQTT CONTROL
bool mqttSendingEnabled = false;  // По умолчанию отправка выключена

// Реальные данные давления (кПа) для разных объемов шприца
const float testData[3][10] = {
  // 2ml
  { 150.71, 147.5, 144.23, 146.12, 146.04, 146.42, 148.79, 145.54, 150.46, 149.85 },
  // 5ml
  { 622.38, 615.07, 618.11, 623.33, 632.47, 628.17, 619.59, 620.73, 621.75, 627.02 },
  // 8ml
  { 2046.81, 2054.13, 2049.87, 2062.68, 2041.27, 2049.86, 2050.26, 2048.09, 2044.4, 2034.23 }
};

const int BUFFER_THRESHOLD = 450;
const int RECORDS_PER_PACKET = 50;

// WiFi Configuration
String apSSID = "VALVE_CONFIG";
String apPass = "66666666";
bool wifiClientMode = false;
String clientSSID = "";
String clientPassword = "";

// MQTT Configuration
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqttServer = "";
int mqttPort = 1883;
String mqttUser = "";
String mqttPassword = "";
String mqttBaseTopic = "sensors";
String deviceUID = "";
bool mqttInitSent = false;

// Preferences keys
const char *PREF_NS = "valve-config";
const char *PREF_SSID = "ssid";
const char *PREF_PASS = "password";
const char *PREF_SESSION = "session";
const char *PREF_MQTT_SERVER = "mqtt_srv";
const char *PREF_MQTT_PORT = "mqtt_port";
const char *PREF_MQTT_USER = "mqtt_user";
const char *PREF_MQTT_PASS = "mqtt_pass";
const char *PREF_MQTT_TOPIC = "mqtt_topic";
const char *PREF_DEVICE_UID = "device_uid";

// Auth
const char *AUTH_USER = "admin";
const char *AUTH_PASS = "1234";

// Valve and sensor state
bool valveOpen = false;
float currentPressure = 0.0;
float pressure30msAgo = 0.0;
unsigned long lastPressureRead = 0;

// Pressure sensor pin
const int PRESSURE_PIN = 32;

// ========== DEVICE UID ==========
String generateDeviceUID() {
  uint64_t chipid = ESP.getEfuseMac();
  char uid[17];
  sprintf(uid, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(uid);
}

// ========== LED HELPER ==========
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  M5.dis.drawpix(0, color);
}

void updateLEDStatus() {
  if (!wifiClientMode) {
    setLEDColor(0, 0, 255);  // Blue - AP mode
  } else if (!mqttClient.connected()) {
    setLEDColor(255, 255, 0);  // Yellow - no MQTT
  } else if (!mqttSendingEnabled) {
    // 🔥 ПАУЗA - фиолетовый цвет (мигание)
    static bool blinkState = false;
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
      blinkState = !blinkState;
      lastBlink = millis();
    }
    setLEDColor(blinkState ? 128 : 0, 0, blinkState ? 128 : 0);  // Purple blink
  } else if (valveOpen) {
    setLEDColor(0, 255, 0);  // Green - valve open
  } else {
    setLEDColor(255, 0, 0);  // Red - valve closed
  }
}

// ========== TEST MODE PRESSURE ==========
float getTestPressure() {
  static float lastTestPressure = 0.0;
  
  int volumeIndex = (testVolume == 2) ? 0 : (testVolume == 5) ? 1 : 2;
  float pressure_kPa = testData[volumeIndex][testDataIndex];
  float pressure_bar = pressure_kPa / 100.0;
  
  // УМНАЯ ЛОГИКА КЛАПАНА
  if (testDataIndex == 0) {
    valveOpen = true;
    updateLEDStatus();
    Serial.println("🟢 Test: Valve OPENED - starting injection");
  } else if (testDataIndex >= 9) {
    valveOpen = false;
    updateLEDStatus();
    Serial.println("🔴 Test: Valve CLOSED - cycle complete");
  } else if (pressure_bar < lastTestPressure) {
    if (valveOpen) {
      valveOpen = false;
      updateLEDStatus();
      Serial.println("🔴 Test: Valve CLOSED - pressure dropping (" + 
                     String(pressure_bar, 2) + " < " + String(lastTestPressure, 2) + " bar)");
    }
  } else if (pressure_bar > lastTestPressure) {
    if (!valveOpen) {
      valveOpen = true;
      updateLEDStatus();
      Serial.println("🟢 Test: Valve OPENED - pressure rising (" + 
                     String(pressure_bar, 2) + " > " + String(lastTestPressure, 2) + " bar)");
    }
  }
  
  lastTestPressure = pressure_bar;

  testDataIndex++;
  if (testDataIndex >= 10) {
    testDataIndex = 0;
    valveOpen = false;
    updateLEDStatus();
    
    if (testVolume == 2) testVolume = 5;
    else if (testVolume == 5) testVolume = 8;
    else testVolume = 2;

    Serial.println("📊 Test: switching to " + String(testVolume) + "ml");
    delay(500);
  }

  return pressure_bar;
}

// ========== PRESSURE SENSOR ==========
float readPressure() {
  if (testMode) {
    return getTestPressure();
  }
  
  int adcValue = analogRead(PRESSURE_PIN);
  float voltage = (adcValue / 4095.0) * 3.3;
  float pressure_kPa = (voltage - 0.2) / (4.7 - 0.2) * 700.0;
  return pressure_kPa / 100.0;
}

void updatePressureReadings() {
  unsigned long now = millis();
  static unsigned long lastBufferAdd = 0;

  if (now - lastPressureRead >= 30) {
    pressure30msAgo = currentPressure;
    lastPressureRead = now;
  }

  currentPressure = readPressure();
  
  // 🔥 ИСПРАВЛЕНО: правильное условие для добавления в буфер
  bool shouldAdd = false;
  if(mqttSendingEnabled){
  if (sensorBuffer.isEmpty()) {
    shouldAdd = true;  // Первая запись
  } else if (now - lastBufferAdd > 50) {
    shouldAdd = true;  // Прошло 50ms
  } else if (valveOpen != sensorBuffer.last().valveOpen) {
    shouldAdd = true;  // Изменилось состояние клапана
  } else if (currentPressure > 0) {
    shouldAdd = true;  
  }
  }
  
  if (shouldAdd) {
    SensorReading reading;
    
    // ПРАВИЛЬНЫЙ TIMESTAMP
    if (testMode) {
      reading.timestamp = now;  // millis для теста
    } else {
      unsigned long epochTime = timeClient.getEpochTime();
      
      // ⚠️ Проверяем что NTP синхронизировано
      if (epochTime < 1000000000) {  // Меньше Sep 2001 = не синхронизировано
        Serial.println("⚠️ NTP not synced, using millis()");
        reading.timestamp = now;
      } else {
        reading.timestamp = epochTime * 1000ULL + (now % 1000);
      }
    }
    
    reading.valveOpen = valveOpen;
    reading.pressure = currentPressure;
    sensorBuffer.push(reading);
    bufferHasUnsentData = true;
    lastBufferAdd = now;
  }
}

// ========== MQTT FUNCTIONS ==========
String loadHTMLFragment() {
  File file = LittleFS.open("/device_fragment.html", "r");
  if (!file) {
    Serial.println("Failed to open device_fragment.html");
    return "<div>Device: " + deviceUID + "</div>";
  }
  String content = file.readString();
  file.close();
  return content;
}

void sendInitMessage() {
  if (!mqttClient.connected()) {
    Serial.println("❌ Cannot send init: MQTT not connected");
    return;
  }

  String topic = mqttBaseTopic + "/" + deviceUID + "/init";
  String htmlFragment = loadHTMLFragment();

  Serial.println("📤 Sending init message...");
  Serial.println("   Topic: " + topic);
  Serial.println("   Size: " + String(htmlFragment.length()) + " bytes");

  if (mqttClient.publish(topic.c_str(), htmlFragment.c_str(), true)) {
    Serial.println("✅ Init message sent successfully");
    mqttInitSent = true;
  } else {
    Serial.println("❌ Failed to send init message");
    Serial.println("   MQTT state: " + String(mqttClient.state()));
    Serial.println("   Buffer size: " + String(mqttClient.getBufferSize()));
    Serial.println("   Message size: " + String(htmlFragment.length()));
    
    // Если сообщение слишком большое
    if (htmlFragment.length() > mqttClient.getBufferSize()) {
      Serial.println("   ⚠️ Message too large for MQTT buffer!");
    }
  }
}

void publishSensorData() {
  if (!mqttSendingEnabled) {
    return;
  }
   if (sensorBuffer.size() % 100 == 0 && sensorBuffer.size() > 0) {
      Serial.println("📊 Buffer accumulating: " + String(sensorBuffer.size()) + "/500 (sending paused)");
    }
  
  if (!mqttClient.connected() || sensorBuffer.isEmpty()) return;
  
  static unsigned long lastForcedSend = 0;
  bool bufferFull = sensorBuffer.size() >= BUFFER_THRESHOLD;
  bool timeoutReached = (millis() - lastForcedSend) > 10000;

  if (!bufferFull && !timeoutReached) {
    return;
  }

  String topic = mqttBaseTopic + "/" + deviceUID + "/data";
  int recordsToSend = min((int)sensorBuffer.size(), RECORDS_PER_PACKET);
  
  if (recordsToSend == 0) return;

  String payload = "{\"uid\":\"" + deviceUID + "\",\"count\":" + String(recordsToSend) + ",\"data\":[";
  SensorReading tempBuffer[RECORDS_PER_PACKET];

  for (int i = 0; i < recordsToSend; i++) {
    tempBuffer[i] = sensorBuffer.shift();

    if (i > 0) payload += ",";

    payload += "{";
    payload += "\"t\":" + String(tempBuffer[i].timestamp) + ",";
    payload += "\"v\":" + String(tempBuffer[i].valveOpen ? 1 : 0) + ",";
    payload += "\"p\":" + String(tempBuffer[i].pressure, 2);
    payload += "}";

    if (payload.length() > 4000) {
      for (int j = i; j < recordsToSend; j++) {
        sensorBuffer.unshift(tempBuffer[j]);
      }
      recordsToSend = i;
      break;
    }
  }
  
  payload += "]}";

  bool success = mqttClient.publish(topic.c_str(), payload.c_str(), 0);
  
  if (success) {
    Serial.println("✅ Sent " + String(recordsToSend) + " records | Buffer: " + 
                   String(sensorBuffer.size()) + "/500 | Size: " + String(payload.length()) + "b");
    lastForcedSend = millis();
  } else {
    Serial.println("❌ Send failed! Returning " + String(recordsToSend) + " records to buffer");
    for (int i = recordsToSend - 1; i >= 0; i--) {
      sensorBuffer.unshift(tempBuffer[i]);
    }
  }

  bufferHasUnsentData = !sensorBuffer.isEmpty();
}

void reconnectMQTT() {
  if (mqttServer.length() == 0) return;

  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = deviceUID + "-" + String(ESP.getEfuseMac(), HEX);
    
    bool connected = false;
    if (mqttUser.length() > 0) {
      connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str());
    } else {
      connected = mqttClient.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println(" Connected!");
      mqttInitSent = false;
      sendInitMessage();
      bufferHasUnsentData = true;
    } else {
      Serial.print(" Failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// ========== NTP ==========
void setupNTP() {
  timeClient.begin();
  timeClient.setTimeOffset(0);

  Serial.print("Getting NTP time...");
  int attempts = 0;
  while (!timeClient.update() && attempts < 20) {  // Увеличено до 20 попыток
    timeClient.forceUpdate();
    delay(1000);  // Увеличена задержка до 1 секунды
    Serial.print(".");
    attempts++;
  }

  if (timeClient.isTimeSet() && timeClient.getEpochTime() > 1000000000) {
    Serial.println(" Success! UTC time: " + String(timeClient.getEpochTime()));
    Serial.println("   Date: " + timeClient.getFormattedTime());
  } else {
    Serial.println(" Failed! Will use millis() for timestamps");
    Serial.println("   Check your internet connection and NTP server availability");
  }
}

// Функция для обновления NTP в loop
void updateNTP() {
  static unsigned long lastNTPUpdate = 0;
  
  if (millis() - lastNTPUpdate > 60000) {  // Каждую минуту
    if (!timeClient.update()) {
      Serial.println("⚠️ NTP update failed");
    } else {
      unsigned long epoch = timeClient.getEpochTime();
      if (epoch > 1000000000) {
        Serial.println("✅ NTP updated: " + String(epoch));
      }
    }
    lastNTPUpdate = millis();
  }
}

// ========== WIFI SETUP ==========
void setupAPmode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));

  if (WiFi.softAP(apSSID.c_str(), apPass.c_str())) {
    Serial.println("AP started: " + apSSID);
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    wifiClientMode = false;
  } else {
    Serial.println("Failed to start AP!");
  }
}

bool connectToWiFiClient() {
  if (clientSSID.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(clientSSID.c_str(), clientPassword.c_str());

  Serial.print("Connecting to WiFi: " + clientSSID);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    wifiClientMode = true;
    setupNTP();
    return true;
  } else {
    Serial.println("\nFailed to connect");
    setupAPmode();
    return false;
  }
}

// ========== SERIAL COMMANDS ==========
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("wifi ")) {
      String params = cmd.substring(5);
      int splitIndex = params.indexOf(';');
      if (splitIndex == -1) {
        Serial.println("ERROR: Use format: wifi SSID;password");
        return;
      }

      String ssid = params.substring(0, splitIndex);
      String pass = params.substring(splitIndex + 1);
      ssid.trim();
      pass.trim();

      if (pass.length() >= 8) {
        prefs.begin(PREF_NS, false);
        prefs.putString(PREF_SSID, ssid);
        prefs.putString(PREF_PASS, pass);
        prefs.end();

        clientSSID = ssid;
        clientPassword = pass;

        Serial.println("WiFi credentials saved. Reconnecting...");
        connectToWiFiClient();
      } else {
        Serial.println("Password must be at least 8 characters");
      }
    } 
    else if (cmd.startsWith("mqtt ")) {
      int idx1 = cmd.indexOf(' ', 5);
      if (idx1 < 0) {
        Serial.println("Usage: mqtt <server> <port> <user> <pass>");
        return;
      }

      int idx2 = cmd.indexOf(' ', idx1 + 1);
      if (idx2 < 0) {
        Serial.println("Usage: mqtt <server> <port> <user> <pass>");
        return;
      }

      int idx3 = cmd.indexOf(' ', idx2 + 1);
      if (idx3 < 0) {
        Serial.println("Usage: mqtt <server> <port> <user> <pass>");
        return;
      }

      String srv = cmd.substring(5, idx1);
      String portStr = cmd.substring(idx1 + 1, idx2);
      String user = cmd.substring(idx2 + 1, idx3);
      String pass = cmd.substring(idx3 + 1);

      int port = portStr.toInt();
      if (port <= 0) {
        Serial.println("Invalid port");
        return;
      }

      prefs.begin(PREF_NS, false);
      prefs.putString(PREF_MQTT_SERVER, srv);
      prefs.putInt(PREF_MQTT_PORT, port);
      prefs.putString(PREF_MQTT_USER, user);
      prefs.putString(PREF_MQTT_PASS, pass);
      prefs.end();

      mqttServer = srv;
      mqttPort = port;
      mqttUser = user;
      mqttPassword = pass;

      mqttClient.setServer(mqttServer.c_str(), mqttPort);

      Serial.println("MQTT configured:");
      Serial.println("  Server: " + mqttServer);
      Serial.println("  Port:   " + String(mqttPort));
      Serial.println("  User:   " + mqttUser);
    } 
    else if (cmd == "status") {
      Serial.println("=== STATUS ===");
      Serial.println("Device UID: " + deviceUID);
      Serial.println("WiFi Mode: " + String(wifiClientMode ? "CLIENT" : "AP"));
      Serial.println("IP: " + String(wifiClientMode ? WiFi.localIP().toString() : WiFi.softAPIP().toString()));
      Serial.println("Valve: " + String(valveOpen ? "OPEN" : "CLOSED"));
      Serial.println("Pressure: " + String(currentPressure, 2) + " bar");
      Serial.println("MQTT: " + String(mqttClient.connected() ? "Connected" : "Disconnected"));
      Serial.println("MQTT Sending: " + String(mqttSendingEnabled ? "✅ ENABLED" : "⏸️ PAUSED"));
      Serial.println("Buffer size: " + String(sensorBuffer.size()) + "/500");
      Serial.println("Test mode: " + String(testMode ? "ON" : "OFF"));
      
      // NTP статус
      unsigned long epoch = timeClient.getEpochTime();
      Serial.println("NTP time: " + String(epoch) + " (" + 
                     (epoch > 1000000000 ? "✅ synced" : "❌ not synced") + ")");
      if (epoch > 1000000000) {
        Serial.println("  Current time: " + timeClient.getFormattedTime());
      }
    } 
    else if (cmd == "start") {
      mqttSendingEnabled = true;
      Serial.println("▶️ MQTT sending ENABLED");
      Serial.println("   Buffer will be sent when threshold reached (" + String(BUFFER_THRESHOLD) + " records)");
      Serial.println("   Current buffer: " + String(sensorBuffer.size()) + "/500");
    }
    else if (cmd == "stop") {
      mqttSendingEnabled = false;
      Serial.println("⏸️ MQTT sending PAUSED");
      Serial.println("   Data will continue accumulating in buffer");
      Serial.println("   Use 'start' to resume sending");
    } 
    else if (cmd == "ntp") {
      Serial.println("🔄 Force NTP sync...");
      if (timeClient.forceUpdate()) {
        unsigned long epoch = timeClient.getEpochTime();
        Serial.println("✅ NTP synced! Epoch: " + String(epoch));
        Serial.println("   Time: " + timeClient.getFormattedTime());
      } else {
        Serial.println("❌ NTP sync failed!");
      }
    } 
    else if (cmd == "stats") {
      Serial.println("=== BUFFER STATS ===");
      Serial.println("Current size: " + String(sensorBuffer.size()) + "/500");
      Serial.println("Usage: " + String(sensorBuffer.size() * 100 / 500) + "%");
      Serial.println("Threshold: " + String(BUFFER_THRESHOLD));
      Serial.println("Records per packet: " + String(RECORDS_PER_PACKET));
      Serial.println("Is full: " + String(sensorBuffer.isFull() ? "YES" : "NO"));
    } 
    else if (cmd == "test on") {
      testMode = true;
      testVolume = 2;
      testDataIndex = 0;
      valveOpen = false;
      Serial.println("🧪 Test mode ENABLED - using real syringe data (2ml->5ml->8ml cycle)");
      Serial.println("📋 Test logic:");
      Serial.println("   - Valve OPENS when pressure rises");
      Serial.println("   - Valve CLOSES when pressure drops");
      Serial.println("   - Valve CLOSES at end of each cycle");
      Serial.println("Starting with 2ml data...");
    } 
    else if (cmd == "test off") {
      testMode = false;
      valveOpen = false;
      updateLEDStatus();
      Serial.println("🧪 Test mode DISABLED - reading from sensor");
    } 
    else if (cmd.startsWith("test ")) {
      String volStr = cmd.substring(5);
      int vol = volStr.toInt();
      if (vol == 2 || vol == 5 || vol == 8) {
        testVolume = vol;
        testDataIndex = 0;
        testMode = true;
        valveOpen = false;
        Serial.println("🧪 Test mode: switched to " + String(vol) + "ml data");
        Serial.println("   Expected pressure range: " + 
                       String(vol == 2 ? "1.44-1.51" : vol == 5 ? "6.15-6.32" : "20.34-20.63") + " bar");
      } else {
        Serial.println("❌ Invalid volume. Use: test 2, test 5, or test 8");
      }
    } 
    else if (cmd == "open") {
      valveOpen = true;
      updateLEDStatus();
      Serial.println("Valve opened");
    } 
    else if (cmd == "close") {
      valveOpen = false;
      updateLEDStatus();
      Serial.println("Valve closed");
    } 
    else if (cmd == "help") {
      Serial.println("=== COMMANDS ===");
      Serial.println("wifi <ssid>;< password> - Configure WiFi");
      Serial.println("mqtt <server> <port> <user> <pass> - Configure MQTT");
      Serial.println("status - Show current status");
      Serial.println("stats - Show buffer statistics");
      Serial.println("test on - Enable test mode (auto cycle)");
      Serial.println("test off - Disable test mode");
      Serial.println("test 2/5/8 - Test specific volume");
      Serial.println("open/close - Manual valve control");
    }
  }
}

// ========== AUTH ==========
bool isRequestAuthenticated(AsyncWebServerRequest *request) {
  if (!request || !request->hasHeader("Cookie")) return false;
  const AsyncWebHeader *h = request->getHeader("Cookie");
  String cookies = h->value();
  int idx = cookies.indexOf("session=");
  if (idx == -1) return false;
  int start = idx + 8;
  int end = cookies.indexOf(';', start);
  String token = (end == -1) ? cookies.substring(start) : cookies.substring(start, end);
  prefs.begin(PREF_NS, true);
  String saved = prefs.getString(PREF_SESSION, "");
  prefs.end();
  return token.length() && token == saved;
}

void requireAuthOrRedirect(AsyncWebServerRequest *request) {
  request->redirect("/login.html");
}

void safeReboot() {
  delay(1000);
  ESP.restart();
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  M5.begin(true, false, true);
  delay(100);
  M5.dis.setBrightness(50);
  setLEDColor(255, 0, 0);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  
  sensorBuffer.clear();

  prefs.begin(PREF_NS, true);
  clientSSID = prefs.getString(PREF_SSID, "");
  clientPassword = prefs.getString(PREF_PASS, "");
  mqttServer = prefs.getString(PREF_MQTT_SERVER, "");
  mqttPort = prefs.getInt(PREF_MQTT_PORT, 1883);
  mqttUser = prefs.getString(PREF_MQTT_USER, "");
  mqttPassword = prefs.getString(PREF_MQTT_PASS, "");
  mqttBaseTopic = prefs.getString(PREF_MQTT_TOPIC, "sensors");
  deviceUID = "testseade";//prefs.getString(PREF_DEVICE_UID, "");
  prefs.end();

  if (deviceUID.length() == 0) {
    deviceUID = generateDeviceUID();
    prefs.begin(PREF_NS, false);
    prefs.putString(PREF_DEVICE_UID, deviceUID);
    prefs.end();
    Serial.println("Generated device UID: " + deviceUID);
  } else {
    Serial.println("Device UID: " + deviceUID);
  }

  if (clientSSID.length() > 0 && clientPassword.length() >= 8) {
    connectToWiFiClient();
  } else {
    setupAPmode();
  }

  if (mqttServer.length() > 0) {
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setBufferSize(4096);
    Serial.println("MQTT configured: " + mqttServer + ":" + String(mqttPort));
  }

  // Web server routes (сокращенно, полный код остается как в оригинале)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool auth = isRequestAuthenticated(request);
    String json = "{";
    json += "\"authenticated\":" + String(auth ? "true" : "false") + ",";
    json += "\"device_uid\":\"" + deviceUID + "\",";
    json += "\"valve_state\":\"" + String(valveOpen ? "open" : "closed") + "\",";
    json += "\"pressure\":" + String(currentPressure, 2) + ",";
    json += "\"buffer_size\":" + String(sensorBuffer.size()) + ",";
    json += "\"test_mode\":" + String(testMode ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // ... остальные routes ...

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Type 'help' for commands");
  Serial.println("");
  Serial.println("⚠️  IMPORTANT: MQTT sending is PAUSED by default");
  Serial.println("   Use command 'start' to begin sending data");
  Serial.println("");

  updateLEDStatus();
}

// ========== LOOP ==========
void loop() {
  M5.update();
  handleSerialCommands();
  updatePressureReadings();

  if (wifiClientMode && mqttServer.length() > 0) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    } else {
      mqttClient.loop();

      static unsigned long lastAttempt = 0;
      if (!mqttInitSent && millis() - lastAttempt > 2000) {
        sendInitMessage();
        lastAttempt = millis();
      }

      publishSensorData();

      if (sensorBuffer.isFull()) {
        Serial.println("⚠️ WARNING: Buffer full!");
        setLEDColor(255, 165, 0);
      }
    }
    
    // 🔥 ДОБАВЛЕНО: обновление NTP
    updateNTP();
  }

  updateLEDStatus();
  delay(10);
}