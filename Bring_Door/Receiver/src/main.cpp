#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --------------------- CẤU HÌNH ---------------------
const int BUZZER_PIN = 13;
const int STATUS_LED = 2;

typedef struct struct_message {
  bool buttonPressed;
} struct_message;

struct_message myData;

// --------------------- KẾT NỐI ---------------------
unsigned long lastRecvTime = 0;
const unsigned long CONNECTION_TIMEOUT = 8000;  // 8 giây không nhận → mất kết nối

void updateLedStatus() {
  if (millis() - lastRecvTime < CONNECTION_TIMEOUT) {
    digitalWrite(STATUS_LED, HIGH);  // Sáng liên tục khi có kết nối
  } else {
    digitalWrite(STATUS_LED, LOW);   // Tắt khi mất
  }
}

// --------------------- BUZZER ---------------------
void beep(int duration = 80) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duration);
  digitalWrite(BUZZER_PIN, LOW);
  delay(40);
}

void beepBeep() {
  beep(80);
  beep(80);
}

// --------------------- CALLBACK ---------------------
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  lastRecvTime = millis();  // reset timer mỗi khi nhận gói

  Serial.print("Nhận từ: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.print(" | buttonPressed: ");
  Serial.println(myData.buttonPressed ? "true → BEEP BEEP" : "false (heartbeat)");

  if (myData.buttonPressed) {
    beepBeep();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== RECEIVER khởi động ===\n");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC receiver: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi ESP-NOW");
    while (true);
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Chờ sender...");
}

void loop() {
  updateLedStatus();
  delay(50);
}
