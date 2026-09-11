#include <SPI.h>
#include <mcp_can.h>

// ---------------- CAN configuration ----------------

const byte CAN_CS_PIN = 10;

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;

const unsigned long CAN_SEND_INTERVAL = 50;
const unsigned long TEST_INTERVAL = 2000;

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

// สถานะที่กำลังจะส่ง
PS2_Status currentMotorStatus = STOP;
PS2_Status currentArmStatus = STOP;

unsigned long lastSendTime = 0;
unsigned long lastTestChangeTime = 0;

byte testStep = 0;

// --------------------------------------------------
// ส่ง Status จำนวน 1 byte
// --------------------------------------------------

bool send_can_status(
    unsigned long canId,
    PS2_Status status
)
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
// เปลี่ยนสถานะอัตโนมัติเพื่อทดสอบ
// --------------------------------------------------

void update_test_status()
{
    switch (testStep)
    {
    case 0:
        currentMotorStatus = FORWARD;
        currentArmStatus = STOP;
        Serial.println("TEST: Motor forward");
        break;

    case 1:
        currentMotorStatus = LEFT;
        currentArmStatus = STOP;
        Serial.println("TEST: Motor slide left");
        break;

    case 2:
        currentMotorStatus = RIGHT;
        currentArmStatus = STOP;
        Serial.println("TEST: Motor slide right");
        break;

    case 3:
        currentMotorStatus = STOP;
        currentArmStatus = FORWARD;
        Serial.println("TEST: Arm forward");
        break;

    case 4:
        currentMotorStatus = STOP;
        currentArmStatus = LEFT;
        Serial.println("TEST: Arm turn left");
        break;

    case 5:
        currentMotorStatus = STOP;
        currentArmStatus = Release;
        Serial.println("TEST: Gripper release");
        break;

    case 6:
        currentMotorStatus = STOP;
        currentArmStatus = Clamp;
        Serial.println("TEST: Gripper clamp");
        break;

    default:
        currentMotorStatus = STOP;
        currentArmStatus = STOP;
        Serial.println("TEST: Stop everything");
        break;
    }

    testStep++;

    if (testStep > 7)
    {
        testStep = 0;
    }
}

// --------------------------------------------------
// Setup
// --------------------------------------------------

void setup()
{
    Serial.begin(115200);

    Serial.println("Initializing MCP2515...");

    while (
        CAN0.begin(
            MCP_ANY,
            CAN_500KBPS,
            MCP_8MHZ
        ) != CAN_OK
    )
    {
        Serial.println("MCP2515 initialization failed");
        Serial.println("Retrying...");
        delay(1000);
    }

    CAN0.setMode(MCP_NORMAL);

    Serial.println("MCP2515 initialized");
    Serial.println("CAN transmitter ready");

    currentMotorStatus = STOP;
    currentArmStatus = STOP;

    lastSendTime = millis();
    lastTestChangeTime = millis();
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------

void loop()
{
    unsigned long currentTime = millis();

    // เปลี่ยนคำสั่งทดสอบทุก 2 วินาที
    if (currentTime - lastTestChangeTime >= TEST_INTERVAL)
    {
        lastTestChangeTime = currentTime;
        update_test_status();
    }

    // ส่งสถานะซ้ำทุก 50 ms
    if (currentTime - lastSendTime >= CAN_SEND_INTERVAL)
    {
        lastSendTime = currentTime;

        bool motorResult = send_can_status(
            CAN_ID_MOTOR,
            currentMotorStatus
        );

        bool armResult = send_can_status(
            CAN_ID_ARM,
            currentArmStatus
        );

        if (!motorResult)
        {
            Serial.println("Motor status send failed");
        }

        if (!armResult)
        {
            Serial.println("Arm status send failed");
        }
    }
}