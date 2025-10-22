// project.ino
#include <M5Atom.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

Preferences prefs;
AsyncWebServer server(80);

// Default AP credentials (можно менять через /wifi.html)
String apSSID = "KAPO_TEAM";
String apPass = "66666666";

// Текущее состояние RGB (0-255)
uint8_t currentRed = 0, currentGreen = 0, currentBlue = 0;

// Ключи в Preferences
const char* PREF_NS = "wifi-info";
const char* PREF_SSID = "ssid";
const char* PREF_PASS = "password";
const char* PREF_SESSION = "session";
const char* AUTH_USER = "admin";
const char* AUTH_PASS = "1234";

// Вспомогательные
String getColorHex();
void setColorFromHex(const String &hex);
bool isRequestAuthenticated(AsyncWebServerRequest *request);
void requireAuthOrRedirect(AsyncWebServerRequest *request);

void setup(){
  Serial.begin(115200);
  M5.begin(true, false, true);

  // read saved AP creds if any
  prefs.begin(PREF_NS, true); // read
  apSSID = prefs.getString(PREF_SSID, apSSID.c_str());
  apPass = prefs.getString(PREF_PASS, apPass.c_str());
  prefs.end();

  Serial.println("Starting AP:");
  Serial.println(apSSID);
  WiFi.softAP(apSSID.c_str(), apPass.c_str());
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(ip);

  if (!LittleFS.begin()){
    Serial.println("LittleFS mount failed!");
    // continue, but static pages won't be served
  } else {
    Serial.println("LittleFS mounted");
  }

  // Serve static files from LittleFS (index, login, wifi, css, js)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // API: status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    bool auth = isRequestAuthenticated(request);
    String body = "{\"color\":\"" + getColorHex() + "\",\"authenticated\":" + String(auth ? "true" : "false") + "}";
    request->send(200, "application/json", body);
  });

  // API: set color via GET /set?value=%23RRGGBB or /set?value=RRGGBB
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!isRequestAuthenticated(request)) { requireAuthOrRedirect(request); return; }
    if (!request->hasParam("value")) {
      request->send(400, "text/plain", "Missing value param");
      return;
    }
    String val = request->getParam("value")->value(); // may contain %23 or #
    val.replace("%23","#");
    val.trim();
    if (val.length() == 6 && val.charAt(0) != '#') val = "#" + val;
    if (val.length() == 7 && val.charAt(0) == '#') {
      setColorFromHex(val);
      String body = "<!DOCTYPE html><html><body><h3>Color set to " + val + "</h3><p><a href=\"/\">Back</a></p></body></html>";
      request->send(200, "text/html", body);
    } else {
      request->send(400, "text/plain", "Invalid color format");
    }
  });

  // Login page is static /login.html, but login handler:
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request){
    // expect form-post fields username and password (standard form submit)
    String user = "";
    String pass = "";
    if (request->hasParam("username", true)) user = request->getParam("username", true)->value();
    if (request->hasParam("password", true)) pass = request->getParam("password", true)->value();

    // simple auth check
    if (user.equals(AUTH_USER) && pass.equals(AUTH_PASS)) {
      // generate simple session token and store in prefs
      String token = String(millis(), HEX);
      prefs.begin(PREF_NS, false);
      prefs.putString(PREF_SESSION, token);
      prefs.end();

      AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
      String cookie = "session=" + token + "; Path=/; HttpOnly";
      response->addHeader("Set-Cookie", cookie);
      response->addHeader("Location", "/");
      request->send(response);
    } else {
      // redirect back to login with failure
      request->send(302, "text/plain", "", String(), {{"Location", "/login.html?fail=1"}});
    }
  });

  // Logout -> clear session
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    prefs.begin(PREF_NS, false);
    prefs.remove(PREF_SESSION);
    prefs.end();
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Set-Cookie", String("session=; Path=/; Max-Age=0"));
    response->addHeader("Location", "/login.html");
    request->send(response);
  });

  // WiFi change page is static /wifi.html, handler:
  server.on("/setwifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!isRequestAuthenticated(request)) { requireAuthOrRedirect(request); return; }
    String newSsid = "";
    String newPass = "";
    if (request->hasParam("ssid", true)) newSsid = request->getParam("ssid", true)->value();
    if (request->hasParam("password", true)) newPass = request->getParam("password", true)->value();

    if (newSsid.length() && newPass.length() >= 8) {
      prefs.begin(PREF_NS, false);
      prefs.putString(PREF_SSID, newSsid);
      prefs.putString(PREF_PASS, newPass);
      prefs.end();

      // reply then restart
      request->send(200, "text/html", "<!DOCTYPE html><html><body><h3>Credentials saved. Rebooting...</h3></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      request->send(400, "text/plain", "Invalid SSID or password");
    }
  });

  // Start server
  server.begin();

  // set initial LED (off)
  setColorFromHex("#000000");
}

void loop(){
  // no blocking code here; Async server runs callbacks
  delay(100);
}

/* ---------- helpers ---------- */

String getColorHex(){
  char buf[8];
  sprintf(buf, "#%02X%02X%02X", currentRed, currentGreen, currentBlue);
  return String(buf);
}

void setColorFromHex(const String &hexColor){
  if (hexColor.length() != 7 || hexColor.charAt(0) != '#') return;
  String r = hexColor.substring(1,3);
  String g = hexColor.substring(3,5);
  String b = hexColor.substring(5,7);
  currentRed = (uint8_t) strtol(r.c_str(), NULL, 16);
  currentGreen = (uint8_t) strtol(g.c_str(), NULL, 16);
  currentBlue = (uint8_t) strtol(b.c_str(), NULL, 16);
  uint32_t color = ((uint32_t)currentRed << 16) | ((uint32_t)currentGreen << 8) | currentBlue;
  M5.dis.drawpix(0, color);
  Serial.printf("LED set to %s (R=%d G=%d B=%d)\n", hexColor.c_str(), currentRed, currentGreen, currentBlue);
}

bool isRequestAuthenticated(AsyncWebServerRequest *request){
  // check cookie "session"
  if (!request) return false;
  if (request->hasHeader("Cookie")) {
    AsyncWebHeader* h = request->getHeader("Cookie");
    String cookies = h->value();
    // parse session cookie
    int idx = cookies.indexOf("session=");
    if (idx != -1) {
      int start = idx + 8;
      int end = cookies.indexOf(';', start);
      String token = (end == -1) ? cookies.substring(start) : cookies.substring(start, end);
      prefs.begin(PREF_NS, true);
      String saved = prefs.getString(PREF_SESSION, "");
      prefs.end();
      if (token.length() && token == saved) return true;
    }
  }
  return false;
}

void requireAuthOrRedirect(AsyncWebServerRequest *request){
  // redirect to login page
  request->redirect("/login.html");
}
