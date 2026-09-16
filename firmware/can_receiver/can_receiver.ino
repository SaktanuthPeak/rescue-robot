#include <SPI.h>
#include <mcp_can.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <math.h>

// =====================================================
// OLED — software I2C
// OLED SDA -> Arduino D6
// OLED SCL -> Arduino D7
// =====================================================

const byte OLED_SDA_PIN = 6;
const byte OLED_SCL_PIN = 7;
const unsigned long OLED_INTERVAL_MS = 250;
unsigned long lastOledTime = 0;

U8G2_SSD1306_128X64_NONAME_1_SW_I2C oled(
    U8G2_R0,
    OLED_SCL_PIN,
    OLED_SDA_PIN,
    U8X8_PIN_NONE);

// =====================================================
// VOLTAGE SENSOR — A0
// =====================================================

const byte VOLTAGE_SENSOR_PIN = A0;
const float VOLTAGE_R1 = 30000.0;
const float VOLTAGE_R2 = 7500.0;
const float ADC_REFERENCE_VOLTAGE = 5.0;

int voltageAdcValue = 0;
float inputVoltage = 0.0;

// =====================================================
// IR SENSOR — A1 ถึง A4
// Order: front, right, rear, left
// =====================================================

const byte IR_SENSOR_COUNT = 4;
const byte IR_SENSOR_PINS[IR_SENSOR_COUNT] = {A1, A2, A3, A4};
int irAdcValue[IR_SENSOR_COUNT] = {0, 0, 0, 0};

// =====================================================
// DHT11 HUMIDITY SENSOR — D8
// =====================================================

const byte DHT_SENSOR_PIN = 8;
const byte DHT_SENSOR_TYPE = DHT11;
const unsigned long DHT_INTERVAL_MS = 2000;

DHT dht(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
int humidityPercent = -1;
float temperatureC = -1.0;
unsigned long lastDhtTime = 0;

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
// RB3:
// RB3,motor_code,motor_alive,arm_code,arm_alive,
//     battery_mV,battery_adc,ir_front,ir_right,ir_rear,ir_left,
//     humidity_percent,seq*CK
//
// ใช้ USB Serial port เดิมที่ 115200 baud
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
    char payload[128];

    const unsigned long voltageMillivolts =
        static_cast<unsigned long>((inputVoltage * 1000.0) + 0.5);

    snprintf(
        payload,
        sizeof(payload),
        "RB3,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%u",
        lastMotorStatus,
        motorCANAlive ? 1 : 0,
        lastArmStatus,
        armCANAlive ? 1 : 0,
        voltageMillivolts,
        voltageAdcValue,
        irAdcValue[0],
        irAdcValue[1],
        irAdcValue[2],
        irAdcValue[3],
        humidityPercent,
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
// SENSOR READING
// =====================================================

void read_voltage_sensor()
{
    voltageAdcValue = analogRead(VOLTAGE_SENSOR_PIN);

    const float adcVoltage =
        (voltageAdcValue * ADC_REFERENCE_VOLTAGE) / 1024.0;

    inputVoltage =
        adcVoltage * (VOLTAGE_R1 + VOLTAGE_R2) / VOLTAGE_R2;
}

void read_ir_sensors()
{
    for (byte i = 0; i < IR_SENSOR_COUNT; i++)
    {
        irAdcValue[i] = analogRead(IR_SENSOR_PINS[i]);
    }
}

void read_dht_sensor()
{
    const float humidity = dht.readHumidity();
    const float temperature = dht.readTemperature();

    // DHT11 can occasionally return NaN while the line is settling. Keep the
    // sentinel values so the backend/UI can distinguish unavailable data.
    if (isnan(humidity) || isnan(temperature))
    {
        humidityPercent = -1;
        temperatureC = -1.0;
        Serial.println("DHT11 | read failed");
        return;
    }

    humidityPercent = constrain(static_cast<int>(humidity + 0.5f), 0, 100);
    temperatureC = temperature;

    Serial.print("DHT11 | Humidity: ");
    Serial.print(humidityPercent);
    Serial.print("% | Temperature: ");
    Serial.print(temperatureC, 1);
    Serial.println(" C");
}

// =====================================================
// OLED BATTERY VIEW
// =====================================================

void draw_battery_icon(int x, int y, int width, int height)
{
    const int terminalWidth = 5;
    const int terminalHeight = 12;
    const int terminalY = y + (height - terminalHeight) / 2;

    oled.drawFrame(x, y, width, height);
    oled.drawBox(x + width, terminalY, terminalWidth, terminalHeight);

    // The icon is intentionally a battery indicator, not a battery percentage
    // gauge. Numeric voltage remains the source of truth until chemistry limits
    // are configured for the actual battery pack.
    if (inputVoltage > 0.0)
    {
        oled.drawBox(x + 3, y + 3, width - 6, height - 6);
    }
}

void update_oled()
{
    oled.firstPage();

    do
    {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(42, 10, "BATTERY");

        draw_battery_icon(27, 17, 74, 30);

        oled.setCursor(38, 59);
        oled.print(inputVoltage, 2);
        oled.print(" V");
    } while (oled.nextPage());
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

    pinMode(VOLTAGE_SENSOR_PIN, INPUT);
    for (byte i = 0; i < IR_SENSOR_COUNT; i++)
    {
        pinMode(IR_SENSOR_PINS[i], INPUT);
    }

    dht.begin();

    oled.begin();
    read_voltage_sensor();
    read_ir_sensors();
    update_oled();

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
    lastOledTime = millis();
    lastDhtTime = millis();
}

void loop()
{
    read_can_bus();
    check_can_timeout();

    const unsigned long currentTime = millis();
    if (currentTime - lastTelemetryTime >= TELEMETRY_INTERVAL_MS)
    {
        lastTelemetryTime = currentTime;
        read_voltage_sensor();
        read_ir_sensors();
        send_usb_telemetry();
    }

    // DHT11 is a slow sensor; do not poll it at the 100 ms telemetry rate.
    if (currentTime - lastDhtTime >= DHT_INTERVAL_MS)
    {
        lastDhtTime = currentTime;
        read_dht_sensor();
    }

    if (currentTime - lastOledTime >= OLED_INTERVAL_MS)
    {
        lastOledTime = currentTime;
        update_oled();
    }
}
