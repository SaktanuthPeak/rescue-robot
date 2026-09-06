#include <SPI.h>
#include <mcp_can.h>

// ---------------- CAN configuration ----------------

const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2;

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;

const unsigned long CAN_TIMEOUT = 300;

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

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;

bool motorCANAlive = false;
bool armCANAlive = false;

int lastMotorStatus = -1;
int lastArmStatus = -1;

// ==================================================
// ฟังก์ชันควบคุมมอเตอร์รถ
// ตอนนี้ใช้ Serial สำหรับทดสอบ
// ให้นำโค้ดควบคุมมอเตอร์จริงมาใส่แทน
// ==================================================

void motor_forward()
{
    Serial.println("MOTOR: Forward");
}

void motor_backward()
{
    Serial.println("MOTOR: Backward");
}

void motor_slide_left()
{
    Serial.println("MOTOR: Slide left");
}

void motor_slide_right()
{
    Serial.println("MOTOR: Slide right");
}

void motor_forward_left()
{
    Serial.println("MOTOR: Forward left");
}

void motor_forward_right()
{
    Serial.println("MOTOR: Forward right");
}

void motor_backward_left()
{
    Serial.println("MOTOR: Backward left");
}

void motor_backward_right()
{
    Serial.println("MOTOR: Backward right");
}

void motor_stop()
{
    Serial.println("MOTOR: Stop");
}

// ==================================================
// ฟังก์ชันควบคุมแขน
// ตอนนี้ใช้ Serial สำหรับทดสอบ
// ให้นำโค้ดควบคุมแขนจริงมาใส่แทน
// ==================================================

void arm_forward()
{
    Serial.println("ARM: Forward");
}

void arm_backward()
{
    Serial.println("ARM: Backward");
}

void arm_turn_left()
{
    Serial.println("ARM: Turn left");
}

void arm_turn_right()
{
    Serial.println("ARM: Turn right");
}

void arm_stop()
{
    Serial.println("ARM: Stop");
}

void gripper_release()
{
    Serial.println("GRIPPER: Release");
}

void gripper_clamp()
{
    Serial.println("GRIPPER: Clamp");
}

// ==================================================
// นำ Status ไปควบคุมมอเตอร์
// ==================================================

void apply_motor_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD:
        motor_forward();
        break;

    case BACKWARD:
        motor_backward();
        break;

    case LEFT:
        motor_slide_left();
        break;

    case RIGHT:
        motor_slide_right();
        break;

    case FORWARD_LEFT:
        motor_forward_left();
        break;

    case FORWARD_RIGHT:
        motor_forward_right();
        break;

    case BACKWARD_LEFT:
        motor_backward_left();
        break;

    case BACKWARD_RIGHT:
        motor_backward_right();
        break;

    case STOP:
    default:
        motor_stop();
        break;
    }
}

// ==================================================
// นำ Status ไปควบคุมแขน
// ==================================================

void apply_arm_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD:
        arm_forward();
        break;

    case BACKWARD:
        arm_backward();
        break;

    case LEFT:
    case FORWARD_LEFT:
    case BACKWARD_LEFT:
        arm_turn_left();
        break;

    case RIGHT:
    case FORWARD_RIGHT:
    case BACKWARD_RIGHT:
        arm_turn_right();
        break;

    case Release:
        gripper_release();
        break;

    case Clamp:
        gripper_clamp();
        break;

    case STOP:
    default:
        arm_stop();
        break;
    }
}

// ==================================================
// ตรวจสอบค่า Status
// ==================================================

bool is_valid_motor_status(byte value)
{
    // มอเตอร์รับค่าตั้งแต่ STOP ถึง BACKWARD_RIGHT
    return value <= BACKWARD_RIGHT;
}

bool is_valid_arm_status(byte value)
{
    // แขนรับค่าตั้งแต่ STOP ถึง Clamp
    return value <= Clamp;
}

// ==================================================
// ประมวลผลข้อความ CAN
// ==================================================

void process_can_message(
    unsigned long receivedId,
    byte dataLength,
    byte *rxData
)
{
    if (dataLength < 1)
    {
        return;
    }

    byte receivedStatus = rxData[0];

    // ---------------- Motor command ----------------

    if (receivedId == CAN_ID_MOTOR)
    {
        if (!is_valid_motor_status(receivedStatus))
        {
            Serial.println("Invalid motor status");
            motor_stop();
            return;
        }

        lastMotorMessageTime = millis();
        motorCANAlive = true;

        // เรียกฟังก์ชันเมื่อสถานะเปลี่ยนเท่านั้น
        if (receivedStatus != lastMotorStatus)
        {
            lastMotorStatus = receivedStatus;

            apply_motor_from_status(
                (PS2_Status)receivedStatus
            );
        }
    }

    // ---------------- Arm command ----------------

    else if (receivedId == CAN_ID_ARM)
    {
        if (!is_valid_arm_status(receivedStatus))
        {
            Serial.println("Invalid arm status");
            arm_stop();
            return;
        }

        lastArmMessageTime = millis();
        armCANAlive = true;

        // เรียกฟังก์ชันเมื่อสถานะเปลี่ยนเท่านั้น
        if (receivedStatus != lastArmStatus)
        {
            lastArmStatus = receivedStatus;

            apply_arm_from_status(
                (PS2_Status)receivedStatus
            );
        }
    }
}

// ==================================================
// Setup
// ==================================================

void setup()
{
    Serial.begin(115200);

    pinMode(CAN_INT_PIN, INPUT);

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
    Serial.println("CAN receiver ready");

    motor_stop();
    arm_stop();
}

// ==================================================
// Main loop
// ==================================================

void loop()
{
    // ขา INT เป็น LOW เมื่อมีข้อความ CAN เข้ามา
    while (digitalRead(CAN_INT_PIN) == LOW)
    {
        unsigned long receivedId;
        byte dataLength;
        byte rxData[8];

        byte result = CAN0.readMsgBuf(
            &receivedId,
            &dataLength,
            rxData
        );

        if (result != CAN_OK)
        {
            break;
        }

        process_can_message(
            receivedId,
            dataLength,
            rxData
        );
    }

    unsigned long currentTime = millis();

    // ---------------- Motor fail-safe ----------------

    if (
        motorCANAlive &&
        currentTime - lastMotorMessageTime > CAN_TIMEOUT
    )
    {
        motorCANAlive = false;
        lastMotorStatus = -1;

        motor_stop();

        Serial.println("WARNING: Motor CAN timeout");
    }

    // ---------------- Arm fail-safe ----------------

    if (
        armCANAlive &&
        currentTime - lastArmMessageTime > CAN_TIMEOUT
    )
    {
        armCANAlive = false;
        lastArmStatus = -1;

        arm_stop();

        Serial.println("WARNING: Arm CAN timeout");
    }
}