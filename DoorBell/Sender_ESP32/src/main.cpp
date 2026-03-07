#include <Arduino.h>

const int BUZZER_PIN = 19;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== BUZZER ===");
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);
  delay(500);  
}
