#include <SPI.h>
#include <mcp_can.h>
#include "PS2X_lib.h" // นำเข้าไลบรารี PS2

// ---------------- PS2 configuration ----------------
#define PS2_DAT_PIN A0
#define PS2_CMD_PIN A1
#define PS2_ATT_PIN A2
#define PS2_CLK_PIN A3

PS2X ps2x;
const uint8_t PS2_DEADZONE = 12;
const unsigned long PS2_POLL_INTERVAL_MS = 15;
unsigned long last_ps2_poll_ms = 0;

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;
const unsigned long CAN_SEND_INTERVAL = 50;

MCP_CAN CAN0(CAN_CS_PIN);

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

// สถานะที่จะส่งขึ้น CAN
PS2_Status currentMotorStatus = STOP;
PS2_Status currentArmStatus = STOP;

unsigned long lastSendTime = 0;

// --------------------------------------------------
// ฟังก์ชันแปลงค่าแกน X, Y เป็นสถานะทิศทาง
// --------------------------------------------------
PS2_Status get_status_from_sticks(int x, int y, PS2_Status center_value)
{
    if (x == 0 && y == 1) return BACKWARD;
    else if (x == 0 && y == -1) return FORWARD;
    else if (x == -1 && y == 0) return LEFT;
    else if (x == 1 && y == 0) return RIGHT;
    else if (x == -1 && y == 1) return BACKWARD_LEFT;
    else if (x == 1 && y == 1) return BACKWARD_RIGHT;
    else if (x == -1 && y == -1) return FORWARD_LEFT;
    else if (x == 1 && y == -1) return FORWARD_RIGHT;
    else return center_value;
}

// --------------------------------------------------
// ส่ง Status จำนวน 1 byte ขึ้น CAN
// --------------------------------------------------
bool send_can_status(unsigned long canId, PS2_Status status)
{
    byte txData[1];
    txData[0] = (byte)status;

    byte result = CAN0.sendMsgBuf(
        canId,
        0,              // 0 = Standard CAN ID
        1,              // ข้อมูล 1 byte
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

    // --- ตั้งค่า PS2 ---
    delay(300); // หน่วงเวลาให้โมดูล PS2 บูต
    ps2x.config_gamepad(PS2_CLK_PIN, PS2_CMD_PIN, PS2_ATT_PIN, PS2_DAT_PIN, false, false);
    Serial.println("PS2 Initialized");

    // --- ตั้งค่า CAN ---
    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);
    Serial.println("MCP2515 initialized. CAN transmitter ready");

    currentMotorStatus = STOP;
    currentArmStatus = STOP;
    
    lastSendTime = millis();
    last_ps2_poll_ms = millis();
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------
void loop()
{
    unsigned long currentTime = millis();

    // 1. อ่านค่าจากจอย PS2 ทุกๆ 15 ms
    if (currentTime - last_ps2_poll_ms >= PS2_POLL_INTERVAL_MS)
    {
        last_ps2_poll_ms = currentTime;
        ps2x.read_gamepad();

        // ----------------------------------------------------
        // ควบคุม Motor (ล้อ) ด้วย D-PAD และอนาล็อกซ้าย
        // ----------------------------------------------------
        int x_left = 0;
        int y_left = 0;

        if (ps2x.Button(PSB_PAD_UP)) { y_left = -1; }
        else if (ps2x.Button(PSB_PAD_DOWN)) { y_left = 1; }
        if (ps2x.Button(PSB_PAD_LEFT)) { x_left = -1; }
        else if (ps2x.Button(PSB_PAD_RIGHT)) { x_left = 1; }

        if (x_left == 0 && y_left == 0) 
        {
            uint8_t lx = ps2x.Analog(PSS_LX);
            uint8_t ly = ps2x.Analog(PSS_LY);
            
            if (lx != 255 || ly != 255) 
            {
                if (lx < (128 - PS2_DEADZONE)) x_left = -1;
                else if (lx > (128 + PS2_DEADZONE)) x_left = 1;
                if (ly < (128 - PS2_DEADZONE)) y_left = -1;
                else if (ly > (128 + PS2_DEADZONE)) y_left = 1;
            }
        }
        currentMotorStatus = get_status_from_sticks(x_left, y_left, STOP);

        // ----------------------------------------------------
        // ควบคุม Arm (แขนกล) ด้วยอนาล็อกขวา และ Gripper
        // ----------------------------------------------------
        int x_right = 0;
        int y_right = 0;
        
        uint8_t rx = ps2x.Analog(PSS_RX);
        uint8_t ry = ps2x.Analog(PSS_RY);
        
        if (rx != 255 || ry != 255) 
        {
            if (rx < (128 - PS2_DEADZONE)) x_right = -1;
            else if (rx > (128 + PS2_DEADZONE)) x_right = 1;
            if (ry < (128 - PS2_DEADZONE)) y_right = -1;
            else if (ry > (128 + PS2_DEADZONE)) y_right = 1;
        }
        PS2_Status stick_arm_status = get_status_from_sticks(x_right, y_right, STOP);

        // จัดลำดับความสำคัญ: ถ้ากดปุ่มหนีบ/ปล่อย ให้ส่งค่า Gripper
        // ถ้าไม่ได้กด ให้ส่งค่าทิศทางจากก้านโยกขวา
        if (ps2x.Button(PSB_SQUARE)) {
            currentArmStatus = Clamp;
        } else if (ps2x.Button(PSB_CIRCLE)) {
            currentArmStatus = Release;
        } else {
            currentArmStatus = stick_arm_status;
        }
    }

    if (currentTime - lastSendTime >= CAN_SEND_INTERVAL)
    {
        lastSendTime = currentTime;
    
        // สาดข้อมูลขึ้นสาย CAN 
        send_can_status(CAN_ID_MOTOR, currentMotorStatus);
        send_can_status(CAN_ID_ARM, currentArmStatus);

        // ---------- เปิด Debug ให้แสดงผลข้อความที่ส่งออกไป ----------
        Serial.print("SENDING -> MOTOR: ");
        switch (currentMotorStatus)
        {
            case FORWARD: Serial.print("FORWARD"); break;
            case BACKWARD: Serial.print("BACKWARD"); break;
            case LEFT: Serial.print("LEFT"); break;
            case RIGHT: Serial.print("RIGHT"); break;
            case FORWARD_LEFT: Serial.print("FORWARD_LEFT"); break;
            case FORWARD_RIGHT: Serial.print("FORWARD_RIGHT"); break;
            case BACKWARD_LEFT: Serial.print("BACKWARD_LEFT"); break;
            case BACKWARD_RIGHT: Serial.print("BACKWARD_RIGHT"); break;
            default: Serial.print("STOP"); break;
        }

        Serial.print("  |  ARM: ");
        switch (currentArmStatus)
        {
            case FORWARD: Serial.print("FORWARD"); break;
            case BACKWARD: Serial.print("BACKWARD"); break;
            case LEFT: Serial.print("LEFT"); break;
            case RIGHT: Serial.print("RIGHT"); break;
            case FORWARD_LEFT: Serial.print("FORWARD_LEFT"); break;
            case FORWARD_RIGHT: Serial.print("FORWARD_RIGHT"); break;
            case BACKWARD_LEFT: Serial.print("BACKWARD_LEFT"); break;
            case BACKWARD_RIGHT: Serial.print("BACKWARD_RIGHT"); break;
            case Clamp: Serial.print("CLAMP"); break;
            case Release: Serial.print("RELEASE"); break;
            default: Serial.print("CENTER"); break;
        }
        Serial.println();
    }
}