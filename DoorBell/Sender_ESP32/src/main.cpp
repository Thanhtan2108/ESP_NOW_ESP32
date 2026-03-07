#include <Arduino.h>

const int BUTTON_PIN = 18;
const int BUZZER_PIN = 19;

// Debounce variables
int lastButtonState = HIGH;      // Trạng thái trước đó (chưa debounce)
int stableButtonState = HIGH;    // Trạng thái đã ổn định sau debounce
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Buzzer control
bool buzzerActive = false;
unsigned long buzzerOffTime = 0;
const unsigned long BUZZER_DURATION = 200;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP32 SENDER ===");
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Đọc trạng thái ban đầu
  stableButtonState = digitalRead(BUTTON_PIN);
  lastButtonState = stableButtonState;
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  
  // Nếu trạng thái thay đổi, reset timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // Nếu trạng thái ổn định trong khoảng debounce
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Nếu trạng thái ổn định khác với trạng thái đã lưu
    if (reading != stableButtonState) {
      stableButtonState = reading;
      
      // Phát hiện cạnh xuống (NHẤN)
      if (stableButtonState == LOW) {
        Serial.println(">>> NHẤN NÚT (debounce) <<<");

        // Bật buzzer local
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerActive = true;
        buzzerOffTime = millis() + BUZZER_DURATION;
      }
      // Phát hiện cạnh lên (NHẢ) - nếu cần
      else {
        Serial.println(">>> NHẢ NÚT (debounce) <<<");
      }
    }
  }
  
  // Cập nhật trạng thái cuối cùng (chưa debounce)
  lastButtonState = reading;

  // Tắt buzzer
  if (buzzerActive && millis() >= buzzerOffTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}
