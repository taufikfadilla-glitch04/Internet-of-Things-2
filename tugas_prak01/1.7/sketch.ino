const int buttonPin = 25;
const int ledPin = 26;

// Variabel untuk melacak status LED saat ini (state tracking)
// false = LED mati (OFF), true = LED menyala (ON)
bool ledState = false;

// Variabel untuk menyimpan status tombol pada pembacaan sebelumnya,
// digunakan untuk mendeteksi transisi LOW -> HIGH (baru ditekan),
// bukan sekadar status tombol saat ini yang bisa saja masih ditahan.
int lastButtonState = LOW;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Pastikan LED mati saat sistem pertama kali dinyalakan
  digitalWrite(ledPin, ledState);
}

void loop() {
  // Baca status tombol saat ini
  int currentButtonState = digitalRead(buttonPin);

  // --- LOGIKA STATE TRACKING (TOGGLE) ---
  // Perubahan status hanya diproses ketika terjadi transisi dari
  // LOW (tidak ditekan) ke HIGH (baru ditekan). Dengan cara ini,
  // menahan tombol tetap ditekan tidak akan membuat LED
  // berubah status berulang kali (bukan momentary).
  if (currentButtonState == HIGH && lastButtonState == LOW) {

    // --- LOGIKA DEBOUNCE ---
    // Saat tombol fisik ditekan, kontak logamnya bisa "memantul"
    // beberapa kali dalam hitungan milidetik, menyebabkan
    // digitalRead() membaca beberapa transisi palsu.
    // delay(200) di sini memberi jeda singkat agar sinyal tombol
    // benar-benar stabil sebelum status LED diproses lebih lanjut,
    // sehingga mencegah flickering / toggle ganda yang tidak diinginkan.
    delay(200);

    // Balik (toggle) status LED: ON jadi OFF, OFF jadi ON
    ledState = !ledState;
    digitalWrite(ledPin, ledState);

    if (ledState) {
      Serial.println("Tombol ditekan! -> LED ON");
    } else {
      Serial.println("Tombol ditekan! -> LED OFF");
    }
  }

  // Simpan status tombol saat ini sebagai status sebelumnya
  // untuk perbandingan pada iterasi loop() berikutnya
  lastButtonState = currentButtonState;
}