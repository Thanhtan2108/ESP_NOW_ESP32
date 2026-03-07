#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <shared.h>

uint8_t receiverMac[] = {0x48, 0x55, 0x19, 0xDB, 0x2E, 0x91};

const int BUTTON_PIN = 18;
const int BUZZER_PIN = 19;
const int LED_PIN = LED_BUILTIN; // LED on-board ESP32 (thường là GPIO2)

// Debounce variables
int lastButtonState = HIGH;
int stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Buzzer control
bool buzzerActive = false;
unsigned long buzzerOffTime = 0;
const unsigned long BUZZER_DURATION = 200;

// Message
message_t myMessage;
uint32_t sendCounter = 0;

// Heartbeat variables
unsigned long lastHeartbeatTime = 0;
const unsigned long HEARTBEAT_INTERVAL = 1000; // 1 giây gửi heartbeat 1 lần

// Connection status variables (cho sender)
bool isReceiverConnected = false;
unsigned long lastAckTime = 0;
const unsigned long CONNECTION_TIMEOUT = 5000; // 5 giây không nhận được ACK thì coi như mất kết nối

void updateLED() {
  if (isReceiverConnected) {
    digitalWrite(LED_PIN, HIGH); // Bật LED (active HIGH trên hầu hết ESP32)
  } else {
    digitalWrite(LED_PIN, LOW); // Tắt LED
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Gửi: ");
  
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Thành công");
    
    // Cập nhật trạng thái kết nối khi gửi thành công
    if (!isReceiverConnected) {
      isReceiverConnected = true;
      updateLED();
      Serial.println(">>> RECEIVER KẾT NỐI (LED ON) <<<");
    }
    lastAckTime = millis(); // Ghi nhận thời gian gửi thành công cuối cùng
  } else {
    Serial.println("Thất bại");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== ESP32 SENDER ===");
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, HIGH); // Tắt LED ban đầu
  
  stableButtonState = digitalRead(BUTTON_PIN);
  lastButtonState = stableButtonState;

  // Test buzzer khi khởi động
  Serial.println("Test buzzer...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);

  // Test LED khi khởi động
  Serial.println("Test LED...");
  digitalWrite(LED_PIN, LOW);
  delay(500);
  digitalWrite(LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Lỗi thêm peer");
    return;
  }
  
  Serial.println("ESP32 Sender sẵn sàng!");
  Serial.print("Địa chỉ MAC ESP32: ");
  Serial.println(WiFi.macAddress());
  
  lastAckTime = millis(); // Khởi tạo thời gian
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  
  // Debounce
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != stableButtonState) {
      stableButtonState = reading;
      
      // Phát hiện NHẤN nút (cạnh xuống)
      if (stableButtonState == LOW) {
        Serial.println("\n>>> NHẤN NÚT <<<");

        // Bật buzzer local
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerActive = true;
        buzzerOffTime = millis() + BUZZER_DURATION;

        // *** QUAN TRỌNG: Gán dữ liệu trước khi gửi ***
        myMessage.button_pressed = 1;           // Đánh dấu là nhấn nút
        myMessage.counter = sendCounter++;       // Tăng bộ đếm mỗi lần nhấn
        
        Serial.printf("Gửi gói tin #%d\n", myMessage.counter);

        // Gửi qua ESP-NOW
        esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &myMessage, sizeof(myMessage));
        if (result == ESP_OK) {
          Serial.println("Đã gửi tín hiệu thành công");
        } else {
          Serial.println("Lỗi gửi");
        }
      }
      // Phát hiện NHẢ nút (cạnh lên)
      else {
        Serial.println(">>> NHẢ NÚT <<<");
      }
    }
  }
  
  lastButtonState = reading;

  // Tắt buzzer sau thời gian
  if (buzzerActive && millis() >= buzzerOffTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    Serial.println("Tắt buzzer local");
  }

  // Gửi Heartbeat định kỳ mỗi 1 giây
  if (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    lastHeartbeatTime = millis();
    
    // Tạo bản sao của message để không ảnh hưởng đến dữ liệu gốc
    message_t heartbeatMsg;
    heartbeatMsg.button_pressed = 0;  // 0 = không phải nhấn nút
    heartbeatMsg.counter = sendCounter; // Không tăng counter
    
    // Gửi heartbeat
    esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &heartbeatMsg, sizeof(heartbeatMsg));
    
    // Có thể bỏ comment dòng dưới để debug heartbeat nếu muốn
    if (result == ESP_OK) {
      Serial.println("Gửi heartbeat");
    }
  }

  // Kiểm tra timeout kết nối (dựa vào việc gửi thành công)
  if (isReceiverConnected && (millis() - lastAckTime > CONNECTION_TIMEOUT)) {
    isReceiverConnected = false;
    updateLED();
    Serial.println(">>> MẤT KẾT NỐI VỚI RECEIVER (LED OFF) <<<");
  }
}
