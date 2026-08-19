#include "ultrasonic_sensor.h"
#include "robot_config.h"

namespace
{
    const unsigned long SERVO_STEP_INTERVAL_MS = 30UL;
    const unsigned long DISTANCE_SAMPLE_INTERVAL_MS = 70UL;
    const unsigned long ULTRASONIC_TIMEOUT_US = 8000UL;
    const unsigned long SERVO_REFRESH_INTERVAL_MS = 20UL;

    uint8_t servoAngle = 0;
    int8_t sweepDirection = 1;
    uint16_t servoPulseWidthUs = 1500;
    unsigned long lastSweepUpdateMs = 0;
    unsigned long lastDistanceSampleMs = 0;
    unsigned long lastServoPulseMs = 0;

    void setServoAngle(uint8_t angle)
    {
        servoAngle = constrain(angle, 0, 180);
        servoPulseWidthUs = map(servoAngle, 0, 180, 1000, 2000);
    }
}

void ultrasonic_init()
{
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW);

    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    setServoAngle(0);
}

int ultrasonic_read_distance_cm()
{
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    unsigned long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
    if (duration == 0)
    {
        return 0;
    }

    return (int)(duration * 0.0343 / 2.0);
}

void ultrasonic_update()
{
    unsigned long nowMs = millis();

    // 1. ระบบส่งสัญญาณ Servo ด้วย Software (ทำงานทุก 20ms)
    if (nowMs - lastServoPulseMs >= SERVO_REFRESH_INTERVAL_MS)
    {
        lastServoPulseMs = nowMs;
        digitalWrite(SERVO_PIN, HIGH);
        delayMicroseconds(servoPulseWidthUs); // หน่วงเวลา 1-2 ms เพื่อสร้างมุม
        digitalWrite(SERVO_PIN, LOW);
    }

    // 2. ระบบคำนวณองศาการกวาดของ Servo
    if (nowMs - lastSweepUpdateMs >= SERVO_STEP_INTERVAL_MS)
    {
        lastSweepUpdateMs = nowMs;

        if (sweepDirection > 0 && servoAngle >= 180)
        {
            sweepDirection = -1;
        }
        else if (sweepDirection < 0 && servoAngle <= 0)
        {
            sweepDirection = 1;
        }

        setServoAngle((uint8_t)constrain((int)servoAngle + sweepDirection, 0, 180));
    }

    // 3. ระบบอ่านค่า Ultrasonic
    if (nowMs - lastDistanceSampleMs >= DISTANCE_SAMPLE_INTERVAL_MS)
    {
        lastDistanceSampleMs = nowMs;
        int lastDistanceCm = ultrasonic_read_distance_cm();

        Serial.print(servoAngle);
        Serial.print(",");
        Serial.print(lastDistanceCm);
        Serial.println(".");
    }
}