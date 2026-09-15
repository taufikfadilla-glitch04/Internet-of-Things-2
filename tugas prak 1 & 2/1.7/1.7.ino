const int buttonPin = 4;
const int ledPin = 5;

int ledState = LOW;
int lastButtonState = LOW;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, ledState);
}

void loop() {
  int buttonState = digitalRead(buttonPin);

  // Mendeteksi saat tombol baru ditekan
  if (buttonState == HIGH && lastButtonState == LOW) {
    ledState = !ledState;

    digitalWrite(ledPin, ledState);

    if (ledState == HIGH) {
      Serial.println("Tombol ditekan -> LED ON");
    } else {
      Serial.println("Tombol ditekan -> LED OFF");
    }

    delay(50); // debounce sederhana
  }

  lastButtonState = buttonState;
}