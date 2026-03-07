#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "shared.h"

uint8_t receiverMac[] = {0x48, 0x55, 0x19, 0xDB, 0x2E, 0x91};

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

// Message
message_t myMessage;
uint32_t sendCounter = 0;

// callback function of ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Thành công" : "Thất bại");
}

void setup() {
  // initial monitor
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== ESP32 SENDER ===");
  
  // config pin mode and idle of pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Đọc trạng thái ban đầu
  stableButtonState = digitalRead(BUTTON_PIN);
  lastButtonState = stableButtonState;

    // turn on WiFi STA
  WiFi.mode(WIFI_STA);
  
  // inital ESSP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi ESP-NOW");
    return;
  }
  
  // register callback function in ESP-NOW system
  esp_now_register_send_cb(OnDataSent);
  
  // register peer (slave) in ESP-NOW system
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0; // channel wifi
  peerInfo.encrypt = false; // don't encoder
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Lỗi thêm peer");
    return;
  }
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

        // send data via ESP-NOW      
        esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &myMessage, sizeof(myMessage));
        if (result == ESP_OK) {
          Serial.println("Đã gửi tín hiệu");
        } else {
          Serial.println("Lỗi gửi");
        }
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
