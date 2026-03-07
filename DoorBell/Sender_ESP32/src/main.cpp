#include <Arduino.h>

const int BUTTON_PIN = 18;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println("Test nút nhấn - Nhấn nút để xem kết quả");
}

void loop() {
  static int lastState = HIGH;
  int currentState = digitalRead(BUTTON_PIN);
  
  if (currentState != lastState) {
    delay(50); // debounce đơn giản
    currentState = digitalRead(BUTTON_PIN);
    
    if (currentState != lastState) {
      lastState = currentState;
      
      if (currentState == LOW) {
        Serial.println("Nút NHẤN");
      } else {
        Serial.println("Nút THẢ");
      }
    }
  }
}
