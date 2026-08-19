#include "motor.h"
// สมมติว่ามี robot_config.h ที่ประกาศพินไว้แล้ว 
// หรือคุณสามารถใส่ #define พินตรงนี้ได้เลยถ้าต้องการ
#include "robot_config.h" 

/* ตัวอย่างการกำหนดพิน (เพื่อให้เห็นภาพ ต้องไปใส่ใน robot_config.h)
// L298N ตัวที่ 1 (ล้อหน้า)
#define ENA_FL 9   // ล้อหน้าซ้าย PWM
#define IN1_FL 2   // ล้อหน้าซ้าย ทิศทาง 1
#define IN2_FL 3   // ล้อหน้าซ้าย ทิศทาง 2
#define ENB_FR 10  // ล้อหน้าขวา PWM
#define IN3_FR 4   // ล้อหน้าขวา ทิศทาง 1
#define IN4_FR 5   // ล้อหน้าขวา ทิศทาง 2

// L298N ตัวที่ 2 (ล้อหลัง)
#define ENA_BL 11  // ล้อหลังซ้าย PWM
#define IN1_BL 6   // ล้อหลังซ้าย ทิศทาง 1
#define IN2_BL 7   // ล้อหลังซ้าย ทิศทาง 2
#define ENB_BR 12  // ล้อหลังขวา PWM
#define IN3_BR 8   // ล้อหลังขวา ทิศทาง 1
#define IN4_BR 13  // ล้อหลังขวา ทิศทาง 2

#define MOTOR_FULL_SPEED 255
*/

namespace
{
  // ฟังก์ชันช่วยเหลือสำหรับคุมมอเตอร์แต่ละตัวผ่าน L298N
  // state: 1 = เดินหน้า, -1 = ถอยหลัง, 0 = หยุด
  void setSingleMotor(int in1Pin, int in2Pin, int pwmPin, int state, uint8_t speed)
  {
    if (state == 1) { // เดินหน้า
      digitalWrite(in1Pin, HIGH);
      digitalWrite(in2Pin, LOW);
      analogWrite(pwmPin, speed);
    } 
    else if (state == -1) { // ถอยหลัง
      digitalWrite(in1Pin, LOW);
      digitalWrite(in2Pin, HIGH);
      analogWrite(pwmPin, speed);
    } 
    else { // หยุด
      digitalWrite(in1Pin, LOW);
      digitalWrite(in2Pin, LOW);
      analogWrite(pwmPin, 0);
    }
  }

  // ฟังก์ชันรวมสั่ง 4 ล้อ (1=เดินหน้า, -1=ถอยหลัง, 0=หยุด)
  void setAllMotors(int flState, int frState, int blState, int brState, uint8_t speed)
  {
    setSingleMotor(IN1_FL, IN2_FL, ENA_FL, flState, speed); // หน้าซ้าย
    setSingleMotor(IN3_FR, IN4_FR, ENB_FR, frState, speed); // หน้าขวา
    setSingleMotor(IN1_BL, IN2_BL, ENA_BL, blState, speed); // หลังซ้าย
    setSingleMotor(IN3_BR, IN4_BR, ENB_BR, brState, speed); // หลังขวา
  }
}

void motor_init()
{
  // L298N 1
  pinMode(ENA_FL, OUTPUT); pinMode(IN1_FL, OUTPUT); pinMode(IN2_FL, OUTPUT);
  pinMode(ENB_FR, OUTPUT); pinMode(IN3_FR, OUTPUT); pinMode(IN4_FR, OUTPUT);
  // L298N 2
  pinMode(ENA_BL, OUTPUT); pinMode(IN1_BL, OUTPUT); pinMode(IN2_BL, OUTPUT);
  pinMode(ENB_BR, OUTPUT); pinMode(IN3_BR, OUTPUT); pinMode(IN4_BR, OUTPUT);

  motor_stop();
}

void motor_stop()
{
  setAllMotors(0, 0, 0, 0, 0);
}

// อ้างอิงจากภาพ: Straight ahead (ลูกศรขึ้นหมด)
void motor_forward()
{
  setAllMotors(1, 1, 1, 1, MOTOR_FULL_SPEED);
}

void motor_backward()
{
  setAllMotors(-1, -1, -1, -1, MOTOR_FULL_SPEED);
}

// อ้างอิงจากภาพ: Side way (ลูกศรทแยง) สไลด์ขวา
void motor_slide_right()
{
  // หน้าซ้ายไปหน้า, หน้าขวาไปหลัง, หลังซ้ายไปหลัง, หลังขวาไปหน้า
  setAllMotors(1, -1, -1, 1, MOTOR_FULL_SPEED);
}

// สไลด์ซ้าย (ตรงข้ามสไลด์ขวา)
void motor_slide_left()
{
  setAllMotors(-1, 1, 1, -1, MOTOR_FULL_SPEED);
}

// อ้างอิงจากภาพ: Diagonal (เดินหน้าทแยงขวา)
void motor_forward_right()
{
  // หน้าซ้ายไปหน้า, หลังขวาไปหน้า, ล้ออื่นหยุด
  setAllMotors(1, 0, 0, 1, MOTOR_FULL_SPEED);
}

// เดินหน้าทแยงซ้าย
void motor_forward_left()
{
  // หน้าขวาไปหน้า, หลังซ้ายไปหน้า, ล้ออื่นหยุด
  setAllMotors(0, 1, 1, 0, MOTOR_FULL_SPEED);
}

// ถอยหลังทแยงซ้าย
void motor_backward_left()
{
  setAllMotors(-1, 0, 0, -1, MOTOR_FULL_SPEED);
}

// ถอยหลังทแยงขวา
void motor_backward_right()
{
  setAllMotors(0, -1, -1, 0, MOTOR_FULL_SPEED);
}

// อ้างอิงจากภาพ: Turn round (หมุนตัว) หมุนขวา
void motor_spin_right()
{
  // ซ้ายไปหน้า ขวาไปหลัง
  setAllMotors(1, -1, 1, -1, MOTOR_FULL_SPEED);
}

// หมุนซ้าย
void motor_spin_left()
{
  // ซ้ายไปหลัง ขวาไปหน้า
  setAllMotors(-1, 1, -1, 1, MOTOR_FULL_SPEED);
}