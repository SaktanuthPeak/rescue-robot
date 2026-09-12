#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include <Arduino.h>

#include "robot_config.h"

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
    SPIN_LEFT = 9,
    SPIN_RIGHT = 10,
    Pump_On = 11,
    Pump_Off = 12,
    Head_Up = 13,
    Head_Down = 14
} PS2_Status;

void PS2_ReadData(uint8_t *ps2_data);
PS2_Status PS2_GetStatus(uint8_t *ps2_data);

void apply_motor_from_status(PS2_Status current);
void apply_arm_from_status(PS2_Status current);
void print_debug(PS2_Status status_left, PS2_Status status_right, PS2_Status gripper_status);
#endif