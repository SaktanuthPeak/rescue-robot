#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <Arduino.h>
#include "PCA9685_Control.h"

extern PCA9685Control pwm;

extern const int BASE_STOP;

void servo_init();
void turnBase(uint8_t channel, uint16_t speedPulse, int durationMs);
void gripper_clamp();
void gripper_release();
void arm_stop();
void arm_forward();
void arm_backward();
void arm_turn_left();
void arm_turn_right();

#endif
