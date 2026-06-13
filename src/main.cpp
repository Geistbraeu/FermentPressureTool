#include <Arduino.h>
#include <SoftwareSerial.h>
#include "WiFiEsp.h"

// Настройки Wi-Fi
const char* ssid = "Ghost";
const char* pass = "ghostdemonde";

// Настройки Облаков
const char* tsServer = "api.thingspeak.com";
const String tsApiKey = "DEXTOPQCD39G16GW";

const char* bfServer = "log.brewfather.net";
const String bfStreamId = "b8wwXJ3xdW3B8h"; 
const String deviceName = "Pressure_Sensor"; // Имя вашего датчика в Brewfather

// Таймеры (миллисекунды)
unsigned long lastThingSpeakTime = 0;
const unsigned long tsInterval = 15000; // 15 секунд для ThingSpeak

unsigned long lastBrewfatherTime = 0;
const unsigned long bfInterval = 900000; // 15 минут (900 000 мс) для Brewfather

// Железо
SoftwareSerial SerialESP(4, 5); 
int status = WL_IDLE_STATUS;     
const int sensorPin = A0;        
WiFiEspClient client;

// Прототипы функций
void setupWiFi();
void sendDataToThingSpeak(float voltage, float pressure);
void sendDataToBrewfather(float voltage, float pressure);

void setup() {
  Serial.begin(9600);   
  while (!Serial); 

  setupWiFi();
  Serial.println("\n--- Система готова к работе ---");
  
  // Принудительный первый запуск отправки при старте
  lastThingSpeakTime = millis() - tsInterval;
  lastBrewfatherTime = millis() - bfInterval;
}

void loop() {
  unsigned long currentMillis = millis();

  // Читаем датчик давления
  int rawValue = analogRead(sensorPin);
  float voltage = (rawValue * 4.9) / 1023.0;
  float pressurePSI = 0.0;
  
  if (voltage > 0.44) {
    pressurePSI = (voltage - 0.44) * 100.0 / (4.5 - 0.44);
  }

  // ТАЙМЕР 1: Отправка в ThingSpeak (Раз в 15 секунд)
  if (currentMillis - lastThingSpeakTime >= tsInterval) {
    lastThingSpeakTime = currentMillis;
    
    Serial.print("\nЛокально -> V: "); Serial.print(voltage);
    Serial.print("V | P: "); Serial.print(pressurePSI); Serial.println(" PSI");
    
    sendDataToThingSpeak(voltage, pressurePSI);
  }

  // ТАЙМЕР 2: Отправка в Brewfather (Раз в 15 минут!)
  if (currentMillis - lastBrewfatherTime >= bfInterval) {
    lastBrewfatherTime = currentMillis;
    sendDataToBrewfather(voltage, pressurePSI);
  }
}

void setupWiFi() {
  SerialESP.begin(9600); 
  WiFi.init(&SerialESP);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Ошибка: Wi-Fi шилд не найден!");
    while (true); 
  }

  while (status != WL_CONNECTED) {
    Serial.print("Подключение к Wi-Fi: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }
  Serial.println("[Wi-Fi] Подключено успешно!");
}

void sendDataToThingSpeak(float voltage, float pressure) {
  client.stop();
  if (client.connect(tsServer, 80)) {
    String url = "/update?api_key=" + tsApiKey + 
                 "&field1=" + String(voltage, 2) + 
                 "&field2=" + String(pressure, 2);

    // Сборка запроса в одну строку, чтобы отправить за один раз (одна команда AT+CIPSEND)
    String httpRequest;
    httpRequest.reserve(160);
    httpRequest += "GET " + url + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(tsServer) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";

    client.print(httpRequest);
    Serial.println("[ThingSpeak] Данные ушли.");
  }
}

void sendDataToBrewfather(float voltage, float pressure) {
  client.stop();
  
  Serial.println("\n[Brewfather] Подключение к серверу для POST...");
  
  if (client.connect(bfServer, 80)) {
    Serial.println("[Brewfather] Подключено! Формирование JSON пакета...");

    // 1. Формируем тело JSON (с резервированием памяти во избежание фрагментации кучи)
    String jsonBody;
    jsonBody.reserve(120);
    jsonBody += "{";
    jsonBody += "\"name\":\"" + deviceName + "\",";
    jsonBody += "\"pressure\":" + String(pressure, 2) + ",";
    jsonBody += "\"pressure_unit\":\"PSI\",";
    jsonBody += "\"battery\":" + String(voltage, 2);
    jsonBody += "}";
    
    int contentLength = jsonBody.length();

    // 2. Собираем весь HTTP-запрос в одну большую строку.
    // Это критически важно: WiFiEsp отправляет отдельный TCP-пакет (AT+CIPSEND) на каждый вызов client.print().
    // Сборка в одну строку позволяет отправить всё за один раз, предотвращая тайм-ауты из-за коллизий.
    String httpRequest;
    httpRequest.reserve(280);
    httpRequest += "POST /stream?id=" + bfStreamId + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(bfServer) + "\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += "Content-Length: " + String(contentLength) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";
    httpRequest += jsonBody + "\r\n";

    // 3. Отправляем запрос одним вызовом
    client.print(httpRequest);

    // Ожидаем завершения физической передачи данных модулем ESP8266
    delay(500); 
    client.flush();

    Serial.println("[Brewfather] JSON POST запрос успешно отправлен.");
  } else {
    Serial.println("[Brewfather] Ошибка подключения по порту 80.");
  }
}