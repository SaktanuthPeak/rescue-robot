#ifndef ROBOT_CONFIG_H
#define ROBOT_CONFIG_H

#include <Arduino.h>

// =====================================================
// การกำหนด Pin สำหรับตัวส่งสัญญาณ CAN รีโมท PS2 (can-sender)
// =====================================================

// -----------------------------------------
// PS2 Gamepad Receiver Pins
// สายสัญญาณมาตรฐานตัวรับจอย PS2:
//   DAT = น้ำตาล/ส้ม-น้ำตาล
//   CMD = ส้ม
//   ATT = เหลือง (Attention / CS)
//   CLK = น้ำเงิน (Clock)
// -----------------------------------------
constexpr uint8_t PS2_DAT_PIN = A0;
constexpr uint8_t PS2_CMD_PIN = A1;
constexpr uint8_t PS2_ATT_PIN = A2;
constexpr uint8_t PS2_CLK_PIN = A3;

// -----------------------------------------
// MCP2515 CAN Bus Module (SPI)
// -----------------------------------------
constexpr byte CAN_CS_PIN = 10;
constexpr unsigned long CAN_ID_MOTOR = 0x100; // ID ควบคุมมอเตอร์ล้อ
constexpr unsigned long CAN_ID_ARM   = 0x101; // ID ควบคุมแขนกล

// ความถี่ Crystal บนโมดูล MCP2515:
// ให้ดูตัวถังโลหะสีเงินทรงรีบนบอร์ด MCP2515:
// - ถ้าเขียน 8.000 (หรือ 8M)  -> ให้ใช้ MCP_8MHZ
// - ถ้าเขียน 16.000 (หรือ 16M) -> ให้ใช้ MCP_16MHZ
#define CAN_CLOCK_SET MCP_8MHZ

// -----------------------------------------
// Timing & Controller Settings
// -----------------------------------------
constexpr unsigned long CAN_SEND_INTERVAL_MS   = 50;  // ส่งสถานะออก CAN Bus ทุก 50 ms (20 Hz Heartbeat)
constexpr unsigned long PS2_POLL_INTERVAL_MS   = 15;  // อ่านค่าจากจอย PS2 ทุก 15 ms (~66 Hz)
constexpr unsigned long DEBUG_PRINT_INTERVAL_MS = 250; // พิมพ์สถานะออก Serial ทุก 250 ms
constexpr uint8_t PS2_DEADZONE                 = 15;  // ค่า Deadzone ก้านโยกอนาล็อก

#endif
