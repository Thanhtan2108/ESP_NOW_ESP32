// shared.h
#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>

// Cấu trúc dữ liệu gửi qua ESP-NOW
typedef struct {
  uint8_t button_pressed; // 1 khi nút được nhấn, 0 không dùng
  uint32_t counter;       // tăng dần mỗi lần nhấn (có thể dùng để debug)
} message_t;

#endif
