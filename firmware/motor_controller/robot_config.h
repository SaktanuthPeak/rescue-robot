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
#define ENB_BR 13  // ล้อหลังขวา PWM

// -----------------------------------------
// Motion tuning
// -----------------------------------------
constexpr uint8_t MOTOR_FULL_SPEED = 200;
constexpr uint8_t MOTOR_TURN_SPEED = 170;
constexpr uint8_t MOTOR_CURVE_SPEED = 140;

#endif
