#include <SPI.h>
#include <mcp_can.h>
#include "motor.h"
#include "robot_config.h"

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2; 

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_TIMEOUT = 300; // ms

MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- สถานะการควบคุมมอเตอร์ ----------------
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
    SPIN_LEFT = 11,
    SPIN_RIGHT = 12
};

unsigned long lastMotorMessageTime = 0;
bool motorCANAlive = false;
int lastMotorStatus = -1;

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
    case SPIN_LEFT: motor_spin_left(); break;
    case SPIN_RIGHT: motor_spin_right(); break;
    case STOP:
    default: motor_stop(); break;
    }
}

// ==================================================
// ตรวจสอบความถูกต้องของข้อมูล Motor Status (ป้องกันค่าขยะ)
// ==================================================
bool is_valid_motor_status(byte value)
{
    return (value <= BACKWARD_RIGHT) || (value == SPIN_LEFT) || (value == SPIN_RIGHT);
}

// ==================================================
// ประมวลผลข้อความ CAN (รับเฉพาะ CAN_ID_MOTOR สำหรับขับล้อ)
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData)
{
    if (dataLength < 1) return;

    // จัดการเฉพาะคำสั่งควบคุมมอเตอร์ขับเคลื่อนล้อ
    if (receivedId == CAN_ID_MOTOR)
    {
        byte receivedStatus = rxData[0];

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
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    Serial.begin(115200);

    // --- เริ่มต้นระบบมอเตอร์ขับเคลื่อนล้อ ---
    motor_init();

    pinMode(CAN_INT_PIN, INPUT);

    Serial.println("Initializing MCP2515 (Motor Controller)...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    Serial.println("MCP2515 initialized. Motor Controller CAN receiver ready");

    motor_stop();
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

    // 2. Fail-safe ระบบความปลอดภัยสำหรับมอเตอร์
    // หากไม่ได้รับสัญญาณสั่งงานเกินเวลา CAN_TIMEOUT (300 ms) ให้หยุดมอเตอร์อัตโนมัติ
    if (motorCANAlive && (currentTime - lastMotorMessageTime > CAN_TIMEOUT))
    {
        motorCANAlive = false;
        lastMotorStatus = -1;
        motor_stop();
        Serial.println("WARNING: Motor CAN timeout - Force Stopped");
    }
}
