#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <Arduino.h>
#include "robot_config.h"
#include "PS2X_lib.h"

typedef enum
{
    STOP = 0,
    FORWARD = 1,
    BACKWARD = 2,
    LEFT = 3,
    RIGHT = 4,
    FORWARD_LEFT = 5,
    FORWARD_RIGHT = 6,
    BACKWARD_LEFT = 7,
    BACKWARD_RIGHT = 8,
    Clamp = 9,
    Release = 10,
    SPIN_LEFT = 11,
    SPIN_RIGHT = 12
} PS2_Status;

// อ็อบเจกต์ ps2x จากไลบรารี PS2X_lib
extern PS2X ps2x;

// ฟังก์ชันเริ่มต้นและประมวลผลคำสั่ง PS2
bool PS2_Init();
void PS2_Update();
PS2_Status PS2_GetMotorStatus();
PS2_Status PS2_GetArmStatus();

// ฟังก์ชันแปลงแกนก้านโยกเป็นสถานะการเคลื่อนที่
PS2_Status get_status_from_sticks(int x, int y, PS2_Status center_value);

// ฟังก์ชันพิมพ์ข้อมูลดีบัก
void print_debug(PS2_Status status_motor, PS2_Status status_arm);

#endif
