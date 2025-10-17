#include <M5Atom.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <Preferences.h>
#include <DNSServer.h>

// WiFi ssid ja salasõna seadistamise jaoks
Preferences prefs;
String ssid = "KAPO_TEAM";
String password = "66666666";

//Paneme käima WiFi ja DNS serverid
const byte DNS_PORT = 53;
DNSServer dnsServer;
WiFiServer server(80);

// Praeguse LED värvi hoidmiseks
uint8_t currentRed = 0;
uint8_t currentGreen = 0;
uint8_t currentBlue = 0;

// Autentifitseerimise bool (päris projektis tuleks kasutada kas sessioone või tokeneid)
bool isAuthenticated = true;
String authUsername = "admin";
String authPassword = "1234";

// Funktsioonide prototüübid
void sendAutoRedirect(WiFiClient client);
void sendAuthPage(WiFiClient client);
void handleLogin(WiFiClient client, String request);
void sendHTMLResponse(WiFiClient &client);
void handleSetRequest(WiFiClient &client, String request);
String getColorHex();
void setColorFromHex(String hexColor);
bool checkAuthentication(String request);
void handleWiFiSettings(WiFiClient &client, String request);
void clearPreferences();

void setup() {
  Serial.begin(115200);
  M5.begin(true, false, true);  // Paneme käima M5Atom (LED, Serial, I2C)

  //Loeme wifi andmed mälus
  clearPreferences(); //Kasutame kui soovime puhastada M5Atom mälu, muidu las jääb välja kommenteerituna
  prefs.begin("wifi-info", true);
  ssid = prefs.getString("ssid", "KAPO_TEAM");
  password = prefs.getString("password", "66666666");
  prefs.end();

  //Jätame aega qr koodi skripri jaoks
  delay(5000);
  
  //QR koodi skripti jaoks
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("password: ");
  Serial.println(password);

  Serial.println("\nWIFI ACCESS POINT");
  Serial.printf("Please connect: %s \nThen access to:", ssid);

  WiFi.softAP(ssid.c_str(), password.c_str());  // Loome AP
  IPAddress myIP = WiFi.softAPIP();  // Meie IP aadress
  Serial.println(myIP);

  // Käivitame DNS, et kõik domeenid viisid meie IP-ni
  dnsServer.start(DNS_PORT, "*", myIP);

  server.begin();  // Server läheb tääle
  Serial.println("Server started on port 80");
}

void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();  // Ootame kliendi

  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    String request = "";
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        request += c;

        // HTTP päringu lõpp
        if (c == '\n') {
          // Kontrollime autentimist kaitsud leheküljede jaoks
          bool requiresAuth = (request.indexOf("GET / ") != -1 || 
                              request.indexOf("GET /get") != -1 || 
                              request.indexOf("GET /set") != -1);
          
          if (requiresAuth && !isAuthenticated) {
            // Kasutaja pole autentinud, seega saadame sisselogimise lehele
            sendAutoRedirect(client);
          }
          else if (request.indexOf("GET / ") != -1 && isAuthenticated) {
            // Põhilehekülg autentifitseeritud kasutajatele
            sendHTMLResponse(client);
          }
          else if (request.indexOf("GET /auth") != -1) {
            // Autentimise lehekülg
            sendAuthPage(client);
          }
          else if (request.indexOf("GET /get") != -1 && isAuthenticated) {
            sendHTMLResponse(client);
          }
          else if (request.indexOf("GET /set?value=") != -1 && isAuthenticated) {
            handleSetRequest(client, request);
          }
          else if (request.indexOf("POST /login") != -1) {
            // Autentimise andmete töötlemine
            handleLogin(client, request);
          }
          else if (request.indexOf("GET /logout") != -1) {
            // Süsteemist väljumine
            isAuthenticated = false;
            sendAutoRedirect(client);
          } 
          else if (request.indexOf("GET /setwifi?") != -1 && isAuthenticated) {
              // SSID ja salasõna muutmine
              handleWiFiSettings(client, request);
          }
          else {
            // Ülejäänud päringute jaoks
            if (isAuthenticated) {
              sendHTMLResponse(client);
            } else {
              sendAutoRedirect(client);
            }
          }
          break;
        }
      }
    }
    
    client.stop();
    Serial.println("Client disconnected");
  }
}

//Kustutame mälust SSID ja salasõna
void clearPreferences() {
  prefs.begin("wifi-info", false);
  prefs.clear();
  prefs.end();
}

//Funktsioon Wifi ssid ja salasõna muutmiseks 
void handleWiFiSettings(WiFiClient &client, String request) {
  int ssidIndex = request.indexOf("ssid=");
  int passIndex = request.indexOf("password=");

  if (ssidIndex != -1 && passIndex != -1) {
    String newSSID = request.substring(ssidIndex + 5, request.indexOf("&", ssidIndex));
    String newPass = request.substring(passIndex + 9, request.indexOf(" HTTP/1.1", passIndex));

    // Asendame "+" tühikuga (URL jaoks)
    newSSID.replace("+", " ");
    newPass.replace("+", " ");

    // Salvestame
    prefs.begin("wifi-info", false); // write mode
    prefs.putString("ssid", newSSID);
    prefs.putString("password", newPass);
    prefs.end();

    // Vastus kliendile
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<html><body>");
    client.println("<h2>WiFi credentials are changed</h2>");
    client.println("<p>New SSID: " + newSSID + "</p>");
    client.println("<p>Restarting M5Atom Lite...</p>");
    client.println("</body></html>");

    delay(5000);
    ESP.restart();  // Taaskäivitame M5Atom
  } else {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("Not enough new credentials to change WiFi.");
  }
}

// Funktsioon automaatskes ümbersuunamiseks
void sendAutoRedirect(WiFiClient client) {
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /auth");
  client.println("Connection: close");
  client.println();
}

// Autentimise lehekülje loomine
void sendAuthPage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>Authentication Required</title>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; margin: 40px; background-color: #f5f5f5; }");
  client.println(".auth-form { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 300px; margin: 0 auto; }");
  client.println("input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 8px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }");
  client.println("input[type='submit'] { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; }");
  client.println("input[type='submit']:hover { background-color: #45a049; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='auth-form'>");
  client.println("<h2>Authentication Required</h2>");
  client.println("<form action='/login' method='POST'>");
  client.println("<label for='username'>Username:</label>");
  client.println("<input type='text' id='username' name='username' value='admin' required>");
  client.println("<br>");
  client.println("<label for='password'>Password:</label>");
  client.println("<input type='password' id='password' name='password' value='1234' required>");
  client.println("<br><br>");
  client.println("<input type='submit' value='Login'>");
  client.println("</form>");
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

// Logini töötlemise funktsioon
void handleLogin(WiFiClient client, String request) {
  // Parsime "form" andmeid
  String username = "";
  String password = "";
  
  int bodyStart = request.indexOf("\r\n\r\n");
  if (bodyStart != -1) {
    String body = request.substring(bodyStart + 4);
    int userPos = body.indexOf("username=");
    int passPos = body.indexOf("password=");
    
    if (userPos != -1 && passPos != -1) {
      username = body.substring(userPos + 9, body.indexOf('&', userPos));
      password = body.substring(passPos + 9);
      
      // URL decode 
      username.replace("+", " ");
      password.replace("+", " ");
    }
  }
  
  // Kontrollime credentials
  Serial.println(authPassword +' '+authUsername);
  if (username == authUsername && password == authPassword) {
    isAuthenticated = true;
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<head>");
    client.println("<title>Login Successful</title>");
    client.println("<meta http-equiv='refresh' content='1;url=/'>");  // Перенаправление на главную
    client.println("</head>");
    client.println("<body>");
    client.println("<h2>Login Successful!</h2>");
    client.println("<p>Redirecting to main page...</p>");
    client.println("</body>");
    client.println("</html>");
    
    Serial.println("Login successful");
  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<head>");
    client.println("<title>Login Failed</title>");
    client.println("<meta http-equiv='refresh' content='2;url=/auth'>");  // Возврат к логину
    client.println("</head>");
    client.println("<body>");
    client.println("<h2>Login Failed!</h2>");
    client.println("<p>Invalid username or password. Redirecting...</p>");
    client.println("</body>");
    client.println("</html>");
    
    Serial.println("Login failed");
  }
}

void sendHTMLResponse(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<title>M5Atom LED Control</title></head>");
  client.println("<body>");
  client.println("<h1>M5Atom LED Control</h1>");
  
  // Nupp väljumiseks
  client.println("<div style='float: right;'><a href='/logout'>Logout</a></div>");
  
  String colorHex = getColorHex();
  client.print("<p>Current LED color: ");
  client.print(colorHex);
  client.print("</p>");
  
  // Tähtis on jutumärkide õige ekraneerimine(kas õigesti eesti keeles?)
  client.print("<p>");
  client.print("Click <a href=\"/set?value=%23FF0000\">here</a> to turn ON RED the LED.<br>");
  client.print("Click <a href=\"/set?value=%2300FF00\">here</a> to turn on Green the LED.<br>");
  client.print("Click <a href=\"/set?value=%230000FF\">here</a> to turn on Blue the LED.<br>");
  client.print("Click <a href=\"/set?value=%23000000\">here</a> to turn OFF the LED.");
  client.print("</p>");
  
  //Color wheel
  client.print("<form action=\"/set\" method=\"GET\">");
  client.print("<input type=\"color\" id=\"value\" name=\"value\" value=\"" + colorHex + "\">");
  client.print("<input type=\"submit\" value=\"Set Color\">");
  client.print("</form>");

  // Form, et käsitsi trükkida värvi  
  client.print("<form action=\"/set\" method=\"GET\">");
  client.print("Custom color (RRGGBB): #<input type=\"text\" name=\"value\" value=\"");
  client.print(colorHex.substring(1)); // Без #
  client.print("\" maxlength=\"6\" pattern=\"[0-9A-Fa-f]{6}\">");
  client.print("<input type=\"submit\" value=\"Set Color\">");
  client.print("</form>");
  
  // Form, et muuta credentials
  client.println("<h1>Change WiFi AP Credentials</h1>");
  client.println("<form action=\"/setwifi\" method=\"GET\">");
  client.println("SSID: <input type=\"text\" name=\"ssid\" value=\"" + ssid + "\"><br>");
  client.println("Password: <input type=\"text\" name=\"password\" value=\"" + password + "\" minlength=\"8\"><br>");
  client.println("<input type=\"submit\" value=\"Change WiFi Settings\">");
  client.println("</form>");

  client.println("</body>");
  client.println("</html>");
}

void handleSetRequest(WiFiClient &client, String request) {
  int startIndex = request.indexOf("value=");
  if (startIndex != -1) {
    String color = request.substring(startIndex + 6);
    // Извлекаем значение до пробела или конца строки
    int endIndex = color.indexOf(" ");
    if (endIndex != -1) {
      color = color.substring(0, endIndex);
    }
    
    // Kustume võimalikke liigseid sümboleid
    color.trim();
    
    // Dekodeerime URL-encoding: %23 -> #
    color.replace("%23", "#");
    
    Serial.print("Decoded color: ");
    Serial.println(color);
    
    // Kui puudub "#", siis lisame seda
    if (color.length() == 6 && color[0] != '#') {
      color = "#" + color;
    }
    
    if (color.length() == 7 && color[0] == '#') {
      setColorFromHex(color);
      
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      
      client.println("<!DOCTYPE HTML>");
      client.println("<html>");
      client.println("<head><title>Color Set</title></head>");
      client.println("<body>");
      client.print("<h2>LED color set to: ");
      client.print(color);
      client.print("</h2>");
      client.print("<p><a href=\"/\">Back to main page</a></p>");
      client.println("</body>");
      client.println("</html>");
    } else {
      client.println("HTTP/1.1 400 Bad Request");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.print("Invalid color format. Use RRGGBB format.");
      client.print("<br>Received: '");
      client.print(color);
      client.print("'");
      client.print("<br><a href=\"/\">Back to main page</a>");
    }
  } else {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.print("Missing value parameter");
    client.print("<br><a href=\"/\">Back to main page</a>");
  }
}

// Funktsioon praeguse värvi saamiseks HEX formaadis
String getColorHex() {
  char hexColor[8];
  sprintf(hexColor, "#%02X%02X%02X", currentRed, currentGreen, currentBlue);
  return String(hexColor);
}

// Funktsioon värvi määramiseks HEX-väärtuse kaudu
void setColorFromHex(String hexColor) {
  if (hexColor.length() == 7 && hexColor[0] == '#') {
    String redStr = hexColor.substring(1, 3);
    String greenStr = hexColor.substring(3, 5);
    String blueStr = hexColor.substring(5, 7);

    currentRed = strtol(redStr.c_str(), NULL, 16);
    currentGreen = strtol(greenStr.c_str(), NULL, 16);
    currentBlue = strtol(blueStr.c_str(), NULL, 16);

    // Loome värvi 32-bit formaadis
    uint32_t color = (currentRed << 16) | (currentGreen << 8) | currentBlue;

    // LED värvi kehtestamine
    M5.dis.drawpix(0, color);
  }
}