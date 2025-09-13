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
          // Отправляем HTTP ответ
          if (request.indexOf("GET / ") != -1 || request.indexOf("GET /get") != -1) {
            sendHTMLResponse(client);
          }
          else if (request.indexOf("GET /set?value=") != -1) {
            handleSetRequest(client, request);
          }
          break;
        }
      }
    }
    
    client.stop();
    Serial.println("Client disconnected");
  }
}

// void sendHTMLResponse(WiFiClient &client) {
//   client.println("HTTP/1.1 200 OK");
//   client.println("Content-Type: text/html");
//   client.println("Connection: close");
//   client.println();
  
//   client.println("<!DOCTYPE HTML>");
//   client.println("<html>");
//   client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
//   client.println("<title>M5Atom LED Control</title></head>");
//   client.println("<body>");
//   client.println("<h1>M5Atom LED Control</h1>");
  
//   String colorHex = getColorHex();
//   client.print("<p>Current LED color: ");
//   client.print(colorHex);
//   client.print("</p>");
  
//   // Исправленные ссылки - добавлены закрывающие кавычки и теги
//   client.print("<p>");
//   client.print("Click <a href=\"/set?value=#FF0000\">here</a> to turn ON RED the LED.<br>");
//   client.print("Click <a href=\"/set?value=#00FF00\">here</a> to turn on Green the LED.<br>");
//   client.print("Click <a href=\"/set?value=#0000FF\">here</a> to turn on Blue the LED.<br>");
//   client.print("Click <a href=\"/set?value=#000000\">here</a> to turn OFF the LED.");
//   client.print("</p>");
  
//   // Добавляем форму для ручного ввода цвета
//   client.print("<form action=\"/set\" method=\"GET\">");
//   client.print("Custom color (RRGGBB): <input type=\"text\" name=\"value\" value=\"#");
//   client.print(colorHex.substring(1)); // Без #
//   client.print("\">");
//   client.print("<input type=\"submit\" value=\"Set Color\">");
//   client.print("</form>");
  
//   client.println("</body>");
//   client.println("</html>");
// }
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
   client.print("Custom color (RRGGBB): <input type=\"text\" name=\"value\" value=\"#");
   client.print(colorHex.substring(1)); // Без #
   client.print("\">");
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
    
    if (color.length() == 7 && color[0] == '#') {
      setColorFromHex(color);
      
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      
      client.print("LED color set to: ");
      client.print(color);
      client.print("<br><a href=\"/\">Back to main page</a>");
    } else {
      client.println("HTTP/1.1 400 Bad Request");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.print("Invalid color format. Use #RRGGBB format.");
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