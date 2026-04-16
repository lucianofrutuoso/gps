// ============================================================
//  GPS TRACKER COM ESP12E + NEO-6MV2 + OLED 128x64
//  Mostra dados no display OLED e em servidor web
// ============================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== CONFIGURAÇÃO DO DISPLAY OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset via software
#define SCREEN_ADDRESS 0x3C  // Endereço I2C comum (0x3C ou 0x3D)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== CONFIGURAÇÃO DO WIFI =====
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

// ===== CONFIGURAÇÃO DO GPS =====
// SoftwareSerial nos pinos GPIO4 (RX) e GPIO5 (TX)
SoftwareSerial gpsSerial(4, 5);  // RX=GPIO4, TX=GPIO5
TinyGPSPlus gps;

// ===== CONFIGURAÇÃO DO SERVIDOR =====
ESP8266WebServer server(80);

// ===== VARIÁVEIS GLOBAIS =====
float currentLat = 0;
float currentLon = 0;
float currentAlt = 0;
float currentSpeed = 0;
int currentSatellites = 0;
bool hasFix = false;
String lastUpdateTime = "";

// LED onboard (GPIO2)
const int ledPin = 2;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
  Serial.println();
  Serial.println("=== GPS TRACKER ESP12E + OLED ===");
  
  // Inicializa o display OLED
  if (!initOLED()) {
    Serial.println("Falha no OLED! Continuando sem display...");
  }
  
  // Conecta ao WiFi
  connectToWiFi();
  
  // Configura rotas do servidor
  setupServerRoutes();
  
  // Inicia servidor
  server.begin();
  Serial.println("Servidor HTTP iniciado!");
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());
  
  // Mostra tela inicial no OLED
  showSplashScreen();
}

// ===== INICIALIZA OLED =====
bool initOLED() {
  Wire.begin(4, 5);  // SDA=GPIO4, SCL=GPIO5
  Wire.setClock(400000);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED não encontrado!");
    return false;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  Serial.println("OLED inicializado!");
  return true;
}

// ===== TELA DE SPLASH =====
void showSplashScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println("GPS");
  display.setTextSize(1);
  display.setCursor(10, 35);
  display.println("ESP12E + OLED");
  display.setCursor(10, 50);
  display.println("Iniciando...");
  display.display();
  delay(2000);
}

// ===== CONECTA AO WIFI =====
void connectToWiFi() {
  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Mostra status no OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Conectando WiFi...");
    display.print("SSID: ");
    display.println(ssid);
    display.print(".");
    for (int i = 0; i < attempts / 5; i++) display.print(".");
    display.display();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Falha na conexão WiFi!");
  }
}

// ===== LOOP PRINCIPAL =====
void loop() {
  // Lê dados do GPS
  readGPSData();
  
  // Processa requisições do servidor
  server.handleClient();
  
  // Atualiza display OLED
  updateOLEDDisplay();
  
  // Pisca LED conforme status
  updateStatusLED();
  
  delay(50);
}

// ===== LEITURA DOS DADOS GPS =====
void readGPSData() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      updateGPSVariables();
    }
  }
  
  // Se não recebeu dados do GPS
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    static unsigned long lastWarn = 0;
    if (millis() - lastWarn > 5000) {
      Serial.println("⚠️ Nenhum dado do GPS. Verifique as conexões!");
      lastWarn = millis();
    }
  }
}

// ===== ATUALIZA VARIÁVEIS DO GPS =====
void updateGPSVariables() {
  hasFix = gps.location.isValid();
  
  if (hasFix) {
    currentLat = gps.location.lat();
    currentLon = gps.location.lng();
    
    if (gps.altitude.isValid())
      currentAlt = gps.altitude.meters();
    else
      currentAlt = 0;
    
    if (gps.speed.isValid())
      currentSpeed = gps.speed.kmph();
    else
      currentSpeed = 0;
    
    if (gps.satellites.isValid())
      currentSatellites = gps.satellites.value();
    else
      currentSatellites = 0;
    
    // Formata a hora
    if (gps.time.isValid()) {
      char timeStr[10];
      sprintf(timeStr, "%02d:%02d:%02d", 
              gps.time.hour(), gps.time.minute(), gps.time.second());
      lastUpdateTime = String(timeStr);
    }
    
    // Log no Serial a cada 5 segundos
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
      Serial.print("📍 ");
      Serial.print(currentLat, 6);
      Serial.print(", ");
      Serial.print(currentLon, 6);
      Serial.print(" | Sat: ");
      Serial.print(currentSatellites);
      Serial.print(" | Vel: ");
      Serial.print(currentSpeed);
      Serial.println(" km/h");
      lastLog = millis();
    }
  }
}

// ===== ATUALIZA DISPLAY OLED =====
void updateOLEDDisplay() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 200) return; // Atualiza a cada 200ms
  
  lastUpdate = millis();
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (hasFix) {
    // ===== MODO COM SINAL GPS =====
    
    // Linha 1: Status e Satélites
    display.setCursor(0, 0);
    display.print("GPS: FIX");
    display.setCursor(70, 0);
    display.print("SAT:");
    display.print(currentSatellites);
    
    // Linha 2: Hora
    display.setCursor(0, 10);
    display.print("Hora:");
    display.print(lastUpdateTime);
    
    // Linha 3: Latitude
    display.setCursor(0, 22);
    display.print("LAT:");
    display.print(currentLat, 4);
    
    // Linha 4: Longitude
    display.setCursor(0, 34);
    display.print("LON:");
    display.print(currentLon, 4);
    
    // Linha 5: Velocidade
    display.setCursor(0, 46);
    display.print("VEL:");
    display.print(currentSpeed, 1);
    display.print(" km/h");
    
    // Linha 6: Altitude
    display.setCursor(0, 57);
    display.print("ALT:");
    display.print(currentAlt, 0);
    display.print("m");
    
    // Indicador de WiFi (canto superior direito)
    if (WiFi.status() == WL_CONNECTED) {
      display.fillRect(118, 0, 10, 6, SSD1306_WHITE);
      display.fillRect(120, 2, 6, 2, SSD1306_BLACK);
    }
    
  } else {
    // ===== MODO SEM SINAL GPS =====
    
    display.setTextSize(2);
    display.setCursor(15, 10);
    display.println("GPS");
    display.setTextSize(1);
    display.setCursor(20, 30);
    display.println("PROCURANDO");
    display.setCursor(10, 42);
    display.println("SINAL...");
    display.setCursor(25, 54);
    display.print("SAT: ");
    if (gps.satellites.isValid())
      display.print(gps.satellites.value());
    else
      display.print("0");
  }
  
  display.display();
}

// ===== ATUALIZA LED DE STATUS =====
void updateStatusLED() {
  static unsigned long lastBlink = 0;
  
  if (hasFix) {
    // Pisca rápido (a cada 200ms) quando tem sinal
    if (millis() - lastBlink > 200) {
      digitalWrite(ledPin, !digitalRead(ledPin));
      lastBlink = millis();
    }
  } else {
    // Pisca lento (a cada 1000ms) quando procurando
    if (millis() - lastBlink > 1000) {
      digitalWrite(ledPin, !digitalRead(ledPin));
      lastBlink = millis();
    }
  }
}

// ===== CONFIGURA ROTAS DO SERVIDOR =====
void setupServerRoutes() {
  // Página principal
  server.on("/", HTTP_GET, []() {
    String html = generateMainPage();
    server.send(200, "text/html", html);
  });
  
  // API JSON
  server.on("/api/gps", HTTP_GET, []() {
    String json = generateGPSJson();
    server.send(200, "application/json", json);
  });
  
  // Página de status
  server.on("/status", HTTP_GET, []() {
    String status = generateStatusPage();
    server.send(200, "text/html", status);
  });
}

// ===== GERA PÁGINA PRINCIPAL =====
String generateMainPage() {
  String page = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GPS Tracker ESP12E + OLED</title>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #1a1a2e;
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: #16213e;
            border-radius: 15px;
            overflow: hidden;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        header {
            background: #0f3460;
            color: white;
            padding: 20px;
            text-align: center;
        }
        header h1 { font-size: 1.5rem; }
        header h1 i { color: #00ff88; margin-right: 10px; }
        .status-bar {
            display: flex;
            justify-content: space-between;
            background: #0f3460;
            padding: 10px 20px;
            border-top: 1px solid #1a1a2e;
            border-bottom: 1px solid #1a1a2e;
            flex-wrap: wrap;
            gap: 10px;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 10px;
            color: white;
            font-size: 0.9rem;
        }
        .gps-fix { color: #00ff88; }
        .gps-nofix { color: #ff4444; }
        #map {
            height: 400px;
            width: 100%;
        }
        .info-panel {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 10px;
            padding: 20px;
            background: #0f3460;
        }
        .info-card {
            background: #16213e;
            padding: 12px;
            border-radius: 10px;
            text-align: center;
            border-left: 3px solid #00ff88;
        }
        .info-card h3 {
            color: #aaa;
            font-size: 0.7rem;
            margin-bottom: 5px;
        }
        .info-card .value {
            color: white;
            font-size: 1rem;
            font-weight: bold;
            font-family: monospace;
        }
        .refresh {
            text-align: center;
            padding: 10px;
            background: #0f3460;
            color: #888;
            font-size: 0.7rem;
        }
        footer {
            text-align: center;
            padding: 15px;
            background: #0f3460;
            color: #888;
            font-size: 0.7rem;
            border-top: 1px solid #1a1a2e;
        }
        @media (max-width: 600px) {
            .info-panel { grid-template-columns: repeat(2, 1fr); }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1><i>📍</i> GPS Tracker ESP12E + OLED 128x64</h1>
            <p>Dados no display e no mapa online</p>
        </header>
        
        <div class="status-bar">
            <div class="status-item">
                <span>📡 WiFi:</span>
                <span id="wifi-status">Conectado</span>
            </div>
            <div class="status-item">
                <span>🛰️ GPS:</span>
                <span id="gps-status" class="gps-nofix">Aguardando...</span>
            </div>
            <div class="status-item">
                <span>🕐 Hora:</span>
                <span id="last-time">--:--:--</span>
            </div>
        </div>
        
        <div id="map"></div>
        
        <div class="info-panel">
            <div class="info-card">
                <h3>LATITUDE</h3>
                <div class="value" id="lat-value">---</div>
            </div>
            <div class="info-card">
                <h3>LONGITUDE</h3>
                <div class="value" id="lon-value">---</div>
            </div>
            <div class="info-card">
                <h3>ALTITUDE</h3>
                <div class="value" id="alt-value">--- m</div>
            </div>
            <div class="info-card">
                <h3>VELOCIDADE</h3>
                <div class="value" id="speed-value">--- km/h</div>
            </div>
            <div class="info-card">
                <h3>SATÉLITES</h3>
                <div class="value" id="sat-value">--</div>
            </div>
        </div>
        
        <div class="refresh">
            🔄 Atualização automática a cada 2 segundos
        </div>
        
        <footer>
            ESP12E + NEO-6MV2 + OLED 128x64 | Servidor Web Integrado
        </footer>
    </div>

    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script>
        let map = null;
        let marker = null;
        let pathLayer = null;
        let pathPoints = [];
        
        function initMap() {
            map = L.map('map').setView([-15.79, -47.88], 13);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors'
            }).addTo(map);
            pathLayer = L.polyline([], { color: '#00ff88', weight: 3 }).addTo(map);
        }
        
        async function fetchGPSData() {
            try {
                const response = await fetch('/api/gps');
                const data = await response.json();
                updateUI(data);
                if (data.valid && data.lat !== 0 && data.lon !== 0) {
                    updateMap(data);
                }
            } catch (error) {
                console.error('Erro:', error);
            }
        }
        
        function updateUI(data) {
            document.getElementById('last-time').textContent = data.time || '--:--:--';
            
            if (data.valid) {
                document.getElementById('gps-status').textContent = 'FIX OK';
                document.getElementById('gps-status').className = 'gps-fix';
                document.getElementById('lat-value').textContent = data.lat.toFixed(6);
                document.getElementById('lon-value').textContent = data.lon.toFixed(6);
                document.getElementById('alt-value').textContent = data.alt.toFixed(1) + ' m';
                document.getElementById('speed-value').textContent = data.speed.toFixed(1) + ' km/h';
                document.getElementById('sat-value').textContent = data.sat;
            } else {
                document.getElementById('gps-status').textContent = 'SEM SINAL';
                document.getElementById('gps-status').className = 'gps-nofix';
            }
        }
        
        function updateMap(data) {
            const latlng = [data.lat, data.lon];
            
            if (marker === null) {
                marker = L.marker(latlng).addTo(map);
                marker.bindPopup('<b>Posição Atual</b><br>' + data.lat.toFixed(6) + ', ' + data.lon.toFixed(6));
            } else {
                marker.setLatLng(latlng);
            }
            
            pathPoints.push(latlng);
            if (pathPoints.length > 100) pathPoints.shift();
            pathLayer.setLatLngs(pathPoints);
            map.setView(latlng, map.getZoom());
        }
        
        document.addEventListener('DOMContentLoaded', () => {
            initMap();
            fetchGPSData();
            setInterval(fetchGPSData, 2000);
        });
    </script>
</body>
</html>
)=====";
  
  return page;
}

// ===== GERA JSON COM DADOS DO GPS =====
String generateGPSJson() {
  String json = "{";
  json += "\"valid\":" + String(hasFix ? "true" : "false") + ",";
  json += "\"lat\":" + String(currentLat, 6) + ",";
  json += "\"lon\":" + String(currentLon, 6) + ",";
  json += "\"alt\":" + String(currentAlt, 1) + ",";
  json += "\"speed\":" + String(currentSpeed, 1) + ",";
  json += "\"sat\":" + String(currentSatellites) + ",";
  json += "\"time\":\"" + lastUpdateTime + "\"";
  json += "}";
  return json;
}

// ===== GERA PÁGINA DE STATUS =====
String generateStatusPage() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta charset='UTF-8'><title>Status GPS ESP12E</title>";
  page += "<meta http-equiv='refresh' content='5'>";
  page += "<style>";
  page += "body{font-family:Arial;background:#1a1a2e;color:white;padding:20px}";
  page += ".card{background:#16213e;padding:20px;border-radius:10px;margin-bottom:20px}";
  page += ".good{color:#00ff88} .bad{color:#ff4444}";
  page += "td{padding:8px}";
  page += "</style></head><body>";
  
  page += "<h1>📡 Status do Sistema GPS com OLED</h1>";
  
  page += "<div class='card'>";
  page += "<h2>🔧 Informações do ESP12E</h2>";
  page += "<table>";
  page += "<tr>\n<td>IP:</td><td><b>" + WiFi.localIP().toString() + "</b></td></tr>";
  page += "<tr>\n<td>WiFi:穷<td>" + String(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado") + "</td></tr>";
  page += "<tr>\n<td>RSSI:穷<td>" + String(WiFi.RSSI()) + " dBm穷</td>";
  page += "<tr>\n<td>MAC:穷<td>" + WiFi.macAddress() + "穷</tr>";
  page += "</table></div>";
  
  page += "<div class='card'>";
  page += "<h2>🛰️ Dados do GPS</h2>";
  page += "<table>";
  page += "<tr>\n<td>Status:穷<td>" + String(hasFix ? "<span class='good'>FIX OK</span>" : "<span class='bad'>SEM SINAL</span>") + "穷</tr>";
  
  if (hasFix) {
    page += "<tr>\n<td>Latitude:穷<td>" + String(currentLat, 6) + "穷</td>";
    page += "<tr>\n<td>Longitude:穷<td>" + String(currentLon, 6) + "穷</tr>";
    page += "<tr>\n<td>Altitude:穷<td>" + String(currentAlt, 1) + " m穷</tr>";
    page += "<tr>\n<td>Velocidade:穷<td>" + String(currentSpeed, 1) + " km/h穷</tr>";
    page += "<tr>\n<td>Satélites:穷<td>" + String(currentSatellites) + "穷</tr>";
    page += "<tr>\n<td>Hora UTC:穷<td>" + lastUpdateTime + "穷</tr>";
  }
  page += "</table></div>";
  
  page += "<div class='card'>";
  page += "<h2>🔗 Links</h2>";
  page += "<p><a href='/' style='color:#00ff88'>📱 Mapa Principal</a></p>";
  page += "<p><a href='/api/gps' style='color:#00ff88'>📊 API JSON</a></p>";
  if (hasFix) {
    page += "<p><a href='https://www.google.com/maps?q=" + String(currentLat,6) + "," + String(currentLon,6) + "' target='_blank' style='color:#00ff88'>🗺️ Ver no Google Maps</a></p>";
  }
  page += "</div>";
  
  page += "</body></html>";
  return page;
}