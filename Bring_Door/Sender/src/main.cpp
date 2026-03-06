#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --------------------- CẤU HÌNH ---------------------
const int BUTTON_PIN = 4;
const int STATUS_LED = 2;

uint8_t receiverAddress[] = {0x3C, 0x8A, 0x1F, 0x5C, 0xFB, 0x84};

// --------------------- CẤU TRÚC DỮ LIỆU ---------------------
typedef struct struct_message {
  bool buttonPressed;   // true = nhấn nút, false = heartbeat (giữ kết nối)
} struct_message;

struct_message myData;

// --------------------- BIẾN DEBOUNCE ---------------------
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// --------------------- HEARTBEAT & KẾT NỐI ---------------------
unsigned long lastSendTime = 0;
const unsigned long HEARTBEAT_INTERVAL = 2000;  // gửi heartbeat mỗi 2 giây
unsigned long lastSuccessTime = 0;
const unsigned long CONNECTION_TIMEOUT = 8000;  // 8 giây không gửi OK → mất kết nối

void updateLedStatus() {
  if (millis() - lastSuccessTime < CONNECTION_TIMEOUT) {
    digitalWrite(STATUS_LED, HIGH);  // Sáng liên tục khi kết nối OK
  } else {
    digitalWrite(STATUS_LED, LOW);   // Tắt khi mất kết nối
  }
}

// --------------------- CALLBACK ---------------------
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    lastSuccessTime = millis();
    Serial.println("Gửi OK → kết nối ổn định");
  } else {
    Serial.println("Gửi Fail");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);  // Tắt lúc đầu

  WiFi.mode(WIFI_STA);
  Serial.print("MAC sender: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    while (true);
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Không thêm peer");
    while (true);
  }

  Serial.println("ESP-NOW sẵn sàng");
}

void loop() {
  unsigned long now = millis();

  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = now;
  }

  bool sendNow = false;
  myData.buttonPressed = false;  // mặc định heartbeat

  if ((now - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      myData.buttonPressed = true;
      sendNow = true;
      Serial.println("Nhấn nút → gửi ngay");
    }
  }

  lastButtonState = reading;

  // Heartbeat định kỳ nếu không nhấn nút
  if (!sendNow && (now - lastSendTime >= HEARTBEAT_INTERVAL)) {
    sendNow = true;
    lastSendTime = now;
    Serial.println("Gửi heartbeat");
  }

  if (sendNow) {
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&myData, sizeof(myData));
    if (result != ESP_OK) {
      Serial.println("Lỗi gửi");
    }
  }

  updateLedStatus();

  delay(20);  // loop nhanh để phản hồi tốt
}
