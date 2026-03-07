/*
  ESP8266 Receiver với hai buzzer
  - Nhận tín hiệu từ ESP32, bật hai buzzer cùng lúc trong 200ms
  - Nếu nhận liên tiếp, thời gian kêu được reset để kêu liên tục
  - Thêm LED báo kết nối (LED on-board)
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <shared.h>

// Chân kết nối hai buzzer
const int BUZZER1_PIN = D1;  // GPIO5
const int BUZZER2_PIN = D2;  // GPIO4
const int LED_PIN = LED_BUILTIN; // LED on-board (GPIO2 trên hầu hết ESP8266, active LOW)

// Biến điều khiển buzzer
bool buzzerActive = false;
unsigned long buzzerOffTime = 0;
const unsigned long BUZZER_DURATION = 200;

// Heartbeat variables
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_TIMEOUT = 3000; // 3 giây không nhận heartbeat thì coi như mất kết nối
bool isConnected = false; // Trạng thái kết nối

// Dữ liệu nhận được
message_t receivedMessage;

// Đếm số gói tin nhận được
uint32_t packetCount = 0;

void updateLED() {
  if (isConnected) {
    digitalWrite(LED_PIN, LOW); // Bật LED (active LOW)
  } else {
    digitalWrite(LED_PIN, HIGH); // Tắt LED
  }
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  packetCount++;
  
  if (len == sizeof(message_t)) {
    memcpy(&receivedMessage, incomingData, sizeof(message_t));

    // Cập nhật heartbeat - bất kỳ gói tin nào nhận được cũng chứng tỏ sender còn trong vùng phủ sóng
    lastHeartbeatTime = millis();
    if (!isConnected) {
      isConnected = true;
      updateLED();
      Serial.println(">>> SENDER KẾT NỐI (LED ON) <<<");
    }

    // In địa chỉ MAC của sender
    Serial.print("[");
    Serial.print(packetCount);
    Serial.print("] Nhận từ ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", mac[i]);
      if (i < 5) Serial.print(":");
    }
    
    // In chi tiết dữ liệu
    Serial.printf(" | counter=%u | button=%u | len=%d\n", 
                  receivedMessage.counter, 
                  receivedMessage.button_pressed,
                  len);

    // Xử lý tín hiệu (chỉ khi button_pressed == 1)
    if (receivedMessage.button_pressed == 1) {
      Serial.println("   >>> KÍCH HOẠT BUZZER! <<<");
      
      digitalWrite(BUZZER1_PIN, HIGH);
      digitalWrite(BUZZER2_PIN, HIGH);
      buzzerActive = true;
      buzzerOffTime = millis() + BUZZER_DURATION;
    }
  } else {
    Serial.printf("Lỗi: kích thước dữ liệu %d (mong đợi %d)\n", 
                  len, sizeof(message_t));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Đợi Serial ổn định
  
  Serial.println("\n=== ESP8266 RECEIVER ===");
  
  pinMode(BUZZER1_PIN, OUTPUT);
  pinMode(BUZZER2_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(BUZZER1_PIN, LOW);
  digitalWrite(BUZZER2_PIN, LOW);
  digitalWrite(LED_PIN, HIGH); // Tắt LED ban đầu
  
  // Test LED khi khởi động
  Serial.println("Test LED...");
  digitalWrite(LED_PIN, LOW);
  delay(500);
  digitalWrite(LED_PIN, HIGH);
  
  // In địa chỉ MAC của ESP8266
  WiFi.mode(WIFI_STA);
  Serial.print("Địa chỉ MAC ESP8266: ");
  Serial.println(WiFi.macAddress());
  
  // Khởi tạo ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("LỖI khởi tạo ESP-NOW");
    return;
  }
  
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("ESP8266 Receiver sẵn sàng!");
  Serial.println("Đợi dữ liệu từ ESP32...\n");
  
  lastHeartbeatTime = millis(); // Khởi tạo thời gian
}

void loop() {
  // Tắt buzzer khi hết thời gian
  if (buzzerActive && millis() >= buzzerOffTime) {
    digitalWrite(BUZZER1_PIN, LOW);
    digitalWrite(BUZZER2_PIN, LOW);
    buzzerActive = false;
    Serial.println("Tắt buzzer");
  }
  
  // Kiểm tra heartbeat timeout
  if (isConnected && (millis() - lastHeartbeatTime > HEARTBEAT_TIMEOUT)) {
    isConnected = false;
    updateLED();
    Serial.println(">>> MẤT KẾT NỐI (LED OFF) <<<");
  }
  
  // In trạng thái mỗi 5 giây để biết hệ thống còn sống
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.printf("Đang chạy... Đã nhận %d gói tin | Kết nối: %s\n", 
                  packetCount, 
                  isConnected ? "CÓ" : "KHÔNG");
  }
}
