#include <SPI.h>
#include <mcp_can.h>
#include "robot_config.h"
#include "PS2_Controller.h"

// ---------------- CAN configuration ----------------
MCP_CAN CAN0(CAN_CS_PIN);

// ตัวจับเวลาสำหรับ Tasks ต่างๆ
unsigned long lastSendTime = 0;
unsigned long lastPollTime = 0;
unsigned long lastDebugTime = 0;

PS2_Status currentMotorStatus = STOP;
PS2_Status currentArmStatus   = STOP;

// --------------------------------------------------
// ฟังก์ชันส่ง Status ผ่าน CAN Bus (1 Byte Payload)
// --------------------------------------------------
bool send_can_status(unsigned long canId, PS2_Status status)
{
    byte txData[1];
    txData[0] = (byte)status;

    byte result = CAN0.sendMsgBuf(
        canId,
        0,              // 0 = Standard 11-bit CAN ID
        1,              // ขนาดข้อมูล 1 byte
        txData
    );

    return result == CAN_OK;
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Serial.println("==================================================");
    Serial.println("      FireBot / DurianBot CAN-Sender Node         ");
    Serial.println("==================================================");

    // 1. เริ่มต้นระบบ MCP2515 CAN Bus
    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying in 1s...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);
    Serial.println("MCP2515 Initialized: CAN transmitter ready.");

    // 2. เริ่มต้นเชื่อมต่อจอยรีโมท PS2
    Serial.println("Initializing PS2 Gamepad Controller...");
    PS2_Init();

    currentMotorStatus = STOP;
    currentArmStatus   = STOP;
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------
void loop()
{
    unsigned long currentTime = millis();

    // 1. อ่านและประมวลผลคำสั่งจากจอย PS2 ทุก 15 ms (~66 Hz)
    if (currentTime - lastPollTime >= PS2_POLL_INTERVAL_MS)
    {
        lastPollTime = currentTime;

        PS2_Update();
        currentMotorStatus = PS2_GetMotorStatus();
        currentArmStatus   = PS2_GetArmStatus();
    }

    // 2. ส่งสถานะคำสั่งไปยัง CAN Bus ทุก 50 ms (20 Hz Heartbeat)
    // ส่งเป็นจังหวะต่อเนื่อง เพื่อเลี้ยง Watchdog Timeout (300ms) ของบอร์ดรับ
    if (currentTime - lastSendTime >= CAN_SEND_INTERVAL_MS)
    {
        lastSendTime = currentTime;

        bool motorResult = send_can_status(CAN_ID_MOTOR, currentMotorStatus);
        bool armResult   = send_can_status(CAN_ID_ARM, currentArmStatus);

        if (!motorResult)
        {
            Serial.println("WARNING: Motor CAN message send failed");
        }
        if (!armResult)
        {
            Serial.println("WARNING: Arm CAN message send failed");
        }
    }

    // 3. พิมพ์ข้อความตรวจสอบสถานะทาง Serial Monitor ทุก 250 ms
    if (currentTime - lastDebugTime >= DEBUG_PRINT_INTERVAL_MS)
    {
        lastDebugTime = currentTime;
        print_debug(currentMotorStatus, currentArmStatus);
    }
}
