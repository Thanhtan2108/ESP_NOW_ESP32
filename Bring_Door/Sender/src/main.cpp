/*
 * ============================================================
 *  CHUÔNG CỬA KHÔNG DÂY - NODE PHÁT (NGOÀI CỬA)
 *  Phần cứng : ESP32 DevKit V1
 *  Giao thức : ESP-NOW
 *  Tác giả   : Wireless Doorbell Project
 * ============================================================
 *
 *  Sơ đồ chân:
 *    GPIO 5  → Buzzer ngoài cửa (active buzzer hoặc passive qua transistor)
 *    GPIO 18 → Nút nhấn (INPUT_PULLUP, nhấn → LOW)
 *    GPIO 2  → LED trạng thái (built-in hoặc LED ngoài)
 *
 *  Cấp nguồn: Pin 18650 qua mạch ổn áp 3.3 V
 * ============================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_sleep.h>

// ─── CẤU HÌNH PHẦN CỨNG ─────────────────────────────────────
#define PIN_BUTTON      18      // Nút nhấn (INPUT_PULLUP)
#define PIN_BUZZER      5       // Buzzer ngoài cửa
#define PIN_LED         2       // LED trạng thái

// ─── CẤU HÌNH THỜI GIAN (ms) ────────────────────────────────
#define DEBOUNCE_MS     30      // Lọc nhiễu nút nhấn
#define COOLDOWN_MS     1500    // Chặn spam: bỏ qua nhấn trong 1.5 s
#define BEEP_ON_MS      200     // Buzzer kêu 200 ms mỗi tiếng
#define BEEP_OFF_MS     100     // Nghỉ giữa 2 tiếng
#define BEEP_COUNT      2       // Số tiếng beep phản hồi
#define RESEND_COUNT    3       // Gửi lại tối đa 3 lần nếu thất bại
#define RESEND_DELAY_MS 50      // Khoảng cách giữa các lần gửi lại
#define LED_BLINK_MS    80      // Thời gian nhấp nháy LED

// ─── ĐỊA CHỈ MAC CỦA ESP8266 TRONG NHÀ ─────────────────────
//  Thay bằng MAC thực của ESP8266 (in ra từ sketch GetMAC)
uint8_t receiverMAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };

// ─── CẤU TRÚC GÓI TIN ESP-NOW ───────────────────────────────
typedef struct {
  char     event[16];    // "DOORBELL"
  uint8_t  deviceID;     // ID thiết bị (hỗ trợ nhiều nút)
  uint32_t timestamp;    // millis() tại thời điểm nhấn
  uint8_t  seqNum;       // Số thứ tự gói tin
} DoorbellPacket;

// ─── BIẾN TRẠNG THÁI ────────────────────────────────────────
volatile bool     buttonPressed  = false;
unsigned long     lastPressTime  = 0;
unsigned long     lastCooldown   = 0;
uint8_t           seqCounter     = 0;
bool              sendSuccess    = false;

// ─── CALLBACK GỬI ESP-NOW ────────────────────────────────────
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    sendSuccess = true;
    digitalWrite(PIN_LED, HIGH);
    delay(LED_BLINK_MS);
    digitalWrite(PIN_LED, LOW);
  }
}

// ─── NGẮT NÚT NHẤN (ISR) ─────────────────────────────────────
void IRAM_ATTR buttonISR() {
  buttonPressed = true;
}

// ─── PHÁT TIẾNG BEEP ─────────────────────────────────────────
void playBeep(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(BEEP_ON_MS);
    digitalWrite(PIN_BUZZER, LOW);
    if (i < count - 1) delay(BEEP_OFF_MS);
  }
}

// ─── GỬI TÍN HIỆU ESP-NOW (có thử lại) ──────────────────────
void sendDoorbellSignal() {
  DoorbellPacket pkt;
  strcpy(pkt.event, "DOORBELL");
  pkt.deviceID  = 1;
  pkt.timestamp = millis();
  pkt.seqNum    = ++seqCounter;

  sendSuccess = false;

  for (int attempt = 0; attempt < RESEND_COUNT; attempt++) {
    esp_err_t result = esp_now_send(receiverMAC,
                                    (uint8_t *)&pkt,
                                    sizeof(pkt));
    if (result == ESP_OK) {
      delay(20); // chờ callback
      if (sendSuccess) break;
    }
    delay(RESEND_DELAY_MS);
  }
}

// ─── SETUP ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n[TX] Khởi động Node Phát Chuông Cửa");

  // Cấu hình GPIO
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED,    OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED,    LOW);

  // Ngắt nút nhấn
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON),
                  buttonISR, FALLING);

  // Khởi tạo WiFi ở chế độ STA (bắt buộc cho ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[TX] Lỗi khởi tạo ESP-NOW!");
    while (true) { delay(500); }
  }
  esp_now_register_send_cb(onDataSent);

  // Đăng ký peer (ESP8266 trong nhà)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[TX] Không thể thêm peer!");
    while (true) { delay(500); }
  }

  Serial.println("[TX] Sẵn sàng. Chờ nhấn nút...");
  // Nhấp nháy LED báo sẵn sàng
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED, HIGH); delay(100);
    digitalWrite(PIN_LED, LOW);  delay(100);
  }
}

// ─── LOOP ─────────────────────────────────────────────────────
void loop() {
  if (!buttonPressed) return;
  buttonPressed = false;

  unsigned long now = millis();

  // ── Debounce ──
  if (now - lastPressTime < DEBOUNCE_MS) return;
  lastPressTime = now;

  // Kiểm tra nút thực sự đang được nhấn (loại false positive từ ISR)
  if (digitalRead(PIN_BUTTON) != LOW) return;

  // ── Cooldown chống spam ──
  if (now - lastCooldown < COOLDOWN_MS) {
    Serial.println("[TX] Bỏ qua (cooldown)");
    return;
  }
  lastCooldown = now;

  Serial.printf("[TX] Nhấn chuông! Seq=%d, T=%lu ms\n",
                seqCounter + 1, now);

  // ── 1. Phát beep phản hồi ngay cho khách ──
  playBeep(BEEP_COUNT);

  // ── 2. Gửi tín hiệu không dây ──
  sendDoorbellSignal();

  Serial.printf("[TX] Gửi %s. Tổng thời gian: %lu ms\n",
                sendSuccess ? "thành công" : "thất bại (đã thử lại)",
                millis() - now);
}
