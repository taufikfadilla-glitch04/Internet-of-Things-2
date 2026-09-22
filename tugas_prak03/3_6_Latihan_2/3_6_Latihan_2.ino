#include <ESP8266WiFi.h>  
#include <ESP8266WebServer.h>  
#include <DHT.h>

const char* ssid = "XP";  
const char* password = "1a2b3C4D";

ESP8266WebServer server(80);

const byte dhtPin = 2;         
const byte relayPin = 12;      
DHT dht(dhtPin, DHT22);

const char index_html[] PROGMEM = R"rawliteral(  
<!DOCTYPE html>  
<html>  
<head>  
  <meta name="viewport" content="width=device-width, initial-scale=1"> 
  <title>IoT Web Dashboard</title>  
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 40px; background-color: #f4f4f9; }  
    h1 { color: #333; }  
    button { padding: 15px 32px; font-size: 18px; border-radius: 8px; margin: 15px; cursor: pointer; color: white; border: none; font-weight: bold; }  
    .btn-on { background-color: #4CAF50; }  /* Warna Hijau */
    .btn-off { background-color: #f44336; } /* Warna Merah */
    .sensor-box { font-size: 20px; background: #ffffff; display: inline-block; padding: 20px 40px; border-radius: 12px; box-shadow: 0px 4px 10px rgba(0,0,0,0.1); }  
    .sensor-box p { margin: 10px 0; }  
  </style>
</head>  
<body>  
  <h1>ESP8266 Web Server (DHT22)</h1>  
  <div class="sensor-box">  
    <p>Suhu Saat Ini: <strong>%TEMPERATURE%</strong> &deg;C</p>  
    <p>Kelembapan: <strong>%HUMIDITY%</strong> %</p>  
  </div>  
  <h2>Kendali Relay / Lampu</h2>  
  <!-- Tempat Tombol Dinamis Bertukar -->
  %RELAY_BUTTON%  
</body>  
</html>  
)rawliteral";

void handleRoot() {  
  String html = FPSTR(index_html);
  
  float t = dht.readTemperature();  
  float h = dht.readHumidity(); 

  if (isnan(t)) {
    html.replace("%TEMPERATURE%", "--"); 
  } else {
    html.replace("%TEMPERATURE%", String(t, 1)); 
  }

  if (isnan(h)) {
    html.replace("%HUMIDITY%", "--"); 
  } else {
    html.replace("%HUMIDITY%", String(h, 1)); 
  }

  if (digitalRead(relayPin) == HIGH) {
    html.replace("%RELAY_BUTTON%", "<a href=\"/relay/off\"><button class=\"btn-off\">MATIKAN (OFF)</button></a>");
  } else {
    html.replace("%RELAY_BUTTON%", "<a href=\"/relay/on\"><button class=\"btn-on\">NYALAKAN (ON)</button></a>");
  }
  
  server.send(200, "text/html", html);  
}

void handleRelayOn() {  
  digitalWrite(relayPin, HIGH); 
  server.sendHeader("Location", "/");   
  server.send(303);  
}

void handleRelayOff() {  
  digitalWrite(relayPin, LOW);   
  server.sendHeader("Location", "/");   
  server.send(303);  
}

void setup() {  
  Serial.begin(115200);  
  pinMode(relayPin, OUTPUT);  
  digitalWrite(relayPin, LOW);

  dht.begin();  
    
  WiFi.mode(WIFI_STA);   
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }

  Serial.println("\n[SERVER READY] IP Address Server Anda:");  
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);  
  server.on("/relay/on", handleRelayOn);  
  server.on("/relay/off", handleRelayOff);  
  server.begin();  
}

void loop() {  
  server.handleClient();  
}
