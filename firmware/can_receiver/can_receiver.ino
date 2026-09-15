#include <SPI.h>
#include <mcp_can.h>

// =====================================================
// CAN receiver configuration
// =====================================================

const byte CAN_CS_PIN = 10;
const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM = 0x101;

const unsigned long CAN_TIMEOUT_MS = 300;
const unsigned long TELEMETRY_INTERVAL_MS = 100;

MCP_CAN CAN0(CAN_CS_PIN);

// =====================================================
// Status definition — ต้องตรงกับ CAN sender/backend
// =====================================================

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

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;
unsigned long lastTelemetryTime = 0;

bool motorCANAlive = false;
bool armCANAlive = false;
int lastMotorStatus = -1;
int lastArmStatus = -1;
uint16_t telemetrySequence = 0;

bool is_valid_motor_status(byte value)
{
    return value <= BACKWARD_RIGHT;
}

bool is_valid_arm_status(byte value)
{
    return (value == STOP) ||
           (value >= FORWARD && value <= BACKWARD_RIGHT) ||
           (value == Pump_On) ||
           (value == Pump_Off) ||
           (value == Head_Up) ||
           (value == Head_Down);
}

const char *status_name(int status)
{
    switch (status)
    {
    case STOP: return "STOP";
    case FORWARD: return "FORWARD";
    case BACKWARD: return "BACKWARD";
    case LEFT: return "LEFT";
    case RIGHT: return "RIGHT";
    case FORWARD_LEFT: return "FORWARD_LEFT";
    case FORWARD_RIGHT: return "FORWARD_RIGHT";
    case BACKWARD_LEFT: return "BACKWARD_LEFT";
    case BACKWARD_RIGHT: return "BACKWARD_RIGHT";
    case SPIN_LEFT: return "SPIN_LEFT";
    case SPIN_RIGHT: return "SPIN_RIGHT";
    case Pump_On: return "PUMP_ON";
    case Pump_Off: return "PUMP_OFF";
    case Head_Up: return "HEAD_UP";
    case Head_Down: return "HEAD_DOWN";
    default: return "NO_DATA";
    }
}

// =====================================================
// USB telemetry สำหรับ Raspberry Pi
//
// RB2:
// RB2,motor_code,motor_alive,arm_code,arm_alive,
//     battery_mV,battery_adc,seq*CK
//
// Receiver นี้ไม่มี battery/IR sensor จึงส่งสองช่องนั้นเป็น 0
// =====================================================

byte calculate_xor_checksum(const char *payload)
{
    byte checksum = 0;
    while (*payload != '\0')
    {
        checksum ^= static_cast<byte>(*payload);
        payload++;
    }
    return checksum;
}

void send_usb_telemetry()
{
    char payload[80];

    snprintf(
        payload,
        sizeof(payload),
        "RB2,%d,%d,%d,%d,0,0,%u",
        lastMotorStatus,
        motorCANAlive ? 1 : 0,
        lastArmStatus,
        armCANAlive ? 1 : 0,
        telemetrySequence);

    byte checksum = calculate_xor_checksum(payload);

    Serial.print(payload);
    Serial.print('*');
    if (checksum < 0x10)
    {
        Serial.print('0');
    }
    Serial.println(checksum, HEX);

    telemetrySequence++;
}

// =====================================================
// CAN receive — รับและรายงานสถานะเท่านั้น
// ไม่มีการสั่ง motor, servo, PCA9685 หรือ relay ในบอร์ดนี้
// =====================================================

void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData)
{
    if (dataLength < 1)
        return;

    const byte receivedStatus = rxData[0];

    if (receivedId == CAN_ID_MOTOR)
    {
        if (!is_valid_motor_status(receivedStatus))
        {
            motorCANAlive = false;
            lastMotorStatus = -1;
            return;
        }

        lastMotorMessageTime = millis();
        motorCANAlive = true;
        lastMotorStatus = receivedStatus;
        return;
    }

    if (receivedId == CAN_ID_ARM)
    {
        if (!is_valid_arm_status(receivedStatus))
        {
            armCANAlive = false;
            lastArmStatus = -1;
            return;
        }

        lastArmMessageTime = millis();
        armCANAlive = true;
        lastArmStatus = receivedStatus;
    }
}

void read_can_bus()
{
    while (CAN0.checkReceive() == CAN_MSGAVAIL)
    {
        unsigned long receivedId = 0;
        byte dataLength = 0;
        byte receivedData[8];

        byte result = CAN0.readMsgBuf(
            &receivedId,
            &dataLength,
            receivedData);

        if (result != CAN_OK)
            continue;

        Serial.print("CAN RX | ID: 0x");
        Serial.print(receivedId, HEX);
        Serial.print(" | Status: ");
        Serial.print(dataLength > 0 ? receivedData[0] : 0);
        Serial.print(" (");

        if (receivedId == CAN_ID_MOTOR)
            Serial.print(status_name(dataLength > 0 ? receivedData[0] : -1));
        else if (receivedId == CAN_ID_ARM)
            Serial.print(status_name(dataLength > 0 ? receivedData[0] : -1));
        else
            Serial.print("OTHER");

        Serial.println(")");
        process_can_message(receivedId, dataLength, receivedData);
    }
}

void check_can_timeout()
{
    const unsigned long currentTime = millis();

    if (motorCANAlive &&
        currentTime - lastMotorMessageTime > CAN_TIMEOUT_MS)
    {
        motorCANAlive = false;
        lastMotorStatus = -1;
        Serial.println("CAN WARNING: Motor timeout");
    }

    if (armCANAlive &&
        currentTime - lastArmMessageTime > CAN_TIMEOUT_MS)
    {
        armCANAlive = false;
        lastArmStatus = -1;
        Serial.println("CAN WARNING: Arm timeout");
    }
}

void setup()
{
    Serial.begin(115200);

    // ใช้ D53 เป็น hardware SS ของ Mega ให้เป็น OUTPUT เพื่อคง SPI master mode
    pinMode(53, OUTPUT);
    digitalWrite(53, HIGH);
    pinMode(CAN_CS_PIN, OUTPUT);
    digitalWrite(CAN_CS_PIN, HIGH);

    SPI.begin();

    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    Serial.println("MCP2515 initialized. CAN receiver ready");
    lastTelemetryTime = millis();
}

void loop()
{
    read_can_bus();
    check_can_timeout();

    const unsigned long currentTime = millis();
    if (currentTime - lastTelemetryTime >= TELEMETRY_INTERVAL_MS)
    {
        lastTelemetryTime = currentTime;
        send_usb_telemetry();
    }
}
