#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

void motor_init();
void motor_stop();
void motor_forward();
void motor_backward();
void motor_slide_left();   // เปลี่ยนจากเลี้ยวเป็นสไลด์
void motor_slide_right();  // เปลี่ยนจากเลี้ยวเป็นสไลด์
void motor_forward_left();
void motor_forward_right();
void motor_backward_left();
void motor_backward_right();

// เพิ่มการหมุนอยู่กับที่
void motor_spin_left();
void motor_spin_right();

#endif