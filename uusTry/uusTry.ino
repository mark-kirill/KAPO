#include <M5Atom.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>

Preferences prefs;
AsyncWebServer server(80);
Ticker rebootTicker;

// ---------- Config ----------
String apSSID = "KAPO_TEAM";
String apPass = "66666666";

uint8_t currentRed = 0, currentGreen = 0, currentBlue = 0;

const char* PREF_NS = "wifi-info";
const char* PREF_SSID = "ssid";
const char* PREF_PASS = "password";
const char* PREF_SESSION = "session";

const char* AUTH_USER = "admin";
const char* AUTH_PASS = "1234";

// ---------- Helpers ----------
String getColorHex() {
  char buf[8];
  sprintf(buf, "#%02X%02X%02X", currentRed, currentGreen, currentBlue);
  return String(buf);
}

bool validateHexColor(const String &hex) {
  if (hex.length() != 7) return false;
  if (hex.charAt(0) != '#') return false;
  for (int i = 1; i < 7; i++) 
    if (!isxdigit(hex.charAt(i))) return false;
  return true;
}

void setColorFromHex(const String &hex) {
  if (!validateHexColor(hex)) return;
  currentRed = strtol(hex.substring(1, 3).c_str(), NULL, 16);
  currentGreen = strtol(hex.substring(3, 5).c_str(), NULL, 16);
  currentBlue = strtol(hex.substring(5, 7).c_str(), NULL, 16);
  uint32_t color = ((uint32_t)currentRed << 16) | ((uint32_t)currentGreen << 8) | currentBlue;
  M5.dis.drawpix(0, color);
}

bool isRequestAuthenticated(AsyncWebServerRequest *request) {
  if (!request || !request->hasHeader("Cookie")) return false;
  const AsyncWebHeader* h = request->getHeader("Cookie");
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

// ---------- Safe reboot ----------
void safeReboot() {
  delay(1000);
  ESP.restart();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  
  // Инициализация M5Atom с правильными параметрами
  M5.begin(true, false, true); // serial, I2C, display
  delay(100); // пауза для стабилизации
  
  // Инициализация LED матрицы
  M5.dis.setBrightness(50); // установка яркости
  setColorFromHex("#000000"); // начальный черный цвет

  // Инициализация LittleFS
  if (!LittleFS.begin()) { 
    Serial.println("LittleFS initialization failed!");
    return;
  }

  // Загрузка сохраненных WiFi credentials
  prefs.begin(PREF_NS, true);
  String savedSSID = prefs.getString(PREF_SSID, "");
  String savedPASS = prefs.getString(PREF_PASS, "");
  prefs.end();

  // Используем сохраненные credentials если они есть
  if (savedSSID.length() > 0 && savedPASS.length() >= 8) {
    apSSID = savedSSID;
    apPass = savedPASS;
  }

  // Настройка WiFi в режиме AP
  WiFi.mode(WIFI_AP);
  
  // Установка конфигурации AP
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), 
                   IPAddress(192, 168, 4, 1), 
                   IPAddress(255, 255, 255, 0));
  
  // Запуск точки доступа
  if (!WiFi.softAP(apSSID.c_str(), apPass.c_str())) {
    Serial.println("Failed to start AP!");
  } else {
    Serial.println("AP started successfully");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
  
  delay(100);

  // Обработка статических файлов
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Status API
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool auth = isRequestAuthenticated(request);
    String body = "{\"color\":\"" + getColorHex() + "\",\"authenticated\":" + String(auth ? "true" : "false") + "}";
    request->send(200, "application/json", body);
  });

  // Set color endpoint
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isRequestAuthenticated(request)) {
      requireAuthOrRedirect(request); 
      return; 
    }
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Missing value"); 
      return; 
    }
    String val = request->getParam("value")->value();
    val.replace("%23", "#"); 
    val.trim();
    if (val.length() == 6 && val.charAt(0) != '#') val = "#" + val;
    if (validateHexColor(val)) {
      setColorFromHex(val);
      request->send(200, "text/html", "<html><body><h3>Color set to " + val + "</h3><a href='/'>Back</a></body></html>");
    } else {
      request->send(400, "text/plain", "Invalid color format: " + val);
    }
  });

  // Login endpoint
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    String user = "", pass = "";
    if (request->hasParam("username", true)) user = request->getParam("username", true)->value();
    if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();
    
    if (user.equals(AUTH_USER) && pass.equals(AUTH_PASS)) {
      String token = String(millis(), HEX) + String(random(1000), HEX);
      prefs.begin(PREF_NS, false);
      prefs.putString(PREF_SESSION, token);
      prefs.end();
      
      AsyncWebServerResponse *resp = request->beginResponse(302, "text/plain", "");
      resp->addHeader("Set-Cookie", "session=" + token + "; Path=/; HttpOnly");
      resp->addHeader("Location", "/");
      request->send(resp);
    } else {
      AsyncWebServerResponse *resp = request->beginResponse(302, "text/plain", "");
      resp->addHeader("Location", "/login.html?fail=1");
      request->send(resp);
    }
  });

  // Logout endpoint
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    prefs.begin(PREF_NS, false);
    prefs.remove(PREF_SESSION);
    prefs.end();
    
    AsyncWebServerResponse *resp = request->beginResponse(302, "text/plain", "");
    resp->addHeader("Set-Cookie", "session=; Path=/; Max-Age=0");
    resp->addHeader("Location", "/login.html");
    request->send(resp);
  });

  // WiFi settings endpoint
  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isRequestAuthenticated(request)) {
      requireAuthOrRedirect(request); 
      return; 
    }
    String newSsid = "", newPass = "";
    if (request->hasParam("ssid", true)) newSsid = request->getParam("ssid", true)->value();
    if (request->hasParam("password", true)) newPass = request->getParam("password", true)->value();
    
    if (newSsid.length() > 0 && newPass.length() >= 8) {
      prefs.begin(PREF_NS, false);
      prefs.putString(PREF_SSID, newSsid);
      prefs.putString(PREF_PASS, newPass);
      prefs.end();
      
      request->send(200, "text/html", "<html><body><h3>Credentials saved. Rebooting...</h3></body></html>");
      rebootTicker.once(2.0, safeReboot); // Увеличено время для гарантированной отправки ответа
    } else {
      request->send(400, "text/plain", "Invalid SSID or password (min 8 characters)");
    }
  });

  // 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
      request->send(404, "text/plain", "File not found");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  M5.update(); // Обновление состояния кнопок M5Atom
  delay(50);
}
