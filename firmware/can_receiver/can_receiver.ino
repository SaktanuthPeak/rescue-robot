#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mcp_can.h>

#include "PCA9685_Control.h"

// -----------------------------------------------------------------------------
// Hardware pin map
// -----------------------------------------------------------------------------
// A4/A5 are the hardware I2C pins used by PCA9685. Battery and IR sensors live
// on firmware/motor_controller_mega/ instead of this arm/CAN bridge board.
const uint8_t RELAY_PUMP_PIN = 4;
const uint8_t CAN_CS_PIN = 10;

// -----------------------------------------------------------------------------
// CAN
// -----------------------------------------------------------------------------
const uint8_t CAN_CLOCK = MCP_8MHZ;
const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM = 0x101;
const unsigned long CAN_TIMEOUT_MS = 1000;
const uint8_t MAX_RX_PER_LOOP = 8;

MCP_CAN CAN0(CAN_CS_PIN);

// -----------------------------------------------------------------------------
// PCA9685 arm controller
// -----------------------------------------------------------------------------
PCA9685Control pca;

const uint8_t SERVO_COUNT = 3;
int servoPWM[SERVO_COUNT] = {335, 305, 305};
const int SERVO_MIN[SERVO_COUNT] = {150, 150, 150};
const int SERVO_MAX[SERVO_COUNT] = {450, 450, 450};
const int SERVO_STEP = 2;
const unsigned long SERVO_INTERVAL_MS = 15;
unsigned long lastServoTime = 0;

const uint8_t RELAY_ON_STATE = LOW;
const uint8_t RELAY_OFF_STATE = HIGH;
bool pumpOn = false;

// -----------------------------------------------------------------------------
// Shared command/status codes
// -----------------------------------------------------------------------------
enum ArmCommand : uint8_t
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
    PUMP_ON = 11,
    PUMP_OFF = 12,
    HEAD_UP = 13,
    HEAD_DOWN = 14
};

bool valid_motor_command(uint8_t command)
{
    return command <= BACKWARD_RIGHT;
}

bool valid_arm_command(uint8_t command)
{
    return command <= BACKWARD_RIGHT ||
           command == PUMP_ON ||
           command == PUMP_OFF ||
           command == HEAD_UP ||
           command == HEAD_DOWN;
}

const __FlashStringHelper *command_name(int command)
{
    switch (command)
    {
        case STOP: return F("STOP");
        case FORWARD: return F("FORWARD");
        case BACKWARD: return F("BACKWARD");
        case LEFT: return F("LEFT");
        case RIGHT: return F("RIGHT");
        case FORWARD_LEFT: return F("FORWARD_LEFT");
        case FORWARD_RIGHT: return F("FORWARD_RIGHT");
        case BACKWARD_LEFT: return F("BACKWARD_LEFT");
        case BACKWARD_RIGHT: return F("BACKWARD_RIGHT");
        case PUMP_ON: return F("PUMP_ON");
        case PUMP_OFF: return F("PUMP_OFF");
        case HEAD_UP: return F("HEAD_UP");
        case HEAD_DOWN: return F("HEAD_DOWN");
        default: return F("NO_DATA");
    }
}

// -----------------------------------------------------------------------------
// Control source state
// -----------------------------------------------------------------------------
// CAN is the local remote-control source. A valid USB command temporarily
// overrides CAN so FastAPI can control the robot even while the remote keeps
// transmitting STOP heartbeats. The override expires automatically.
int canMotorCommand = -1;
bool canMotorAlive = false;
unsigned long lastMotorRxTime = 0;

int canArmCommand = -1;
bool canArmAlive = false;
unsigned long lastArmRxTime = 0;

int activeMotorCommand = -1;
int activeArmCommand = -1;

int serialMotorCommand = STOP;
bool serialMotorActive = false;
unsigned long lastSerialMotorCommandTime = 0;

int serialArmCommand = STOP;
bool serialArmActive = false;
bool serialArmStopOverride = false;
unsigned long lastSerialArmCommandTime = 0;

const unsigned long SERIAL_COMMAND_TIMEOUT_MS = 1000;
const unsigned long SERIAL_CAN_TX_INTERVAL_MS = 50;
const unsigned long ARM_SLEEP_MS = 30000;
bool armSleeping = false;
unsigned long lastActivityTime = 0;
unsigned long lastSerialMotorTxTime = 0;

// -----------------------------------------------------------------------------
// Telemetry state
// -----------------------------------------------------------------------------
const unsigned long TELEMETRY_INTERVAL_MS = 100;
unsigned long lastTelemetryTime = 0;
uint16_t telemetrySequence = 0;

unsigned long totalRxCount = 0;
unsigned long motorRxCount = 0;
unsigned long armRxCount = 0;
unsigned long invalidRxCount = 0;
unsigned long readErrorCount = 0;

void set_pump(bool enabled)
{
    pumpOn = enabled;
    digitalWrite(RELAY_PUMP_PIN, enabled ? RELAY_ON_STATE : RELAY_OFF_STATE);
}

bool send_can_status(unsigned long canId, uint8_t command)
{
    uint8_t data[1] = {command};
    return CAN0.sendMsgBuf(canId, 0, 1, data) == CAN_OK;
}

void refresh_active_commands()
{
    activeMotorCommand = serialMotorActive
        ? serialMotorCommand
        : (canMotorAlive ? canMotorCommand : -1);

    activeArmCommand = serialArmActive
        ? serialArmCommand
        : (serialArmStopOverride ? STOP : (canArmAlive ? canArmCommand : -1));
}

void stop_arm_if_no_override()
{
    if (!serialArmActive)
    {
        activeArmCommand = STOP;
        set_pump(false);
    }
}

// -----------------------------------------------------------------------------
// CAN reception
// -----------------------------------------------------------------------------
void process_can_message(unsigned long id, uint8_t length, uint8_t *data)
{
    if (length < 1)
    {
        invalidRxCount++;
        return;
    }

    const uint8_t command = data[0];
    const unsigned long now = millis();

    if (id == CAN_ID_MOTOR)
    {
        motorRxCount++;
        if (!valid_motor_command(command))
        {
            invalidRxCount++;
            canMotorAlive = false;
            canMotorCommand = -1;
            refresh_active_commands();
            return;
        }

        canMotorCommand = command;
        canMotorAlive = true;
        lastMotorRxTime = now;
        refresh_active_commands();
        return;
    }

    if (id == CAN_ID_ARM)
    {
        armRxCount++;
        if (!valid_arm_command(command))
        {
            invalidRxCount++;
            canArmAlive = false;
            canArmCommand = -1;
            stop_arm_if_no_override();
            return;
        }

        canArmCommand = command;
        canArmAlive = true;
        lastArmRxTime = now;

        if (command != STOP)
        {
            lastActivityTime = now;
            armSleeping = false;
        }

        if (!serialArmActive && !serialArmStopOverride)
        {
            activeArmCommand = command;
            if (command == PUMP_ON)
            {
                set_pump(true);
            }
            else if (command == PUMP_OFF)
            {
                set_pump(false);
            }
        }
    }
}

void read_can()
{
    for (uint8_t i = 0; i < MAX_RX_PER_LOOP; i++)
    {
        if (CAN0.checkReceive() != CAN_MSGAVAIL)
        {
            break;
        }

        unsigned long id = 0;
        uint8_t length = 0;
        uint8_t data[8] = {0};

        if (CAN0.readMsgBuf(&id, &length, data) != CAN_OK)
        {
            readErrorCount++;
            break;
        }

        totalRxCount++;
        process_can_message(id, length, data);
    }
}

// -----------------------------------------------------------------------------
// Serial protocol: FastAPI -> receiver
// -----------------------------------------------------------------------------
bool parse_code(const char *line, const char *prefix, int minimum, int maximum, int *result)
{
    const size_t prefixLength = strlen(prefix);
    if (strncmp(line, prefix, prefixLength) != 0 || line[prefixLength] == '\0')
    {
        return false;
    }

    char *end = nullptr;
    const long value = strtol(line + prefixLength, &end, 10);
    if (*end != '\0' || value < minimum || value > maximum)
    {
        return false;
    }

    *result = static_cast<int>(value);
    return true;
}

void serial_ack(const __FlashStringHelper *channel)
{
    Serial.print(F("{\"t\":\"ACK\",\"cmd\":\""));
    Serial.print(channel);
    Serial.println(F("\",\"ok\":true}"));
}

void serial_error(const __FlashStringHelper *reason)
{
    Serial.print(F("{\"t\":\"ERR\",\"error\":\""));
    Serial.print(reason);
    Serial.println(F("\"}"));
}

void apply_serial_motor(int code)
{
    serialMotorCommand = code;
    // Keep STOP alive for the same watchdog window so a remote heartbeat cannot
    // immediately overwrite an explicit FastAPI emergency stop.
    serialMotorActive = true;
    lastSerialMotorCommandTime = millis();
    lastSerialMotorTxTime = lastSerialMotorCommandTime;
    refresh_active_commands();

    // This board is the USB/CAN bridge for the motor controller.
    send_can_status(CAN_ID_MOTOR, static_cast<uint8_t>(code));
}

void service_serial_motor(unsigned long now)
{
    if (!serialMotorActive ||
        now - lastSerialMotorTxTime < SERIAL_CAN_TX_INTERVAL_MS)
    {
        return;
    }

    lastSerialMotorTxTime = now;
    send_can_status(CAN_ID_MOTOR, static_cast<uint8_t>(serialMotorCommand));
}

void apply_serial_arm(int code)
{
    serialArmCommand = code;
    serialArmStopOverride = code == STOP;
    serialArmActive =
        (code >= FORWARD && code <= BACKWARD_RIGHT) ||
        code == HEAD_UP ||
        code == HEAD_DOWN;
    lastSerialArmCommandTime = millis();
    armSleeping = false;
    lastActivityTime = lastSerialArmCommandTime;
    activeArmCommand = code;

    if (code == PUMP_ON)
    {
        set_pump(true);
    }
    else if (code == PUMP_OFF || code == STOP)
    {
        set_pump(false);
    }
}

void stop_all_from_serial()
{
    serialMotorActive = true;
    serialArmActive = false;
    serialArmStopOverride = true;
    serialMotorCommand = STOP;
    serialArmCommand = STOP;
    lastSerialMotorCommandTime = millis();
    lastSerialMotorTxTime = lastSerialMotorCommandTime;
    lastSerialArmCommandTime = lastSerialMotorCommandTime;
    activeMotorCommand = STOP;
    activeArmCommand = STOP;
    set_pump(false);
    send_can_status(CAN_ID_MOTOR, STOP);
}

void process_serial_command(char *line)
{
    if (line[0] == '\0')
    {
        return;
    }

    if (strcmp(line, "PING") == 0)
    {
        Serial.println(F("{\"t\":\"PONG\"}"));
        return;
    }

    if (strcmp(line, "STOP") == 0 || strcmp(line, "CMD:ALL:0") == 0)
    {
        stop_all_from_serial();
        serial_ack(F("ALL_STOP"));
        return;
    }

    int code = 0;
    if (parse_code(line, "CMD:MOTOR:", STOP, BACKWARD_RIGHT, &code))
    {
        apply_serial_motor(code);
        serial_ack(F("MOTOR"));
        return;
    }

    if (parse_code(line, "CMD:ARM:", STOP, 14, &code) && valid_arm_command(code))
    {
        apply_serial_arm(code);
        serial_ack(F("ARM"));
        return;
    }

    serial_error(F("UNKNOWN_OR_INVALID_COMMAND"));
}

void process_serial_commands()
{
    static char line[48];
    static uint8_t length = 0;

    while (Serial.available() > 0)
    {
        const char character = static_cast<char>(Serial.read());

        if (character == '\n')
        {
            line[length] = '\0';
            process_serial_command(line);
            length = 0;
            continue;
        }

        if (character == '\r')
        {
            continue;
        }

        if (length < sizeof(line) - 1)
        {
            line[length++] = character;
        }
        else
        {
            // Drop an overlong line instead of executing a truncated command.
            length = 0;
        }
    }
}

// -----------------------------------------------------------------------------
// Timeouts and arm motion
// -----------------------------------------------------------------------------
void check_timeouts(unsigned long now)
{
    if (canMotorAlive && now - lastMotorRxTime > CAN_TIMEOUT_MS)
    {
        canMotorAlive = false;
        canMotorCommand = -1;
        refresh_active_commands();
    }

    if (canArmAlive && now - lastArmRxTime > CAN_TIMEOUT_MS)
    {
        canArmAlive = false;
        canArmCommand = -1;
        refresh_active_commands();
        stop_arm_if_no_override();
    }

    if (serialMotorActive &&
        now - lastSerialMotorCommandTime > SERIAL_COMMAND_TIMEOUT_MS)
    {
        serialMotorActive = false;
        serialMotorCommand = STOP;
        send_can_status(CAN_ID_MOTOR, STOP);
        refresh_active_commands();
    }

    if (serialArmActive &&
        now - lastSerialArmCommandTime > SERIAL_COMMAND_TIMEOUT_MS)
    {
        serialArmActive = false;
        serialArmCommand = STOP;
        refresh_active_commands();
        if (!canArmAlive)
        {
            activeArmCommand = STOP;
            set_pump(false);
        }
    }

    if (serialArmStopOverride &&
        now - lastSerialArmCommandTime > SERIAL_COMMAND_TIMEOUT_MS)
    {
        serialArmStopOverride = false;
        refresh_active_commands();
    }

    if (!armSleeping && now - lastActivityTime >= ARM_SLEEP_MS)
    {
        armSleeping = true;
        set_pump(false);
        if (!serialArmActive)
        {
            activeArmCommand = STOP;
        }
    }
}

void update_arm(unsigned long now)
{
    if (armSleeping || activeArmCommand < 0 || activeArmCommand == STOP)
    {
        return;
    }

    if (now - lastServoTime < SERVO_INTERVAL_MS)
    {
        return;
    }

    lastServoTime = now;

    int channel = -1;
    int change = 0;

    switch (activeArmCommand)
    {
        case LEFT:
        case FORWARD_LEFT:
        case BACKWARD_LEFT:
            channel = 0;
            change = -SERVO_STEP;
            break;

        case RIGHT:
        case FORWARD_RIGHT:
        case BACKWARD_RIGHT:
            channel = 0;
            change = SERVO_STEP;
            break;

        case FORWARD:
            channel = 1;
            change = -SERVO_STEP;
            break;

        case BACKWARD:
            channel = 1;
            change = SERVO_STEP;
            break;

        case HEAD_UP:
            channel = 2;
            change = -SERVO_STEP;
            break;

        case HEAD_DOWN:
            channel = 2;
            change = SERVO_STEP;
            break;

        default:
            return;
    }

    const int nextPWM = constrain(
        servoPWM[channel] + change,
        SERVO_MIN[channel],
        SERVO_MAX[channel]);

    if (nextPWM != servoPWM[channel])
    {
        servoPWM[channel] = nextPWM;
        pca.setPWM(channel, 0, servoPWM[channel]);
    }
}

// -----------------------------------------------------------------------------
// RB4 telemetry: receiver -> FastAPI
// RB4,motor_code,motor_alive,arm_code,arm_alive,battery_mV,battery_adc,
//     axis1_pwm,axis2_pwm,axis3_pwm,pump_on,seq*CK
// -----------------------------------------------------------------------------
uint8_t calculate_xor_checksum(const char *payload)
{
    uint8_t checksum = 0;
    while (*payload != '\0')
    {
        checksum ^= static_cast<uint8_t>(*payload);
        payload++;
    }
    return checksum;
}

void send_telemetry()
{
    char payload[128];
    const int motorCode = activeMotorCommand;
    const int armCode = activeArmCommand;
    snprintf(
        payload,
        sizeof(payload),
        "RB4,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%u",
        motorCode,
        (serialMotorActive || canMotorAlive) ? 1 : 0,
        armCode,
        (serialArmActive || serialArmStopOverride || canArmAlive) ? 1 : 0,
        0UL,
        0,
        servoPWM[0],
        servoPWM[1],
        servoPWM[2],
        pumpOn ? 1 : 0,
        telemetrySequence);

    const uint8_t checksum = calculate_xor_checksum(payload);
    Serial.print(payload);
    Serial.print('*');
    if (checksum < 0x10)
    {
        Serial.print('0');
    }
    Serial.println(checksum, HEX);

    telemetrySequence++;
}

// -----------------------------------------------------------------------------
// Arduino lifecycle
// -----------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);

    pinMode(RELAY_PUMP_PIN, OUTPUT);
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE);

    // A4/SDA and A5/SCL are reserved for PCA9685. No IR/DHT/OLED sensor
    // feature is initialized in this sketch.
    Wire.begin();
    pca.begin();
    pca.setPWMFreq(50.0f);
    for (uint8_t i = 0; i < SERVO_COUNT; i++)
    {
        pca.setPWM(i, 0, servoPWM[i]);
    }

    pinMode(SS, OUTPUT);
    digitalWrite(SS, HIGH);
    pinMode(CAN_CS_PIN, OUTPUT);
    digitalWrite(CAN_CS_PIN, HIGH);
    SPI.begin();

    while (CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK) != CAN_OK)
    {
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    const unsigned long now = millis();
    lastServoTime = now;
    lastActivityTime = now;
    lastTelemetryTime = now;
}

void loop()
{
    process_serial_commands();
    read_can();

    const unsigned long now = millis();
    check_timeouts(now);
    refresh_active_commands();
    service_serial_motor(now);
    update_arm(now);

    if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS)
    {
        lastTelemetryTime = now;
        send_telemetry();
    }
}
