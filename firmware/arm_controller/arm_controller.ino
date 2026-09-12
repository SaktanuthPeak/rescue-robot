#include <SPI.h>
#include <mcp_can.h>
#include "PCA9685_Control.h" // นำเข้าไลบรารีคุม Servo
#include "robot_config.h"

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2; 

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;
const unsigned long CAN_TIMEOUT  = 300; 

MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- Relay configuration (ปั๊มน้ำ) ----------------
// กำหนดขา Relay สำหรับเปิด/ปิด ปั๊มน้ำ (ค่าเริ่มต้นใช้ขา 4 ตาม robot_config.h)
#ifndef RELAY_PUMP_PIN
#define RELAY_PUMP_PIN 4
#endif

// โมดูล Relay ทั่วไปเป็นแบบ Active-LOW (LOW = Relay ON, HIGH = Relay OFF)
// หากใช้โมดูล Active-HIGH ให้สลับค่า LOW กับ HIGH
const byte RELAY_ON_STATE  = LOW;  // สถานะสั่งเปิด Relay
const byte RELAY_OFF_STATE = HIGH; // สถานะสั่งปิด Relay
bool pumpState = false;

// ---------------- PCA9685 configuration ----------------
PCA9685Control pca;

// ตัวแปรจดจำตำแหน่งปัจจุบันของ Servo
int servo0_pwm = 305; // ช่อง 0 (PCA ตัวที่ 1): ฐานหมุน ซ้าย-ขวา (LEFT=3, RIGHT=4)
int servo1_pwm = 305; // ช่อง 1 (PCA ตัวที่ 2): แขนยก ขึ้น-ลง (FORWARD=1, BACKWARD=2)
int servo2_pwm = 305; // ช่อง 2 (PCA ตัวที่ 3): หัวยก ขึ้น-ลง (Head_Up=13, Head_Down=14)

unsigned long last_servo_update_ms = 0;
const unsigned long SERVO_UPDATE_INTERVAL = 15; // ความเร็วในการขยับแขนกล (ค่าน้อย = ขยับไว)

// ---------------- Status definition (ตรงกับ sender CAN bus) ----------------
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
    SPIN_LEFT = 9,
    SPIN_RIGHT = 10,
    Pump_On = 11,
    Pump_Off = 12,
    Head_Up = 13,
    Head_Down = 14
};

unsigned long lastArmMessageTime = 0;
bool armCANAlive = false;
int lastArmStatus = -1;

// ==================================================
// ตรวจสอบความถูกต้องของข้อมูล (ป้องกันค่าขยะ)
// ==================================================
bool is_valid_arm_status(byte value)
{
    return (value == STOP) ||
           (value >= FORWARD && value <= BACKWARD_RIGHT) ||
           (value == Pump_On || value == Pump_Off) ||
           (value == Head_Up || value == Head_Down);
}

// ==================================================
// ประมวลผลข้อความ CAN
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData)
{
    if (dataLength < 1) return;

    byte receivedStatus = rxData[0];

    // ---------------- คำสั่งควบคุมแขนกลและปั๊มน้ำ ----------------
    if (receivedId == CAN_ID_ARM)
    {
        if (!is_valid_arm_status(receivedStatus)) return;

        lastArmMessageTime = millis();
        armCANAlive = true;
        
        // ควบคุม Relay ปั๊มน้ำ
        if (receivedStatus == Pump_On)
        {
            digitalWrite(RELAY_PUMP_PIN, RELAY_ON_STATE);
            pumpState = true;
        }
        else if (receivedStatus == Pump_Off)
        {
            digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE);
            pumpState = false;
        }

        // อัปเดตสถานะล่าสุดเพื่อให้ loop() ขยับ Servo อย่างต่อเนื่อง
        lastArmStatus = receivedStatus; 
    }
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    Serial.begin(115200);

    // --- เริ่มต้นระบบ Relay ควบคุมปั๊มน้ำ ---
    pinMode(RELAY_PUMP_PIN, OUTPUT);
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE); // ปิดปั๊มน้ำเริ่มต้น

    // --- เริ่มต้นระบบแขนกล (PCA9685) ---
    pca.begin();
    pca.setPWMFreq(50.0);
    pca.setPWM(0, 0, servo0_pwm); // PCA ช่อง 0 (ตัวที่ 1): ฐานหมุนกึ่งกลาง
    pca.setPWM(1, 0, servo1_pwm); // PCA ช่อง 1 (ตัวที่ 2): แขนยกกึ่งกลาง
    pca.setPWM(2, 0, servo2_pwm); // PCA ช่อง 2 (ตัวที่ 3): หัวยกกึ่งกลาง

    pinMode(CAN_INT_PIN, INPUT);

    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);
    
    Serial.println("MCP2515 initialized. Arm CAN receiver ready");
}

// ==================================================
// Main loop
// ==================================================
void loop()
{
    // 1. รับข้อมูลจาก CAN Bus แบบ Polling
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
            Serial.print(receivedId, HEX);
            Serial.print(" | Len: ");
            Serial.print(dataLength);
            Serial.print(" | Status Data: ");
            if (dataLength > 0) {
                Serial.print(rxData[0]);
                Serial.print(" (");
                switch (rxData[0])
                {
                    case FORWARD:   Serial.print("FORWARD"); break;
                    case BACKWARD:  Serial.print("BACKWARD"); break;
                    case LEFT:      Serial.print("LEFT"); break;
                    case RIGHT:     Serial.print("RIGHT"); break;
                    case Pump_On:   Serial.print("Pump_On"); break;
                    case Pump_Off:  Serial.print("Pump_Off"); break;
                    case Head_Up:   Serial.print("Head_Up"); break;
                    case Head_Down: Serial.print("Head_Down"); break;
                    case STOP:      Serial.print("STOP"); break;
                    default:        Serial.print("OTHER"); break;
                }
                Serial.print(")");
            }
            Serial.print(" | Pump: ");
            Serial.println(pumpState ? "ON" : "OFF");
            // ---------- จบส่วน Debug ----------

            process_can_message(receivedId, dataLength, rxData);
        }
    }

    unsigned long currentTime = millis();

    // 2. ขยับแขนกลอย่างต่อเนื่องตามสถานะที่ได้รับมา
    if (currentTime - last_servo_update_ms >= SERVO_UPDATE_INTERVAL) 
    {
        last_servo_update_ms = currentTime;
        bool servo_changed = false;

        // อัปเดต Servo 0 (PCA ตัวที่ 1: ฐานหมุนซ้าย-ขวา ด้วย LEFT=3 / RIGHT=4)
        if (lastArmStatus == LEFT || lastArmStatus == FORWARD_LEFT || lastArmStatus == BACKWARD_LEFT) {
            servo0_pwm -= 2; servo_changed = true;
        } 
        else if (lastArmStatus == RIGHT || lastArmStatus == FORWARD_RIGHT || lastArmStatus == BACKWARD_RIGHT) {
            servo0_pwm += 2; servo_changed = true;
        }

        // อัปเดต Servo 1 (PCA ตัวที่ 2: แขนยกขึ้น-ลง ด้วย FORWARD=1 / BACKWARD=2)
        if (lastArmStatus == FORWARD) {
            servo1_pwm -= 2; servo_changed = true;
        } 
        else if (lastArmStatus == BACKWARD) {
            servo1_pwm += 2; servo_changed = true;
        }

        // อัปเดต Servo 2 (PCA ตัวที่ 3: หัวยกขึ้น-ลง ด้วย Head_Up=13 / Head_Down=14)
        if (lastArmStatus == Head_Up) {
            servo2_pwm -= 2; servo_changed = true; 
        } 
        else if (lastArmStatus == Head_Down) {
            servo2_pwm += 2; servo_changed = true;
        }

        // ส่งคำสั่งไปที่บอร์ด PCA9685 เฉพาะตอนที่มีการเปลี่ยนค่าองศา
        if (servo_changed) 
        {
            // จำกัดขอบเขตองศาให้อยู่ในช่วงปลอดภัย ป้องกันเซอร์โวติดขัด
            servo0_pwm = constrain(servo0_pwm, 150, 450);
            servo1_pwm = constrain(servo1_pwm, 150, 450);
            servo2_pwm = constrain(servo2_pwm, 150, 450);

            pca.setPWM(0, 0, servo0_pwm); // PCA ตัวที่ 1
            pca.setPWM(1, 0, servo1_pwm); // PCA ตัวที่ 2
            pca.setPWM(2, 0, servo2_pwm); // PCA ตัวที่ 3
        }
    }

    // ---------------- Fail-safe ระบบความปลอดภัย ----------------
    if (armCANAlive && (currentTime - lastArmMessageTime > CAN_TIMEOUT))
    {
        armCANAlive = false;
        lastArmStatus = -1; // ถ้าสัญญาณขาดหาย แขนกลจะหยุดขยับ
        digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE); // ปิดปั๊มน้ำเพื่อความปลอดภัย
        pumpState = false;
        Serial.println("WARNING: Arm CAN timeout - Pump stopped");
    }
}