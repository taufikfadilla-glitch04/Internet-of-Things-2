/*
 * Tugas 6.7: Sistem Telemetri & Kendali Closed-Loop Industri
 * ---------------------------------------------------------------
 * 1. Telemetri JSON tiap 4 detik + field "kondisi_suhu" (NORMAL / PANAS)
 * 2. Alert otomatis ke topik /alert saat suhu > 31.0 C (state latch)
 * 3. LED menyala otomatis saat bahaya, tanpa menunggu operator
 * 4. Last Will and Testament (QoS 1, retain) + pesan ONLINE saat terhubung
 * 5. Manual override LED lewat topik /kendali (hanya saat suhu aman)
 * 6. Tanpa delay() yang memblokir (semua berbasis millis())
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Jaringan Wi-Fi (Gunakan Hotspot Pribadi Anda)
const char* ssid = "XP";
const char* password = "1a2b3C4D";

// Konfigurasi Broker MQTT Publik
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

// IDENTITAS KLIEN (Wajib Unik!)
const char* clientId = "ESP8266_LabIoT_NIM-25782114_Safety";

// Pembagian Topik Mandiri
const char* topicTelemetri = "kampus/labiot/25782114/telemetri";
const char* topicKendali   = "kampus/labiot/25782114/kendali";
const char* topicAlert     = "kampus/labiot/25782114/alert";
const char* topicStatus    = "kampus/labiot/25782114/status";

// Pesan Last Will (dikirim broker jika NodeMCU putus mendadak) dan pesan ONLINE
const char* pesanLWT    = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";
const char* pesanOnline = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";

// Alokasi Pin Perangkat Keras
const byte dhtPin = 2; // Pin D4 (GPIO 2)
const byte ledPin = 5; // Pin D1 (GPIO 5)
#define DHTTYPE DHT22

// Parameter sistem
const float BATAS_SUHU = 31.0;                    // Ambang bahaya (derajat C)
const unsigned long intervalTelemetri = 4000;     // Kirim telemetri tiap 4 detik
const unsigned long intervalReconnect = 5000;     // Jeda antar percobaan konek MQTT

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// Variabel status
unsigned long lastTelemetryTime = 0;
unsigned long lastReconnectTime = 0;
bool bahayaAktif   = false;   // true selama suhu melewati batas
bool alertTerkirim = false;   // latch: alert sudah dikirim untuk kejadian ini
bool ledState      = false;   // status LED saat ini
bool wifiOk        = false;   // untuk mencatat perubahan status Wi-Fi

// Mengatur LED sekaligus mencatat statusnya
void setLed(bool on) {
  ledState = on;
  digitalWrite(ledPin, on ? HIGH : LOW);
}

// Callback: menangani perintah manual dari operator
void callback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("----------------------------------------");
  Serial.printf("[KONTROL DITERIMA] Topik: %s | Payload: %s\n", topic, incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  if (!doc["led"].isNull()) {
    int val = doc["led"];

    // Proteksi: saat bahaya, override manual ditolak agar LED tidak bisa dimatikan
    if (bahayaAktif) {
      Serial.println("--> [DITOLAK] Kondisi BAHAYA, LED dikunci menyala oleh sistem.");
    } else {
      setLed(val == 1);
      Serial.printf("--> [OVERRIDE] Status LED diubah: %s\n", ledState ? "MENYALA" : "PADAM");
    }
  }
  Serial.println("----------------------------------------");
}

// Memantau Wi-Fi tanpa memblokir (ESP8266 menyambung ulang sendiri)
void cekWiFi() {
  bool ok = (WiFi.status() == WL_CONNECTED);
  if (ok && !wifiOk) {
    Serial.print("[WIFI] Terhubung, IP: ");
    Serial.println(WiFi.localIP());
  } else if (!ok && wifiOk) {
    Serial.println("[WIFI] Terputus, mencoba menyambung ulang...");
  }
  wifiOk = ok;
}

// Menjaga koneksi MQTT secara non-blocking
void jagaKoneksiMQTT() {
  if (client.connected()) {
    client.loop(); // Memelihara sesi TCP dan memproses callback masuk
    return;
  }
  if (!wifiOk) return;

  unsigned long now = millis();
  if (now - lastReconnectTime < intervalReconnect) return;
  lastReconnectTime = now;

  Serial.print("[MQTT] Menghubungkan ke broker...");
  // connect(id, user, pass, willTopic, willQoS, willRetain, willMessage)
  if (client.connect(clientId, NULL, NULL, topicStatus, 1, true, pesanLWT)) {
    Serial.println(" Sukses Terhubung!");
    client.subscribe(topicKendali);
    client.publish(topicStatus, pesanOnline, true); // timpa pesan OFFLINE yang tersimpan
    Serial.println("[MQTT] Status ONLINE dipublikasikan.");
  } else {
    Serial.printf(" Gagal (rc=%d)\n", client.state());
  }
}

// Mengirim peringatan bahaya, mengembalikan true jika berhasil
bool kirimAlert(float suhu) {
  StaticJsonDocument<128> doc;
  doc["status"]     = "BAHAYA";
  doc["parameter"]  = "OVERHEAT";
  doc["nilai_suhu"] = serialized(String(suhu, 1));

  char buffer[128];
  serializeJson(doc, buffer);

  bool ok = client.publish(topicAlert, buffer);
  Serial.printf("[ALERT] %s -> %s\n", buffer, ok ? "terkirim" : "GAGAL");
  return ok;
}

// Satu siklus: baca sensor -> cek bahaya -> kirim telemetri
void prosesSiklus() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Peringatan: Gagal membaca data DHT!");
    return;
  }

  // ----- Edge safety engine (tetap jalan walau broker putus) -----
  if (t > BATAS_SUHU) {
    if (!bahayaAktif) {                 // transisi NORMAL -> BAHAYA
      bahayaAktif   = true;
      alertTerkirim = false;
      setLed(true);                     // LED menyala otomatis
      Serial.println("[SAFETY] SUHU KRITIS! LED dinyalakan otomatis.");
    }
    if (!alertTerkirim && client.connected()) {
      alertTerkirim = kirimAlert(t);    // kirim sekali per kejadian bahaya
    }
  } else if (bahayaAktif) {             // transisi BAHAYA -> NORMAL
    bahayaAktif   = false;
    alertTerkirim = false;              // reset latch untuk kejadian berikutnya
    setLed(false);
    Serial.println("[SAFETY] Suhu kembali aman. LED padam, override manual aktif.");
  }

  // ----- Telemetri rutin -----
  if (client.connected()) {
    StaticJsonDocument<256> docOut;
    docOut["node_id"]      = clientId;
    docOut["suhu"]         = serialized(String(t, 2));
    docOut["kelembapan"]   = serialized(String(h, 2));
    docOut["status_led"]   = ledState ? 1 : 0;
    docOut["kondisi_suhu"] = (t > BATAS_SUHU) ? "PANAS" : "NORMAL";

    char buffer[256];
    serializeJson(docOut, buffer);

    if (client.publish(topicTelemetri, buffer)) {
      Serial.printf("[TELEMETRI TERKIRIM] %s\n", buffer);
    } else {
      Serial.println("[ERROR] Gagal memublikasikan telemetri!");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  setLed(false); // Status awal LED padam
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); // Tanpa loop menunggu, dipantau di loop()

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(512);  // Ruang cukup untuk paket dan pesan LWT
  client.setKeepAlive(15);    // Pesan LWT muncul sekitar 20-25 detik setelah putus
}

void loop() {
  cekWiFi();
  jagaKoneksiMQTT();

  unsigned long now = millis();
  if (now - lastTelemetryTime >= intervalTelemetri) {
    lastTelemetryTime = now;
    prosesSiklus();
  }
}