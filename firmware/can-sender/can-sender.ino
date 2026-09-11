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
unsigned long lastErrorWarnTime = 0;

PS2_Status currentMotorStatus = STOP;
PS2_Status currentArmStatus   = STOP;

// --------------------------------------------------
// ฟังก์ชันส่ง Status ผ่าน CAN Bus (1 Byte Payload)
// คืนค่า Return Code จาก MCP_CAN (0 = CAN_OK)
// --------------------------------------------------
byte send_can_status(unsigned long canId, PS2_Status status)
{
    byte txData[1];
    txData[0] = (byte)status;

    byte result = CAN0.sendMsgBuf(
        canId,
        0,              // 0 = Standard 11-bit CAN ID
        1,              // ขนาดข้อมูล 1 byte
        txData
    );

    return result;
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup()
{
    Serial.begin(115200);

    Serial.println(F("=================================================="));
    Serial.println(F("      FireBot / DurianBot CAN-Sender Node         "));
    Serial.println(F("=================================================="));

    // 1. เริ่มต้นระบบ MCP2515 CAN Bus
    Serial.print(F("Initializing MCP2515 (Clock: "));
#if CAN_CLOCK_SET == MCP_16MHZ
    Serial.println(F("16 MHz)..."));
#else
    Serial.println(F("8 MHz)..."));
#endif

    while (CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK_SET) != CAN_OK)
    {
        Serial.println(F("MCP2515 initialization failed. Retrying in 1s..."));
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    // เปิด One-Shot Mode เพื่อไม่ให้ตัวส่งค้างส่งซ้ำไม่รู้จบเมื่อไม่มีบอร์ดรับ ACK
    CAN0.enOneShotTX();

    Serial.println(F("MCP2515 Initialized: CAN transmitter ready."));

    // 2. เริ่มต้นเชื่อมต่อจอยรีโมท PS2
    Serial.println(F("Initializing PS2 Gamepad Controller..."));
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
    if (currentTime - lastSendTime >= CAN_SEND_INTERVAL_MS)
    {
        lastSendTime = currentTime;

        byte motorResult = send_can_status(CAN_ID_MOTOR, currentMotorStatus);
        byte armResult   = send_can_status(CAN_ID_ARM, currentArmStatus);

        // ตรวจสอบความผิดพลาดในการส่ง CAN
        if (motorResult != CAN_OK || armResult != CAN_OK)
        {
            // ล้าง Buffer ที่ค้างเพื่อไม่ให้บัฟเฟอร์เต็มถาวร
            CAN0.abortTX();

            // แจ้งเตือนแบบสรุปทุก 1 วินาที (ไม่พิมพ์รัวจนบอร์ดค้าง)
            if (currentTime - lastErrorWarnTime >= 1000)
            {
                lastErrorWarnTime = currentTime;
                byte tec = CAN0.errorCountTX();

                Serial.println(F("---------------------------------------------------------"));
                Serial.print(F("[CAN TRANSMIT ERROR] Code: "));
                Serial.print(motorResult != CAN_OK ? motorResult : armResult);

                if (motorResult == 7 || armResult == 7) {
                    Serial.println(F(" (TIMEOUT / NO ACK - ไม่มีบอร์ดรับบนสาย)"));
                } else if (motorResult == 6 || armResult == 6) {
                    Serial.println(F(" (BUFFER FULL - บัฟเฟอร์ส่งเต็ม ค้างรอส่ง)"));
                } else {
                    Serial.println();
                }

                Serial.print(F("  -> Transmit Error Count (TEC): "));
                Serial.println(tec);
                Serial.println(F("  -> สาเหตุที่พบบ่อย:"));
                Serial.println(F("     1. บอร์ดรับ (motor_controller) ยังไม่ได้เปิด หรือยังไม่ได้รันโค้ด"));
                Serial.println(F("     2. สาย CAN_H / CAN_L สลับขั้ว หรือไม่ได้ต่อสาย GND ระหว่างบอร์ด"));
                Serial.println(F("     3. ความถี่ Crystal ไม่ตรง (ตรวจดูตัวถังแร่สีเงินว่าเขียน 8.000 หรือ 16.000)"));
                Serial.println(F("     4. ลืมเสียบ Jumper 120R (J1) ที่ปลายสาย CAN Bus"));
                Serial.println(F("---------------------------------------------------------"));
            }
        }
    }

    // 3. พิมพ์ข้อความตรวจสอบสถานะจอยทาง Serial Monitor ทุก 250 ms
    if (currentTime - lastDebugTime >= DEBUG_PRINT_INTERVAL_MS)
    {
        lastDebugTime = currentTime;
        print_debug(currentMotorStatus, currentArmStatus);
    }
}
