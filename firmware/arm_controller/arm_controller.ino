#include <SPI.h>
#include <mcp_can.h>
#include "motor.h"
#include "PCA9685_Control.h" // นำเข้าไลบรารีคุม Servo

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2; 

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;
const unsigned long CAN_TIMEOUT = 300; 

MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- PCA9685 configuration ----------------
PCA9685Control pca;

// ตัวแปรจดจำตำแหน่งปัจจุบันของ Servo
int servo0_pwm = 305; // ช่อง 0: ฐานหมุนซ้าย-ขวา
int servo1_pwm = 305; // ช่อง 1: แขนยกขึ้น-ลง
int servo2_pwm = 305; // ช่อง 2: Gripper หนีบ-ปล่อย

unsigned long last_servo_update_ms = 0;
const unsigned long SERVO_UPDATE_INTERVAL = 15; // ความเร็วในการขยับแขนกล (ค่าน้อย = ขยับไว)

// ---------------- Status definition ----------------
enum PS2_Status : uint8_t
{
    STOP = 0,
    FORWARD = 1,
    BACKWARD = 2,
    LEFT = 3,
    RIGHT = 4,
    FORWARD_LEFT = 5,
    FORWARD_RIGHT = 6,
    BACKWARD_LEFT = 7,
    BACKWARD_RIGHT = 8,
    Release = 9,
    Clamp = 10
};

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;

bool motorCANAlive = false;
bool armCANAlive = false;

int lastMotorStatus = -1;
int lastArmStatus = -1;

// ==================================================
// ฟังก์ชันเรียกใช้งาน Motor ตาม Status ที่รับมาจาก CAN
// ==================================================
void apply_motor_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD: motor_forward(); break;
    case BACKWARD: motor_backward(); break;
    case LEFT: motor_slide_left(); break;
    case RIGHT: motor_slide_right(); break;
    case FORWARD_LEFT: motor_forward_left(); break;
    case FORWARD_RIGHT: motor_forward_right(); break;
    case BACKWARD_LEFT: motor_backward_left(); break;
    case BACKWARD_RIGHT: motor_backward_right(); break;
    case STOP:
    default: motor_stop(); break;
    }
}

// ==================================================
// ตรวจสอบความถูกต้องของข้อมูล (ป้องกันค่าขยะ)
// ==================================================
bool is_valid_motor_status(byte value)
{
    return value <= BACKWARD_RIGHT;
}

bool is_valid_arm_status(byte value)
{
    return value <= Clamp;
}

// ==================================================
// ประมวลผลข้อความ CAN
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData)
{
    if (dataLength < 1) return;

    byte receivedStatus = rxData[0];

    // ---------------- คำสั่งควบคุมรถ ----------------
    if (receivedId == CAN_ID_MOTOR)
    {
        if (!is_valid_motor_status(receivedStatus))
        {
            motor_stop();
            return;
        }

        lastMotorMessageTime = millis();
        motorCANAlive = true;

        if (receivedStatus != lastMotorStatus)
        {
            lastMotorStatus = receivedStatus;
            apply_motor_from_status((PS2_Status)receivedStatus);
        }
    }
    // ---------------- คำสั่งควบคุมแขนกล ----------------
    else if (receivedId == CAN_ID_ARM)
    {
        if (!is_valid_arm_status(receivedStatus)) return;

        lastArmMessageTime = millis();
        armCANAlive = true;
        
        // สำหรับแขนกล เราแค่อัปเดตสถานะล่าสุดไว้ 
        // แล้วปล่อยให้ลูปใน loop() ดึงค่าไปจัดการขยับ Servo อย่างต่อเนื่อง
        lastArmStatus = receivedStatus; 
    }
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    Serial.begin(115200);

    // --- เริ่มต้นระบบมอเตอร์ ---
    motor_init();
    
    // --- เริ่มต้นระบบแขนกล (PCA9685) ---
    pca.begin();
    pca.setPWMFreq(50.0);
    pca.setPWM(0, 0, servo0_pwm); // สั่ง Servo 0 ไปท่ากึ่งกลาง
    pca.setPWM(1, 0, servo1_pwm); // สั่ง Servo 1 ไปท่ากึ่งกลาง
    pca.setPWM(2, 0, servo2_pwm); // สั่ง Servo 2 ไปท่ากึ่งกลาง

    pinMode(CAN_INT_PIN, INPUT);

    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);
    
    Serial.println("MCP2515 initialized. CAN receiver ready");

    motor_stop();
}

// ==================================================
// Main loop
// ==================================================
void loop()
{
    // 1. รับข้อมูลจาก CAN Bus แบบ Polling (ถามหาข้อมูลตลอดเวลา)
    if (CAN0.checkReceive() == CAN_MSGAVAIL)
    {
        unsigned long receivedId;
        byte dataLength;
        byte rxData[8];

        byte result = CAN0.readMsgBuf(&receivedId, &dataLength, rxData);
        
        if (result == CAN_OK) 
        {
            // ---------- เริ่มส่วน Debug ข้อความ CAN ----------
            Serial.print("CAN RX | ID: 0x");
            Serial.print(receivedId, HEX); // ปริ้นท์ ID เป็นฐาน 16 (เช่น 100 หรือ 101)
            Serial.print(" | Len: ");
            Serial.print(dataLength);
            Serial.print(" | Status Data: ");
            if (dataLength > 0) {
                Serial.print(rxData[0]); // ปริ้นท์ค่า Status ที่รับมาได้
            }
            Serial.println();
            // ---------- จบส่วน Debug ----------

            // นำข้อมูลไปสั่งมอเตอร์/แขนกลต่อ
            process_can_message(receivedId, dataLength, rxData);
        }
    }

    unsigned long currentTime = millis();

    // 2. ขยับแขนกลอย่างต่อเนื่องตามสถานะที่ได้รับมา
    if (currentTime - last_servo_update_ms >= SERVO_UPDATE_INTERVAL) 
    {
        last_servo_update_ms = currentTime;
        bool servo_changed = false;

        // อัปเดต Servo 0 (ฐานหมุนซ้าย-ขวา)
        if (lastArmStatus == LEFT || lastArmStatus == FORWARD_LEFT || lastArmStatus == BACKWARD_LEFT) {
            servo0_pwm -= 2; servo_changed = true;
        } 
        else if (lastArmStatus == RIGHT || lastArmStatus == FORWARD_RIGHT || lastArmStatus == BACKWARD_RIGHT) {
            servo0_pwm += 2; servo_changed = true;
        }

        // อัปเดต Servo 1 (แขนยกขึ้น-ลง)
        if (lastArmStatus == FORWARD) {
            servo1_pwm -= 2; servo_changed = true;
        } 
        else if (lastArmStatus == BACKWARD) {
            servo1_pwm += 2; servo_changed = true;
        }

        // อัปเดต Servo 2 (Gripper หนีบ-ปล่อย)
        if (lastArmStatus == Clamp) {
            servo2_pwm -= 2; servo_changed = true; 
        } 
        else if (lastArmStatus == Release) {
            servo2_pwm += 2; servo_changed = true;
        }

        // ส่งคำสั่งไปที่บอร์ดเฉพาะตอนที่มีการเปลี่ยนค่าองศา
        if (servo_changed) 
        {
            // จำกัดขอบเขตองศาให้อยู่ในช่วงปลอดภัย ป้องกันมอเตอร์ไหม้
            servo0_pwm = constrain(servo0_pwm, 150, 450);
            servo1_pwm = constrain(servo1_pwm, 150, 450);
            servo2_pwm = constrain(servo2_pwm, 150, 450);

            pca.setPWM(0, 0, servo0_pwm);
            pca.setPWM(1, 0, servo1_pwm);
            pca.setPWM(2, 0, servo2_pwm);
        }
    }

    // ---------------- Fail-safe ระบบความปลอดภัย ----------------
    if (motorCANAlive && (currentTime - lastMotorMessageTime > CAN_TIMEOUT))
    {
        motorCANAlive = false;
        lastMotorStatus = -1;
        motor_stop();
        Serial.println("WARNING: Motor CAN timeout - Force Stopped");
    }

    if (armCANAlive && (currentTime - lastArmMessageTime > CAN_TIMEOUT))
    {
        armCANAlive = false;
        lastArmStatus = -1; // ถ้าสายหลุด แขนกลจะหยุดขยับอัตโนมัติ
        Serial.println("WARNING: Arm CAN timeout");
    }
}