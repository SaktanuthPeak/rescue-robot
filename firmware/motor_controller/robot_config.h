#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

#include <Arduino.h>

// =====================================================
// ผังการต่อสายฉบับใช้งานจริง (Arduino Mega 2560)
// อ้างอิงตามรูปภาพ firmware/pinout/l298n_1.png & l298n_2.png
// =====================================================

// -----------------------------------------
// 1. Encoder (Quadrature Hall / Optical)
// สายสัญญาณ:
//   ส้ม  = Phase A (OA -> Hardware Interrupt บน Mega)
//   เขียว = Phase B (OB -> Digital Pin เช็คทิศทาง)
//   เหลือง = VCC (5V)
//   ขาว   = GND
// -----------------------------------------
#define ENC_FL_A 18  // M1 หน้าซ้าย (Hardware INT3 บน Mega)
#define ENC_FL_B 26
#define ENC_FR_A 19  // M2 หน้าขวา (Hardware INT2 บน Mega)
#define ENC_FR_B 27
#define ENC_BL_A 2   // M3 หลังซ้าย (Hardware INT0 บน Mega)
#define ENC_BL_B 28
#define ENC_BR_A 3   // M4 หลังขวา (Hardware INT1 บน Mega)
#define ENC_BR_B 29

// ตัวคูณทิศทางการนับพัลส์ (1 หรือ -1 ให้หมุนเดินหน้าแล้ว ticks เป็นบวก)
#define ENC_FL_DIR  1
#define ENC_FR_DIR  1
#define ENC_BL_DIR  1
#define ENC_BR_DIR  1

// -----------------------------------------
// 2. L298N #1 (หน้า) — ขับ M1 (FL), M2 (FR)
// -----------------------------------------
#define ENA_FL 5   // ล้อหน้าซ้าย PWM
#define IN1_FL 22  // ล้อหน้าซ้าย ทิศทาง 1
#define IN2_FL 23  // ล้อหน้าซ้าย ทิศทาง 2
#define IN3_FR 24  // ล้อหน้าขวา ทิศทาง 1
#define IN4_FR 25  // ล้อหน้าขวา ทิศทาง 2
#define ENB_FR 6   // ล้อหน้าขวา PWM

// -----------------------------------------
// 3. L298N #2 (หลัง) — ขับ M3 (BL), M4 (BR)
// -----------------------------------------
#define ENA_BL 7   // ล้อหลังซ้าย PWM
#define IN1_BL 30  // ล้อหลังซ้าย ทิศทาง 1
#define IN2_BL 31  // ล้อหลังซ้าย ทิศทาง 2
#define IN3_BR 32  // ล้อหลังขวา ทิศทาง 1
#define IN4_BR 33  // ล้อหลังขวา ทิศทาง 2
#define ENB_BR 8   // ล้อหลังขวา PWM

// -----------------------------------------
// 4. CAN Bus (MCP2515 ผ่าน SPI บน Mega)
// SCK=52, MISO=50, MOSI=51, CS=10
// (หมายเหตุ: ตรวจจับข้อความผ่าน SPI checkReceive จึงไม่ชนกับขา Pin 2)
// -----------------------------------------
const byte CAN_CS_PIN = 10;
const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_TELEMETRY = 0x102; // ส่งรายงานสถานะ Encoder กลับผ่าน CAN
const unsigned long CAN_TIMEOUT = 300;        // Timeout (ms) ตัดการทำงานมอเตอร์อัตโนมัติ

// -----------------------------------------
// 5. Motion Tuning & Closed-Loop PID
// -----------------------------------------
constexpr uint8_t MOTOR_BASE_PWM = 180;       // ค่า PWM พื้นฐาน
constexpr uint8_t MOTOR_MAX_PWM  = 255;       // ค่า PWM สูงสุด
constexpr uint8_t MOTOR_MIN_PWM  = 60;        // ค่า PWM ขั้นต่ำที่มอเตอร์เอาชนะแรงเสียดทาน

// ความเร็วเป้าหมาย (Encoder ticks / second)
constexpr float TARGET_SPEED_STRAIGHT = 800.0f;
constexpr float TARGET_SPEED_SLIDE    = 700.0f;
constexpr float TARGET_SPEED_SPIN     = 600.0f;

// ค่า Gain PID สำหรับควบคุมความเร็วแต่ละล้อให้เท่ากัน
constexpr float PID_KP = 0.35f;
constexpr float PID_KI = 0.05f;
constexpr float PID_KD = 0.01f;

#endif
