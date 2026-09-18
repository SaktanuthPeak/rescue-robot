#include "ir_sensors.h"
#include "robot_config.h"

void ir_sensors_init()
{
    pinMode(IR_FRONT_PIN, INPUT);
    pinMode(IR_RIGHT_PIN, INPUT);
    pinMode(IR_REAR_PIN, INPUT);
    pinMode(IR_LEFT_PIN, INPUT);
}

IrSample ir_sensors_read()
{
    return IrSample{
        static_cast<uint16_t>(analogRead(IR_FRONT_PIN)),
        static_cast<uint16_t>(analogRead(IR_RIGHT_PIN)),
        static_cast<uint16_t>(analogRead(IR_REAR_PIN)),
        static_cast<uint16_t>(analogRead(IR_LEFT_PIN)),
    };
}
