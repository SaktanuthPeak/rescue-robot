#include "motor.h"
#include "robot_config.h"
#include "encoder.h"

struct WheelPID {
    float kp = PID_KP;
    float ki = PID_KI;
    float kd = PID_KD;
    float integral = 0.0f;
    float last_error = 0.0f;

    float compute(float target_speed, float current_speed, float dt) {
        if (target_speed == 0.0f) {
            reset();
            return 0.0f;
        }

        float error = target_speed - current_speed;
        integral += error * dt;
        integral = constrain(integral, -100.0f, 100.0f); // Anti-windup

        float derivative = (dt > 0.0f) ? (error - last_error) / dt : 0.0f;
        last_error = error;

        // Feedforward: ให้พลังงานเริ่มต้นทันทีตามทิศทาง
        float ff = 0.0f;
        if (target_speed > 0.0f) {
            ff = (float)MOTOR_BASE_PWM;
        } else if (target_speed < 0.0f) {
            ff = -(float)MOTOR_BASE_PWM;
        }

        float output = ff + (kp * error) + (ki * integral) + (kd * derivative);
        return constrain(output, -(float)MOTOR_MAX_PWM, (float)MOTOR_MAX_PWM);
    }

    void reset() {
        integral = 0.0f;
        last_error = 0.0f;
    }
};

static WheelPID pid_fl;
static WheelPID pid_fr;
static WheelPID pid_bl;
static WheelPID pid_br;

static float target_fl = 0.0f;
static float target_fr = 0.0f;
static float target_bl = 0.0f;
static float target_br = 0.0f;

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
    target_fl = 0.0f;
    target_fr = 0.0f;
    target_bl = 0.0f;
    target_br = 0.0f;

    pid_fl.reset();
    pid_fr.reset();
    pid_bl.reset();
    pid_br.reset();

    set_raw_motor_speeds(0, 0, 0, 0);
}

// ----------------------------------------------------
// Mecanum Kinematics
// ----------------------------------------------------
void motor_forward() {
    target_fl = TARGET_SPEED_STRAIGHT;
    target_fr = TARGET_SPEED_STRAIGHT;
    target_bl = TARGET_SPEED_STRAIGHT;
    target_br = TARGET_SPEED_STRAIGHT;
}

void motor_backward() {
    target_fl = -TARGET_SPEED_STRAIGHT;
    target_fr = -TARGET_SPEED_STRAIGHT;
    target_bl = -TARGET_SPEED_STRAIGHT;
    target_br = -TARGET_SPEED_STRAIGHT;
}

void motor_slide_right() {
    target_fl = TARGET_SPEED_SLIDE;
    target_fr = -TARGET_SPEED_SLIDE;
    target_bl = -TARGET_SPEED_SLIDE;
    target_br = TARGET_SPEED_SLIDE;
}

void motor_slide_left() {
    target_fl = -TARGET_SPEED_SLIDE;
    target_fr = TARGET_SPEED_SLIDE;
    target_bl = TARGET_SPEED_SLIDE;
    target_br = -TARGET_SPEED_SLIDE;
}

void motor_forward_right() {
    target_fl = TARGET_SPEED_STRAIGHT;
    target_fr = 0.0f;
    target_bl = 0.0f;
    target_br = TARGET_SPEED_STRAIGHT;
}

void motor_forward_left() {
    target_fl = 0.0f;
    target_fr = TARGET_SPEED_STRAIGHT;
    target_bl = TARGET_SPEED_STRAIGHT;
    target_br = 0.0f;
}

void motor_backward_left() {
    target_fl = -TARGET_SPEED_STRAIGHT;
    target_fr = 0.0f;
    target_bl = 0.0f;
    target_br = -TARGET_SPEED_STRAIGHT;
}

void motor_backward_right() {
    target_fl = 0.0f;
    target_fr = -TARGET_SPEED_STRAIGHT;
    target_bl = -TARGET_SPEED_STRAIGHT;
    target_br = 0.0f;
}

void motor_spin_right() {
    target_fl = TARGET_SPEED_SPIN;
    target_fr = -TARGET_SPEED_SPIN;
    target_bl = TARGET_SPEED_SPIN;
    target_br = -TARGET_SPEED_SPIN;
}

void motor_spin_left() {
    target_fl = -TARGET_SPEED_SPIN;
    target_fr = TARGET_SPEED_SPIN;
    target_bl = -TARGET_SPEED_SPIN;
    target_br = TARGET_SPEED_SPIN;
}

void motor_update_pid(float dt) {
    if (target_fl == 0.0f && target_fr == 0.0f && target_bl == 0.0f && target_br == 0.0f) {
        set_raw_motor_speeds(0, 0, 0, 0);
        return;
    }

    float cur_fl = encoder_get_speed_fl();
    float cur_fr = encoder_get_speed_fr();
    float cur_bl = encoder_get_speed_bl();
    float cur_br = encoder_get_speed_br();

    int pwm_fl = (int)pid_fl.compute(target_fl, cur_fl, dt);
    int pwm_fr = (int)pid_fr.compute(target_fr, cur_fr, dt);
    int pwm_bl = (int)pid_bl.compute(target_bl, cur_bl, dt);
    int pwm_br = (int)pid_br.compute(target_br, cur_br, dt);

    set_raw_motor_speeds(pwm_fl, pwm_fr, pwm_bl, pwm_br);
}
