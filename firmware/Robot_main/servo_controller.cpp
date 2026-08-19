#include "servo_controller.h"

PCA9685Control pwm = PCA9685Control(0x40);

const int BASE_STOP = 275; // ค่าที่ทำให้มอเตอร์ฐานหยุดนิ่งสนิท
int arm1_position = 470;
int arm2_position = 200;
int arm3_position = 400;
void servo_init()
{
  pwm.begin();
  pwm.setPWMFreq(50);

  Serial.println("เริ่มต้น: กำลังตั้งค่าแขนกล");
  pwm.setPWM(0, 0, BASE_STOP);
  pwm.setPWM(1, 0, arm1_position);
  pwm.setPWM(2, 0, arm2_position);
  pwm.setPWM(3, 0, arm3_position);
  pwm.setPWM(4, 0, BASE_STOP);
  pwm.setPWM(5, 0, 450);

  Serial.println("เริ่มต้น: กำลังจัดตำแหน่งแขนกล");
  // turnBase(5, 300, 1000);
  // delay(500);

  Serial.println("เริ่มต้น: จบการตั้งค่าแขนกล");
  delay(1000);
}

//  servo 360
// รับค่าความเร็ว/ทิศทาง (speedPulse) และเวลาที่ให้หมุน (durationMs)
void turnBase(uint8_t channel, uint16_t speedPulse, int durationMs)
{
  pwm.setPWM(channel, 0, speedPulse);
  delay(durationMs);
  pwm.setPWM(channel, 0, BASE_STOP);
}

void gripper_clamp()
{
  pwm.setPWM(5, 0, 200);
}
void gripper_release()
{
  pwm.setPWM(5, 0, 450);
}

void arm_stop()
{
  pwm.setPWM(0, 0, BASE_STOP);
  pwm.setPWM(1, 0, arm1_position);
  pwm.setPWM(2, 0, arm2_position);
  pwm.setPWM(3, 0, arm3_position);
  pwm.setPWM(4, 0, BASE_STOP);
}
// Smoothly move a channel from current position to target position
static void smoothMove(uint8_t channel, int &currentPos, int targetPos, int step = 5, int stepDelayMs = 15)
{
  if (currentPos == targetPos) return;
  if (currentPos < targetPos)
  {
    for (int p = currentPos; p < targetPos; p += step)
    {
      pwm.setPWM(channel, 0, p);
      delay(stepDelayMs);
    }
  }
  else
  {
    for (int p = currentPos; p > targetPos; p -= step)
    {
      pwm.setPWM(channel, 0, p);
      delay(stepDelayMs);
    }
  }
  // ensure exact final position
  currentPos = targetPos;
  pwm.setPWM(channel, 0, currentPos);
}

void arm_forward()
{
  // smoothMove(1, arm1_position, arm1_position - 100);
  smoothMove(2, arm2_position, arm2_position - 10);
  smoothMove(3, arm3_position, arm3_position - 5);
}

void arm_backward()
{
  // smoothMove(1, arm1_position, arm1_position + 100);
  smoothMove(2, arm2_position, arm2_position + 10);
  smoothMove(3, arm3_position, arm3_position + 5);
}
void arm_turn_left()
{
  turnBase(0, 250, 100);
}
void arm_turn_right()
{
  turnBase(0, 300, 100);
}

