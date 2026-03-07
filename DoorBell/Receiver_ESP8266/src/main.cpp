/*
  ESP8266 Receiver với hai buzzer
  - Nhận tín hiệu từ ESP32, bật hai buzzer cùng lúc trong 200ms
  - Nếu nhận liên tiếp, thời gian kêu được reset để kêu liên tục
  - Thêm LED báo kết nối (LED on-board)
  - Thêm Deep Sleep để tiết kiệm năng lượng
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

// Deep Sleep variables
const unsigned long DEEP_SLEEP_TIMEOUT = 30000; // 30 giây không hoạt động thì vào deep sleep
unsigned long lastActivityTime = 0;
int bootCount = 0; // Đếm số lần boot (ESP8266 không có RTC_DATA_ATTR như ESP32)

// Wake-up pins
const int WAKE_PIN = D3; // GPIO0 - có thể dùng để wake từ deep sleep

void updateLED() {
  if (isConnected) {
    digitalWrite(LED_PIN, LOW); // Bật LED (active LOW)
  } else {
    digitalWrite(LED_PIN, HIGH); // Tắt LED
  }
}

void goToDeepSleep() {
  Serial.println("\n>>> CHUẨN BỊ VÀO DEEP SLEEP... <<<");
  Serial.println("Tắt các thiết bị...");
  
  // Tắt buzzer nếu đang kêu
  digitalWrite(BUZZER1_PIN, LOW);
  digitalWrite(BUZZER2_PIN, LOW);
  
  // Tắt LED
  digitalWrite(LED_PIN, HIGH);
  
  // Đợi Serial gửi xong
  delay(100);
  
  Serial.println("Vào Deep Sleep trong 30 giây...");
  Serial.flush();
  
  // Cấu hình wake-up từ timer (30 giây)
  // ESP8266 deep sleep: thời gian tính bằng micro giây
  ESP.deepSleep(30 * 1000000); // 30 giây
  
  // Code sau đây không chạy vì ESP đã sleep
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  packetCount++;
  
  if (len == sizeof(message_t)) {
    memcpy(&receivedMessage, incomingData, sizeof(message_t));

    // Cập nhật heartbeat - bất kỳ gói tin nào nhận được cũng chứng tỏ sender còn trong vùng phủ sóng
    lastHeartbeatTime = millis();
    lastActivityTime = millis(); // Cập nhật thời gian hoạt động
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
  
  // Đếm số lần boot
  bootCount++;
  
  Serial.println("\n=== ESP8266 RECEIVER ===");
  Serial.print("Boot count: ");
  Serial.println(bootCount);
  
  // Kiểm tra lý do wake (ESP8266 không có hàm esp_sleep_get_wakeup_cause đơn giản)
  rst_info *resetInfo = ESP.getResetInfoPtr();
  Serial.print("Reset reason: ");
  Serial.println(resetInfo->reason);
  
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
  
  // Test buzzer khi khởi động
  Serial.println("Test buzzer...");
  digitalWrite(BUZZER1_PIN, HIGH);
  digitalWrite(BUZZER2_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER1_PIN, LOW);
  digitalWrite(BUZZER2_PIN, LOW);
  
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
  lastActivityTime = millis(); // Khởi tạo thời gian hoạt động
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
  
  // Kiểm tra điều kiện vào deep sleep
  // Chỉ vào deep sleep khi không có kết nối và không hoạt động
  if (!isConnected && (millis() - lastActivityTime > DEEP_SLEEP_TIMEOUT)) {
    goToDeepSleep();
  }
}
