#include "motor.h"
#include "robot_config.h"

namespace {
  // สั่งงาน L298N รายล้อ: ค่า speed ระหว่าง -255 ถึง 255
  void setSingleMotor(int in1Pin, int in2Pin, int pwmPin, int speed) {
      if (speed > 0) { // เดินหน้า
          digitalWrite(in1Pin, HIGH);
          digitalWrite(in2Pin, LOW);
          analogWrite(pwmPin, constrain(speed, 0, 255));
      } else if (speed < 0) { // ถอยหลัง
          digitalWrite(in1Pin, LOW);
          digitalWrite(in2Pin, HIGH);
          analogWrite(pwmPin, constrain(-speed, 0, 255));
      } else { // หยุด
          digitalWrite(in1Pin, LOW);
          digitalWrite(in2Pin, LOW);
          analogWrite(pwmPin, 0);
      }
  }
}

void set_raw_motor_speeds(int pwm_fl, int pwm_fr, int pwm_bl, int pwm_br) {
    setSingleMotor(IN1_FL, IN2_FL, ENA_FL, pwm_fl);
    setSingleMotor(IN3_FR, IN4_FR, ENB_FR, pwm_fr);
    setSingleMotor(IN1_BL, IN2_BL, ENA_BL, pwm_bl);
    setSingleMotor(IN3_BR, IN4_BR, ENB_BR, pwm_br);
}

void motor_init() {
    // L298N #1 (หน้า: FL, FR)
    pinMode(ENA_FL, OUTPUT); pinMode(IN1_FL, OUTPUT); pinMode(IN2_FL, OUTPUT);
    pinMode(ENB_FR, OUTPUT); pinMode(IN3_FR, OUTPUT); pinMode(IN4_FR, OUTPUT);

    // L298N #2 (หลัง: BL, BR)
    pinMode(ENA_BL, OUTPUT); pinMode(IN1_BL, OUTPUT); pinMode(IN2_BL, OUTPUT);
    pinMode(ENB_BR, OUTPUT); pinMode(IN3_BR, OUTPUT); pinMode(IN4_BR, OUTPUT);

    motor_stop();
}

void motor_stop() {
    set_raw_motor_speeds(0, 0, 0, 0);
}

// ----------------------------------------------------
// Mecanum direct-PWM commands (ไม่มี PID feedback)
// ----------------------------------------------------
void motor_forward() {
    set_raw_motor_speeds(MOTOR_BASE_PWM, MOTOR_BASE_PWM,
                         MOTOR_BASE_PWM, MOTOR_BASE_PWM);
}

void motor_backward() {
    set_raw_motor_speeds(-MOTOR_BASE_PWM, -MOTOR_BASE_PWM,
                         -MOTOR_BASE_PWM, -MOTOR_BASE_PWM);
}

void motor_slide_right() {
    set_raw_motor_speeds(MOTOR_BASE_PWM, -MOTOR_BASE_PWM,
                         -MOTOR_BASE_PWM, MOTOR_BASE_PWM);
}

void motor_slide_left() {
    set_raw_motor_speeds(-MOTOR_BASE_PWM, MOTOR_BASE_PWM,
                         MOTOR_BASE_PWM, -MOTOR_BASE_PWM);
}

void motor_forward_right() {
    set_raw_motor_speeds(MOTOR_BASE_PWM, 0, 0, MOTOR_BASE_PWM);
}

void motor_forward_left() {
    set_raw_motor_speeds(0, MOTOR_BASE_PWM, MOTOR_BASE_PWM, 0);
}

void motor_backward_left() {
    set_raw_motor_speeds(-MOTOR_BASE_PWM, 0, 0, -MOTOR_BASE_PWM);
}

void motor_backward_right() {
    set_raw_motor_speeds(0, -MOTOR_BASE_PWM, -MOTOR_BASE_PWM, 0);
}

void motor_spin_right() {
    set_raw_motor_speeds(MOTOR_BASE_PWM, -MOTOR_BASE_PWM,
                         MOTOR_BASE_PWM, -MOTOR_BASE_PWM);
}

void motor_spin_left() {
    set_raw_motor_speeds(-MOTOR_BASE_PWM, MOTOR_BASE_PWM,
                         -MOTOR_BASE_PWM, MOTOR_BASE_PWM);
}
