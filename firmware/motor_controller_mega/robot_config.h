#ifndef MOTOR_CONTROLLER_MEGA_ROBOT_CONFIG_H
#define MOTOR_CONTROLLER_MEGA_ROBOT_CONFIG_H

#include <Arduino.h>

// Arduino Mega 2560 motor/encoder pin map.
#define ENC_FL_A 18
#define ENC_FL_B 26
#define ENC_FR_A 19
#define ENC_FR_B 27
#define ENC_BL_A 2
#define ENC_BL_B 28
#define ENC_BR_A 3
#define ENC_BR_B 29

#define ENC_FL_DIR 1
#define ENC_FR_DIR 1
#define ENC_BL_DIR 1
#define ENC_BR_DIR 1

#define ENA_FL 5
#define IN1_FL 22
#define IN2_FL 23
#define IN3_FR 24
#define IN4_FR 25
#define ENB_FR 6

#define ENA_BL 7
#define IN1_BL 30
#define IN2_BL 31
#define IN3_BR 32
#define IN4_BR 33
#define ENB_BR 8

// MCP2515 uses the Mega hardware SPI pins: MISO=50, MOSI=51, SCK=52.
// Keep CS on D10 to match the existing receiver wiring/documentation.
constexpr uint8_t CAN_CS_PIN = 10;
constexpr uint8_t SPI_SS_PIN = 53;
constexpr unsigned long CAN_ID_MOTOR = 0x100;
constexpr unsigned long CAN_ID_ARM = 0x101;
constexpr unsigned long CAN_ID_TELEMETRY = 0x102;
constexpr unsigned long CAN_TIMEOUT_MS = 300;
#define CAN_CLOCK_SET MCP_8MHZ

// Local sensors. Mega I2C is on D20/D21, so A0..A4 are available here.
constexpr uint8_t BATTERY_VOLTAGE_PIN = A0;
constexpr uint8_t IR_FRONT_PIN = A1;
constexpr uint8_t IR_RIGHT_PIN = A2;
constexpr uint8_t IR_REAR_PIN = A3;
constexpr uint8_t IR_LEFT_PIN = A4;

// Common 0-25 V analog voltage module divider.
constexpr float BATTERY_R1_OHMS = 30000.0f;
constexpr float BATTERY_R2_OHMS = 7500.0f;
constexpr float ADC_REFERENCE_VOLTAGE = 5.0f;
constexpr float ADC_COUNTS = 1023.0f;

constexpr uint8_t MOTOR_BASE_PWM = 180;
constexpr uint8_t MOTOR_MAX_PWM = 255;
constexpr float TARGET_SPEED_STRAIGHT = 800.0f;
constexpr float TARGET_SPEED_SLIDE = 700.0f;
constexpr float TARGET_SPEED_SPIN = 600.0f;
constexpr float PID_KP = 0.35f;
constexpr float PID_KI = 0.05f;
constexpr float PID_KD = 0.01f;

constexpr unsigned long CONTROL_LOOP_INTERVAL_MS = 20;
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;
constexpr unsigned long SERIAL_COMMAND_TIMEOUT_MS = 1000;
constexpr unsigned long CAN_FORWARD_INTERVAL_MS = 50;

#endif
