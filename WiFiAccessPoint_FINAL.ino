#include <M5Atom.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>

// Set your WiFi credentials
const char *ssid = "KAPO_TEAM";
const char *password = "66666666";

WiFiServer server(80);

// Для хранения текущего цвета светодиода
uint8_t currentRed = 0;
uint8_t currentGreen = 0;
uint8_t currentBlue = 0;

// Флажок аутентификации (в реальном проекте используйте сессии или токены)
bool isAuthenticated = false;
String authUsername = "admin";
String authPassword = "1234";

// Прототипы функций
void sendAutoRedirect(WiFiClient client);
void sendAuthPage(WiFiClient client);
void handleLogin(WiFiClient client, String request);
void sendHTMLResponse(WiFiClient &client);
void handleSetRequest(WiFiClient &client, String request);
String getColorHex();
void setColorFromHex(String hexColor);
bool checkAuthentication(String request);

void setup() {
  M5.begin(true, false, true);  // Инициализация M5Atom (LED, Serial, I2C)
  Serial.println("\nWIFI ACCESS POINT");
  Serial.printf("Please connect: %s \nThen access to:", ssid);

  WiFi.softAP(ssid, password);  // Создаём точку доступа
  IPAddress myIP = WiFi.softAPIP();  // Получаем IP адрес
  Serial.println(myIP);
  
  server.begin();  // Запускаем сервер
  Serial.println("Server started on port 80");
}

void loop() {
  WiFiClient client = server.available();  // Ожидаем клиента

  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    String request = "";
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        request += c;

        // Конец HTTP запроса
        if (c == '\n') {
          // Проверяем аутентификацию для защищенных страниц
          bool requiresAuth = (request.indexOf("GET / ") != -1 || 
                              request.indexOf("GET /get") != -1 || 
                              request.indexOf("GET /set") != -1);
          
          if (requiresAuth && !isAuthenticated) {
            // Пользователь не аутентифицирован - отправляем на страницу логина
            sendAutoRedirect(client);
          }
          else if (request.indexOf("GET / ") != -1 && isAuthenticated) {
            // Главная страница для аутентифицированных пользователей
            sendHTMLResponse(client);
          }
          else if (request.indexOf("GET /auth") != -1) {
            // Страница аутентификации
            sendAuthPage(client);
          }
          else if (request.indexOf("GET /get") != -1 && isAuthenticated) {
            sendHTMLResponse(client);
          }
          else if (request.indexOf("GET /set?value=") != -1 && isAuthenticated) {
            handleSetRequest(client, request);
          }
          else if (request.indexOf("POST /login") != -1) {
            // Обработка данных аутентификации
            handleLogin(client, request);
          }
          else if (request.indexOf("GET /logout") != -1) {
            // Выход из системы
            isAuthenticated = false;
            sendAutoRedirect(client);
          }
          else {
            // Для любых других запросов
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

// Функция для автоматического перенаправления
void sendAutoRedirect(WiFiClient client) {
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /auth");
  client.println("Connection: close");
  client.println();
}

// Функция для отправки страницы аутентификации
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

// Функция для обработки логина
void handleLogin(WiFiClient client, String request) {
  // Парсим данные формы
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
      
      // URL decode (простой вариант)
      username.replace("+", " ");
      password.replace("+", " ");
    }
  }
  
  // Проверяем credentials
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
  
  // Кнопка выхода
  client.println("<div style='float: right;'><a href='/logout'>Logout</a></div>");
  
  String colorHex = getColorHex();
  client.print("<p>Current LED color: ");
  client.print(colorHex);
  client.print("</p>");
  
  // Правильное экранирование кавычек
  client.print("<p>");
  client.print("Click <a href=\"/set?value=%23FF0000\">here</a> to turn ON RED the LED.<br>");
  client.print("Click <a href=\"/set?value=%2300FF00\">here</a> to turn on Green the LED.<br>");
  client.print("Click <a href=\"/set?value=%230000FF\">here</a> to turn on Blue the LED.<br>");
  client.print("Click <a href=\"/set?value=%23000000\">here</a> to turn OFF the LED.");
  client.print("</p>");
  
  // Добавляем форму для ручного ввода цвета
  client.print("<form action=\"/set\" method=\"GET\">");
  client.print("Custom color (RRGBB): #<input type=\"text\" name=\"value\" value=\"");
  client.print(colorHex.substring(1)); // Без #
  client.print("\" maxlength=\"6\" pattern=\"[0-9A-Fa-f]{6}\">");
  client.print("<input type=\"submit\" value=\"Set Color\">");
  client.print("</form>");
  
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
    
    // Убираем возможные лишние символы
    color.trim();
    
    // Декодируем URL-encoding: %23 -> #
    color.replace("%23", "#");
    
    Serial.print("Decoded color: ");
    Serial.println(color);
    
    // Если цвет без #, добавляем его
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

// Функция для получения текущего цвета в формате HEX
String getColorHex() {
  char hexColor[8];
  sprintf(hexColor, "#%02X%02X%02X", currentRed, currentGreen, currentBlue);
  return String(hexColor);
}

// Функция для установки цвета по HEX-значению
void setColorFromHex(String hexColor) {
  if (hexColor.length() == 7 && hexColor[0] == '#') {
    String redStr = hexColor.substring(1, 3);
    String greenStr = hexColor.substring(3, 5);
    String blueStr = hexColor.substring(5, 7);

    currentRed = strtol(redStr.c_str(), NULL, 16);
    currentGreen = strtol(greenStr.c_str(), NULL, 16);
    currentBlue = strtol(blueStr.c_str(), NULL, 16);

    // Формируем цвет в 32-битном формате
    uint32_t color = (currentRed << 16) | (currentGreen << 8) | currentBlue;

    // Устанавливаем цвет на LED
    M5.dis.drawpix(0, color);
  }
}
