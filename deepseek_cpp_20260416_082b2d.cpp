// ============================================================
//  GPS TRACKER COM ESP12E + NEO-6MV2
//  Mostra localização em servidor web com mapa
// ============================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// ===== CONFIGURAÇÃO DO WIFI =====
// Altere para os dados da sua rede
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

// ===== CONFIGURAÇÃO DO GPS =====
// Usando SoftwareSerial nos pinos do ESP12E
// RX do GPS → GPIO3 (pino RXD do ESP)
// TX do GPS → GPIO1 (pino TXD do ESP)
// Mas como o ESP já tem Serial padrão nesses pinos,
// vamos usar SoftwareSerial em outros pinos

// Para ESP12E, use GPIO4 e GPIO5 para SoftwareSerial
SoftwareSerial gpsSerial(4, 5);  // RX=GPIO4(D2), TX=GPIO5(D1)
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

// LED onboard do ESP12E (GPIO2)
const int ledPin = 2;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // LED apagado (lógica invertida)
  
  Serial.println();
  Serial.println("=== GPS TRACKER ESP12E ===");
  
  // Conecta ao WiFi
  connectToWiFi();
  
  // Configura rotas do servidor
  setupServerRoutes();
  
  // Inicia servidor
  server.begin();
  Serial.println("Servidor HTTP iniciado!");
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());
}

// ===== LOOP PRINCIPAL =====
void loop() {
  // Lê dados do GPS
  readGPSData();
  
  // Processa requisições do servidor
  server.handleClient();
  
  // Pisca o LED conforme status do GPS
  updateStatusLED();
  
  delay(10);
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
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Falha na conexão WiFi!");
    Serial.println("Verifique SSID e senha");
  }
}

// ===== LEITURA DOS DADOS GPS =====
void readGPSData() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      // Dados atualizados
      updateGPSVariables();
    }
  }
  
  // Se não recebeu dados do GPS
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println("⚠️  Nenhum dado do GPS. Verifique as conexões!");
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
      sprintf(timeStr, "%02d:%02d:%02d UTC", 
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
  
  // API JSON para dados em tempo real
  server.on("/api/gps", HTTP_GET, []() {
    String json = generateGPSJson();
    server.send(200, "application/json", json);
  });
  
  // Página de status simples
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
    <title>GPS Tracker ESP12E</title>
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
        header h1 { font-size: 1.8rem; }
        header h1 i { color: #00ff88; margin-right: 10px; }
        .status-bar {
            display: flex;
            justify-content: space-between;
            background: #0f3460;
            padding: 10px 20px;
            border-top: 1px solid #1a1a2e;
            border-bottom: 1px solid #1a1a2e;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 10px;
            color: white;
        }
        .gps-fix { color: #00ff88; }
        .gps-nofix { color: #ff4444; }
        #map {
            height: 450px;
            width: 100%;
        }
        .info-panel {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            padding: 20px;
            background: #0f3460;
        }
        .info-card {
            background: #16213e;
            padding: 15px;
            border-radius: 10px;
            text-align: center;
            border-left: 4px solid #00ff88;
        }
        .info-card h3 {
            color: #aaa;
            font-size: 0.8rem;
            margin-bottom: 8px;
        }
        .info-card .value {
            color: white;
            font-size: 1.3rem;
            font-weight: bold;
            font-family: monospace;
        }
        .refresh {
            text-align: center;
            padding: 10px;
            background: #0f3460;
            color: #888;
            font-size: 0.8rem;
        }
        footer {
            text-align: center;
            padding: 15px;
            background: #0f3460;
            color: #888;
            font-size: 0.8rem;
            border-top: 1px solid #1a1a2e;
        }
        @media (max-width: 600px) {
            .info-panel { grid-template-columns: repeat(2, 1fr); }
            .info-card .value { font-size: 1rem; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1><i>📍</i> GPS Tracker ESP12E + NEO-6M</h1>
            <p>Rastreamento em tempo real</p>
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
                <span>🕐 Última:</span>
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
            <div class="info-card">
                <h3>STATUS</h3>
                <div class="value" id="fix-value">---</div>
            </div>
        </div>
        
        <div class="refresh">
            🔄 Atualização automática a cada 3 segundos
        </div>
        
        <footer>
            ESP12E + NEO-6MV2 | Desenvolvido com ❤️
        </footer>
    </div>

    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script>
        let map = null;
        let marker = null;
        let pathLayer = null;
        let pathPoints = [];
        let lastLat = null;
        let lastLon = null;
        
        // Inicializa o mapa
        function initMap() {
            map = L.map('map').setView([-15.79, -47.88], 13);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors'
            }).addTo(map);
            pathLayer = L.polyline([], { color: '#00ff88', weight: 3 }).addTo(map);
        }
        
        // Busca dados da API
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
        
        // Atualiza interface
        function updateUI(data) {
            document.getElementById('last-time').textContent = data.time || '--:--:--';
            
            if (data.valid) {
                document.getElementById('gps-status').textContent = 'FIX OK';
                document.getElementById('gps-status').className = 'gps-fix';
                document.getElementById('fix-value').textContent = 'FIX';
                document.getElementById('fix-value').style.color = '#00ff88';
                
                document.getElementById('lat-value').textContent = data.lat.toFixed(6);
                document.getElementById('lon-value').textContent = data.lon.toFixed(6);
                document.getElementById('alt-value').textContent = data.alt.toFixed(1) + ' m';
                document.getElementById('speed-value').textContent = data.speed.toFixed(1) + ' km/h';
                document.getElementById('sat-value').textContent = data.sat;
            } else {
                document.getElementById('gps-status').textContent = 'SEM SINAL';
                document.getElementById('gps-status').className = 'gps-nofix';
                document.getElementById('fix-value').textContent = '---';
                document.getElementById('fix-value').style.color = '#ff4444';
            }
        }
        
        // Atualiza mapa
        function updateMap(data) {
            const latlng = [data.lat, data.lon];
            
            if (marker === null) {
                marker = L.marker(latlng).addTo(map);
                marker.bindPopup('<b>Posição Atual</b><br>' + 
                    data.lat.toFixed(6) + ', ' + data.lon.toFixed(6));
            } else {
                marker.setLatLng(latlng);
                marker.getPopup().setContent('<b>Posição Atual</b><br>' + 
                    data.lat.toFixed(6) + ', ' + data.lon.toFixed(6));
            }
            
            pathPoints.push(latlng);
            if (pathPoints.length > 100) pathPoints.shift();
            pathLayer.setLatLngs(pathPoints);
            
            map.setView(latlng, map.getZoom());
            lastLat = data.lat;
            lastLon = data.lon;
        }
        
        // Inicialização
        document.addEventListener('DOMContentLoaded', () => {
            initMap();
            fetchGPSData();
            setInterval(fetchGPSData, 3000);
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
  
  page += "<h1>📡 Status do Sistema GPS</h1>";
  
  page += "<div class='card'>";
  page += "<h2>🔧 Informações do ESP12E</h2>";
  page += "<table>";
  page += "<tr><td>IP:</td><td><b>" + WiFi.localIP().toString() + "</b></td></tr>";
  page += "<tr><td>WiFi:</td><td>" + String(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado") + "</td></tr>";
  page += "<tr><td>RSSI:</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  page += "<tr><td>MAC:</td><td>" + WiFi.macAddress() + "</td></tr>";
  page += "</table></div>";
  
  page += "<div class='card'>";
  page += "<h2>🛰️ Dados do GPS</h2>";
  page += "<table>";
  page += "<tr><td>Status:</td><td>" + String(hasFix ? "<span class='good'>FIX OK</span>" : "<span class='bad'>SEM SINAL</span>") + "</td></tr>";
  
  if (hasFix) {
    page += "<tr><td>Latitude:</td><td>" + String(currentLat, 6) + "</td></tr>";
    page += "<tr><td>Longitude:</td><td>" + String(currentLon, 6) + "</td></tr>";
    page += "<tr><td>Altitude:</td><td>" + String(currentAlt, 1) + " m</td></tr>";
    page += "<tr><td>Velocidade:</td><td>" + String(currentSpeed, 1) + " km/h</td></tr>";
    page += "<tr><td>Satélites:</td><td>" + String(currentSatellites) + "</td></tr>";
    page += "<tr><td>Hora UTC:</td><td>" + lastUpdateTime + "</td></tr>";
  }
  page += "</table></div>";
  
  page += "<div class='card'>";
  page += "<h2>🔗 Links</h2>";
  page += "<p><a href='/' style='color:#00ff88'>📱 Página Principal</a></p>";
  page += "<p><a href='/api/gps' style='color:#00ff88'>📊 API JSON</a></p>";
  if (hasFix) {
    page += "<p><a href='https://www.google.com/maps?q=" + String(currentLat,6) + "," + String(currentLon,6) + "' target='_blank' style='color:#00ff88'>🗺️ Ver no Google Maps</a></p>";
  }
  page += "</div>";
  
  page += "</body></html>";
  return page;
}