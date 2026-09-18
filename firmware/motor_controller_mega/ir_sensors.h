#ifndef MOTOR_CONTROLLER_MEGA_IR_SENSORS_H
#define MOTOR_CONTROLLER_MEGA_IR_SENSORS_H

#include <Arduino.h>

struct IrSample
{
    uint16_t front;
    uint16_t right;
    uint16_t rear;
    uint16_t left;
};

void ir_sensors_init();
IrSample ir_sensors_read();

#endif
