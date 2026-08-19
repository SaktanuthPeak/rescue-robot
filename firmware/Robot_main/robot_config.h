#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

#include <Arduino.h>

// -----------------------------------------
// Motor pins - L298N ตัวที่ 1 (ล้อคู่หน้า)
// ลำดับ: ENA, IN1, IN2, IN3, IN4, ENB
// -----------------------------------------
#define ENA_FL 2   // ล้อหน้าซ้าย PWM
#define IN1_FL 3   // ล้อหน้าซ้าย ทิศทาง 1
#define IN2_FL 4   // ล้อหน้าซ้าย ทิศทาง 2
#define IN3_FR 5   // ล้อหน้าขวา ทิศทาง 1
#define IN4_FR 6   // ล้อหน้าขวา ทิศทาง 2
#define ENB_FR 7   // ล้อหน้าขวา PWM

// -----------------------------------------
// Motor pins - L298N ตัวที่ 2 (ล้อคู่หลัง)
// ลำดับ: ENA, IN1, IN2, IN3, IN4, ENB
// -----------------------------------------
#define ENA_BL 8   // ล้อหลังซ้าย PWM
#define IN1_BL 9   // ล้อหลังซ้าย ทิศทาง 1
#define IN2_BL 10  // ล้อหลังซ้าย ทิศทาง 2
#define IN3_BR 11  // ล้อหลังขวา ทิศทาง 1
#define IN4_BR 12  // ล้อหลังขวา ทิศทาง 2
#define ENB_BR 13  // ล้อหลังขวา PWM (เพิ่มขา 13 ให้เป็น ENB นะครับ)

// -----------------------------------------
// PS2 software SPI pins (รีโมต)
// ใช้พิน Analog หรือ Digital ของ Mega ก็ได้ (ในที่นี้คงไว้ที่ A0-A3)
// -----------------------------------------
constexpr uint8_t PS2_DAT_PIN = A0;
constexpr uint8_t PS2_CMD_PIN = A1;
constexpr uint8_t PS2_ATT_PIN = A2;
constexpr uint8_t PS2_CLK_PIN = A3;

// -----------------------------------------
// Extra peripherals
// ย้ายไปใช้พินสูงๆ ของบอร์ด Mega เพื่อความเป็นระเบียบและไม่ชนกัน
// -----------------------------------------8778yuuuuuuuuu666666666666666666666666666666666666uuuuuuuu0-hj
constexpr uint8_t BUZZER_PIN = 44;
constexpr uint8_t SERVO_PIN = 45;
constexpr uint8_t ULTRASONIC_TRIG_PIN = 46;
constexpr uint8_t ULTRASONIC_ECHO_PIN = 47;

// -----------------------------------------
// Motion tuning
// -----------------------------------------
constexpr uint8_t MOTOR_FULL_SPEED = 200;
constexpr uint8_t MOTOR_TURN_SPEED = 170;
constexpr uint8_t MOTOR_CURVE_SPEED = 140;

#endif