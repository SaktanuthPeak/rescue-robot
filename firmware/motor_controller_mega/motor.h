#ifndef MOTOR_CONTROLLER_MEGA_MOTOR_H
#define MOTOR_CONTROLLER_MEGA_MOTOR_H

#include <Arduino.h>

void motor_init();
void motor_stop();
void motor_forward();
void motor_backward();
void motor_slide_left();
void motor_slide_right();
void motor_forward_left();
void motor_forward_right();
void motor_backward_left();
void motor_backward_right();
void motor_spin_left();
void motor_spin_right();
void motor_update_pid(float dt);
void set_raw_motor_speeds(int pwm_fl, int pwm_fr, int pwm_bl, int pwm_br);

#endif
