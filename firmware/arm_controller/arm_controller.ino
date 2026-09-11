#include <SPI.h>
#include <mcp_can.h>
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
    Head_Up = 9,
    Head_Down = 10,
    PUMP_ON = 11,
    PUMP_OFF = 12,
    FLASHLIGHT_ON = 13,
    FLASHLIGHT_OFF = 14
};

unsigned long lastArmMessageTime = 0;

bool armCANAlive = false;

int lastArmStatus = -1;

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
}

// ==================================================
// Main loop
// ==================================================
void loop()
{
    // 1. รับข้อมูลจาก CAN Bus
    while (digitalRead(CAN_INT_PIN) == LOW)
    {
        unsigned long receivedId;
        byte dataLength;
        byte rxData[8];

        byte result = CAN0.readMsgBuf(&receivedId, &dataLength, rxData);
        if (result != CAN_OK) break;

        process_can_message(receivedId, dataLength, rxData);
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
        if (lastArmStatus == Head_Up) {
            servo2_pwm -= 2; servo_changed = true; 
        } 
        else if (lastArmStatus == Head_Down) {
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
    if (armCANAlive && (currentTime - lastArmMessageTime > CAN_TIMEOUT))
    {
        armCANAlive = false;
        lastArmStatus = -1; // ถ้าสายหลุด แขนกลจะหยุดขยับอัตโนมัติ
        Serial.println("WARNING: Arm CAN timeout");
    }
}