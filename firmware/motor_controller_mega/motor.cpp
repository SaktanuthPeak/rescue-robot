#include "motor.h"
#include "encoder.h"
#include "robot_config.h"

struct WheelPID
{
    float kp = PID_KP;
    float ki = PID_KI;
    float kd = PID_KD;
    float integral = 0.0f;
    float last_error = 0.0f;

    float compute(float target, float current, float dt)
    {
        if (target == 0.0f)
        {
            integral = 0.0f;
            last_error = 0.0f;
            return 0.0f;
        }

        const float error = target - current;
        integral = constrain(integral + error * dt, -100.0f, 100.0f);
        const float derivative = dt > 0.0f ? (error - last_error) / dt : 0.0f;
        last_error = error;

        const float feedforward = target > 0.0f ? MOTOR_BASE_PWM : -MOTOR_BASE_PWM;
        return constrain(
            feedforward + kp * error + ki * integral + kd * derivative,
            -static_cast<float>(MOTOR_MAX_PWM),
            static_cast<float>(MOTOR_MAX_PWM));
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

namespace
{
void set_single_motor(int in1, int in2, int pwm, int speed)
{
    if (speed > 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        analogWrite(pwm, constrain(speed, 0, 255));
    }
    else if (speed < 0)
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        analogWrite(pwm, constrain(-speed, 0, 255));
    }
    else
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
        analogWrite(pwm, 0);
    }
}

void reset_pid()
{
    pid_fl = WheelPID{};
    pid_fr = WheelPID{};
    pid_bl = WheelPID{};
    pid_br = WheelPID{};
}
}

void set_raw_motor_speeds(int pwm_fl, int pwm_fr, int pwm_bl, int pwm_br)
{
    set_single_motor(IN1_FL, IN2_FL, ENA_FL, pwm_fl);
    set_single_motor(IN3_FR, IN4_FR, ENB_FR, pwm_fr);
    set_single_motor(IN1_BL, IN2_BL, ENA_BL, pwm_bl);
    set_single_motor(IN3_BR, IN4_BR, ENB_BR, pwm_br);
}

void motor_init()
{
    pinMode(ENA_FL, OUTPUT); pinMode(IN1_FL, OUTPUT); pinMode(IN2_FL, OUTPUT);
    pinMode(ENB_FR, OUTPUT); pinMode(IN3_FR, OUTPUT); pinMode(IN4_FR, OUTPUT);
    pinMode(ENA_BL, OUTPUT); pinMode(IN1_BL, OUTPUT); pinMode(IN2_BL, OUTPUT);
    pinMode(ENB_BR, OUTPUT); pinMode(IN3_BR, OUTPUT); pinMode(IN4_BR, OUTPUT);
    motor_stop();
}

void motor_stop()
{
    target_fl = target_fr = target_bl = target_br = 0.0f;
    reset_pid();
    set_raw_motor_speeds(0, 0, 0, 0);
}

void motor_forward()
{
    target_fl = target_fr = target_bl = target_br = TARGET_SPEED_STRAIGHT;
}

void motor_backward()
{
    target_fl = target_fr = target_bl = target_br = -TARGET_SPEED_STRAIGHT;
}

void motor_slide_right()
{
    target_fl = TARGET_SPEED_SLIDE; target_fr = -TARGET_SPEED_SLIDE;
    target_bl = -TARGET_SPEED_SLIDE; target_br = TARGET_SPEED_SLIDE;
}

void motor_slide_left()
{
    target_fl = -TARGET_SPEED_SLIDE; target_fr = TARGET_SPEED_SLIDE;
    target_bl = TARGET_SPEED_SLIDE; target_br = -TARGET_SPEED_SLIDE;
}

void motor_forward_right()
{
    target_fl = TARGET_SPEED_STRAIGHT; target_fr = 0.0f;
    target_bl = 0.0f; target_br = TARGET_SPEED_STRAIGHT;
}

void motor_forward_left()
{
    target_fl = 0.0f; target_fr = TARGET_SPEED_STRAIGHT;
    target_bl = TARGET_SPEED_STRAIGHT; target_br = 0.0f;
}

void motor_backward_left()
{
    target_fl = -TARGET_SPEED_STRAIGHT; target_fr = 0.0f;
    target_bl = 0.0f; target_br = -TARGET_SPEED_STRAIGHT;
}

void motor_backward_right()
{
    target_fl = 0.0f; target_fr = -TARGET_SPEED_STRAIGHT;
    target_bl = -TARGET_SPEED_STRAIGHT; target_br = 0.0f;
}

void motor_spin_right()
{
    target_fl = TARGET_SPEED_SPIN; target_fr = -TARGET_SPEED_SPIN;
    target_bl = TARGET_SPEED_SPIN; target_br = -TARGET_SPEED_SPIN;
}

void motor_spin_left()
{
    target_fl = -TARGET_SPEED_SPIN; target_fr = TARGET_SPEED_SPIN;
    target_bl = -TARGET_SPEED_SPIN; target_br = TARGET_SPEED_SPIN;
}

void motor_update_pid(float dt)
{
    if (target_fl == 0.0f && target_fr == 0.0f &&
        target_bl == 0.0f && target_br == 0.0f)
    {
        set_raw_motor_speeds(0, 0, 0, 0);
        return;
    }

    set_raw_motor_speeds(
        static_cast<int>(pid_fl.compute(target_fl, encoder_get_speed_fl(), dt)),
        static_cast<int>(pid_fr.compute(target_fr, encoder_get_speed_fr(), dt)),
        static_cast<int>(pid_bl.compute(target_bl, encoder_get_speed_bl(), dt)),
        static_cast<int>(pid_br.compute(target_br, encoder_get_speed_br(), dt)));
}
