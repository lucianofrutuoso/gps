#include <ESP8266WiFi.h>
#include <TinyGPS++.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

TinyGPSPlus gps;
SoftwareSerial gpsSerial(4, 5); // RX=D4, TX=D5

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);
  
  WiFi.begin("SUA_REDE", "SENHA");
  
  // Quando tiver GPS fix, envia para nuvem
}

void loop() {
  while (gpsSerial.available()) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        sendToCloud();
      }
    }
  }
}

void sendToCloud() {
  HTTPClient http;
  http.begin("http://seuservidor.com/api/gps");
  http.addHeader("Content-Type", "application/json");
  
  String json = "{";
  json += "\"lat\":" + String(gps.location.lat(), 6);
  json += ",\"lon\":" + String(gps.location.lng(), 6);
  json += ",\"speed\":" + String(gps.speed.kmph());
  json += "}";
  
  http.POST(json);
  http.end();
}