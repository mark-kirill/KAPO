#include <M5Atom.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <mbedtls/md.h>

Preferences prefs;
AsyncWebServer server(80);
Ticker rebootTicker;

//crypto
const char *PREF_CRYPTO_KEY = "crypto_key";
String cryptoKey = "";  // key
bool cryptoEnabled = false;

// ---------- Config ----------
String apSSID = "KAPO_TEAM";
String apPass = "66666666";

//wifi kasutaja
bool wifiClientMode = false;
String clientSSID = "";
String clientPassword = "";

uint8_t currentRed = 0, currentGreen = 0, currentBlue = 0;

const char *PREF_NS = "wifi-info";
const char *PREF_SSID = "ssid";
const char *PREF_PASS = "password";
const char *PREF_SESSION = "session";

const char *AUTH_USER = "admin";
const char *AUTH_PASS = "1234";

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
void setupAPmode() {
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
}
// wifi connect nagu klient
bool connectToWiFiClient() {
  if (clientSSID.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(clientSSID.c_str(), clientPassword.c_str());

  Serial.print("Connecting to WiFi ");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    wifiClientMode = true;
    return true;
  } else {
    Serial.println("\nFailed to connect");
    // Возвращаемся в режим AP
    setupAPmode();
    return false;
  }
}

void handleSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd == "ap") {
            Serial.println("Switching to AP mode...");
            setupAPmode();
        } else if (cmd == "client") {
            Serial.println("Switching to Client mode...");
            connectToWiFiClient();
        } else if (cmd == "status") {
            Serial.println("WiFi Mode: " + String(wifiClientMode ? "CLIENT" : "AP"));
            if (wifiClientMode) {
                Serial.println("Client IP: " + WiFi.localIP().toString());
            } else {
                Serial.println("AP IP: " + WiFi.softAPIP().toString());
            }
        }
    }
}
// Функция проверки HMAC подписи
bool verifyHMAC(String message, String receivedSignature) {
  if (!cryptoEnabled || cryptoKey.length() == 0) return false;

  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *)cryptoKey.c_str(), cryptoKey.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char *)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  // Конвертируем в hex строку
  char calculatedSignature[65];
  for (int i = 0; i < 32; i++) {
    sprintf(calculatedSignature + (i * 2), "%02x", hmacResult[i]);
  }
  calculatedSignature[64] = 0;

  return (receivedSignature.equals(calculatedSignature));
}

// Функция для загрузки крипто-ключа
void setupCryptoKey() {
  prefs.begin(PREF_NS, true);
  cryptoKey = prefs.getString(PREF_CRYPTO_KEY, "");
  prefs.end();

  cryptoEnabled = (cryptoKey.length() > 0);
  Serial.println("Crypto key: " + String(cryptoEnabled ? "LOADED" : "NOT SET"));
}

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

// ---------- Safe reboot ----------
void safeReboot() {
  delay(1000);
  ESP.restart();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  // Инициализация M5Atom с правильными параметрами
  M5.begin(true, false, true);  // serial, I2C, display
  delay(100);                   // пауза для стабилизации

  // Инициализация LED матрицы
  M5.dis.setBrightness(50);    // установка яркости
  setColorFromHex("#000000");  // начальный черный цвет

  // Инициализация LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS initialization failed!");
    return;
  }

  prefs.begin(PREF_NS, true);
  String savedSSID = prefs.getString(PREF_SSID, "");
  String savedPASS = prefs.getString(PREF_PASS, "");
  cryptoKey = prefs.getString(PREF_CRYPTO_KEY, "");
  prefs.end();

  cryptoEnabled = (cryptoKey.length() > 0);
  Serial.println("Crypto key: " + String(cryptoEnabled ? "LOADED" : "NOT SET"));

  // 🔥 ВАЖНО: Пытаемся подключиться как клиент, если есть сохраненные данные
  if (savedSSID.length() > 0 && savedPASS.length() >= 8) {
    clientSSID = savedSSID;
    clientPassword = savedPASS;

    Serial.println("Attempting to connect as WiFi client to: " + clientSSID);
    if (connectToWiFiClient()) {
      // Успешно подключились как клиент
      Serial.println("Successfully connected as WiFi client");
    } else {
      // Не удалось - переходим в режим AP
      Serial.println("Failed to connect as client, starting AP mode");
      setupAPmode();
    }
  } else {
    // Нет сохраненных credentials - запускаем AP режим
    Serial.println("No saved WiFi credentials, starting AP mode");
    setupAPmode();
  }

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
    if (wifiClientMode && cryptoEnabled) {
      if (!request->hasParam("signature")) {
        request->send(401, "text/plain", "Signature required in client mode");
        return;
      }
      String signature = request->getParam("signature")->value();
      String message = "";
      if (request->hasParam("value")) {
        message = request->getParam("value")->value();
      }

      if (!verifyHMAC(message, signature)) {
        request->send(401, "text/plain", "Invalid signature");
        return;
      }
    }
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

server.on("/debugkey", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isRequestAuthenticated(request)) {
        requireAuthOrRedirect(request);
        return;
    }
    
    String keyPreview = cryptoKey.length() > 5 ? 
        cryptoKey.substring(0, 3) + "..." + cryptoKey.substring(cryptoKey.length()-3) : 
        "empty";
        
    request->send(200, "text/plain", 
        "Crypto key: " + keyPreview + "\n" +
        "Key length: " + String(cryptoKey.length()) + "\n" +
        "Crypto enabled: " + String(cryptoEnabled ? "true" : "false")
    );
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


  ///cryptoEndpoint
  server.on("/setcryptokey", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Только в режиме AP можно установить ключ
    if (wifiClientMode) {
      request->send(403, "text/plain", "Can only set key in AP mode");
      return;
    }

    if (request->hasParam("key", true)) {
      String newKey = request->getParam("key", true)->value();

      if (newKey.length() >= 8) {
        prefs.begin(PREF_NS, false);
        prefs.putString(PREF_CRYPTO_KEY, newKey);
        prefs.end();

        cryptoKey = newKey;
        cryptoEnabled = true;

        request->send(200, "text/plain", "Crypto key set successfully");
      } else {
        request->send(400, "text/plain", "Key must be at least 8 characters");
      }
    } else {
      request->send(400, "text/plain", "Missing key parameter");
    }
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
      rebootTicker.once(2.0, safeReboot);  // Увеличено время для гарантированной отправки ответа
    } else {
      request->send(400, "text/plain", "Invalid SSID or password (min 8 characters)");
    }
  });

  // Обнови endpoint /status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool auth = isRequestAuthenticated(request);
    String wifiMode = wifiClientMode ? "client" : "ap";
    String ip = wifiClientMode ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

    String body = "{\"color\":\"" + getColorHex() + "\",\"authenticated\":" + String(auth ? "true" : "false") + ",\"wifi_mode\":\"" + wifiMode + "\",\"ip\":\"" + ip + "\",\"crypto_enabled\":" + String(cryptoEnabled ? "true" : "false") + "}";

    request->send(200, "application/json", body);
  });

  server.on("/switchmode", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!isRequestAuthenticated(request)) {
      requireAuthOrRedirect(request);
      return;
    }

    if (request->hasParam("mode", true)) {
      String mode = request->getParam("mode", true)->value();

      if (mode == "ap") {
        setupAPmode();
        request->send(200, "text/plain", "Switched to AP mode");
      } else if (mode == "client") {
        if (connectToWiFiClient()) {
          request->send(200, "text/plain", "Switched to Client mode. IP: " + WiFi.localIP().toString());
        } else {
          request->send(500, "text/plain", "Failed to connect as client");
        }
      } else {
        request->send(400, "text/plain", "Invalid mode. Use 'ap' or 'client'");
      }
    } else {
      request->send(400, "text/plain", "Missing mode parameter");
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


Serial.println("Available endpoints:");
Serial.println("  http://" + WiFi.localIP().toString() + "/");
Serial.println("  http://" + WiFi.localIP().toString() + "/status");
Serial.println("  http://" + WiFi.localIP().toString() + "/set?value=#FF0000");
Serial.println("  http://" + WiFi.localIP().toString() + "/login.html");
}

void loop() {
  M5.update();  // Обновление состояния кнопок M5Atom
  handleSerialCommands();
  delay(50);
}