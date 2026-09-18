#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mcp_can.h>

#include "battery_sensor.h"
#include "encoder.h"
#include "ir_sensors.h"
#include "motor.h"
#include "robot_config.h"

// -----------------------------------------------------------------------------
// Command/status codes shared with the CAN sender, can_receiver and FastAPI.
// -----------------------------------------------------------------------------
enum RobotCommand : uint8_t
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
    PUMP_ON = 11,
    PUMP_OFF = 12,
    HEAD_UP = 13,
    HEAD_DOWN = 14
};

MCP_CAN CAN0(CAN_CS_PIN);

int lastMotorStatus = -1;
bool motorCanAlive = false;
unsigned long lastMotorMessageTime = 0;

int lastArmStatus = -1;
bool armCanAlive = false;
unsigned long lastArmMessageTime = 0;

int serialMotorStatus = STOP;
bool serialMotorOverride = false;
unsigned long lastSerialMotorCommandTime = 0;

int serialArmStatus = STOP;
bool serialArmOverride = false;
unsigned long lastSerialArmCommandTime = 0;
unsigned long lastCanForwardTime = 0;

unsigned long lastControlLoopMs = 0;
unsigned long lastTelemetryMs = 0;
uint16_t telemetrySequence = 0;

BatterySample batterySample = {0, 0};
IrSample irSample = {0, 0, 0, 0};

bool valid_motor_status(uint8_t value)
{
    return value <= SPIN_RIGHT;
}

bool valid_arm_status(uint8_t value)
{
    return value <= BACKWARD_RIGHT ||
           value == PUMP_ON ||
           value == PUMP_OFF ||
           value == HEAD_UP ||
           value == HEAD_DOWN;
}

void apply_motor_from_status(RobotCommand command)
{
    switch (command)
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

bool send_can_command(unsigned long id, uint8_t command)
{
    uint8_t payload[1] = {command};
    return CAN0.sendMsgBuf(id, 0, 1, payload) == CAN_OK;
}

void process_can_message(unsigned long id, uint8_t length, uint8_t *data)
{
    if (length < 1)
    {
        return;
    }

    const uint8_t value = data[0];
    const unsigned long now = millis();

    if (id == CAN_ID_MOTOR)
    {
        if (!valid_motor_status(value))
        {
            motorCanAlive = false;
            if (!serialMotorOverride) motor_stop();
            return;
        }

        lastMotorStatus = value;
        motorCanAlive = true;
        lastMotorMessageTime = now;
        if (!serialMotorOverride)
        {
            apply_motor_from_status(static_cast<RobotCommand>(value));
        }
        return;
    }

    if (id == CAN_ID_ARM)
    {
        if (!valid_arm_status(value))
        {
            armCanAlive = false;
            return;
        }

        lastArmStatus = value;
        armCanAlive = true;
        lastArmMessageTime = now;
    }
}

void read_can_bus()
{
    while (CAN0.checkReceive() == CAN_MSGAVAIL)
    {
        unsigned long id = 0;
        uint8_t length = 0;
        uint8_t data[8] = {0};

        if (CAN0.readMsgBuf(&id, &length, data) == CAN_OK)
        {
            process_can_message(id, length, data);
        }
    }
}

// -----------------------------------------------------------------------------
// FastAPI -> Mega serial protocol
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

void serial_ack(const __FlashStringHelper *command)
{
    Serial.print(F("{\"t\":\"ACK\",\"cmd\":\""));
    Serial.print(command);
    Serial.println(F("\",\"ok\":true}"));
}

void serial_error(const __FlashStringHelper *reason)
{
    Serial.print(F("{\"t\":\"ERR\",\"error\":\""));
    Serial.print(reason);
    Serial.println(F("\"}"));
}

void set_serial_motor_command(int code)
{
    serialMotorStatus = code;
    serialMotorOverride = true;
    lastSerialMotorCommandTime = millis();
    apply_motor_from_status(static_cast<RobotCommand>(code));
}

void set_serial_arm_command(int code)
{
    serialArmStatus = code;
    serialArmOverride = true;
    lastSerialArmCommandTime = millis();
    send_can_command(CAN_ID_ARM, static_cast<uint8_t>(code));
}

void stop_all_from_serial()
{
    set_serial_motor_command(STOP);
    set_serial_arm_command(STOP);
    motor_stop();
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
    if (parse_code(line, "CMD:MOTOR:", STOP, SPIN_RIGHT, &code))
    {
        set_serial_motor_command(code);
        serial_ack(F("MOTOR"));
        return;
    }

    if (parse_code(line, "CMD:ARM:", STOP, HEAD_DOWN, &code) &&
        valid_arm_status(static_cast<uint8_t>(code)))
    {
        set_serial_arm_command(code);
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
        }
        else if (character != '\r')
        {
            if (length < sizeof(line) - 1)
            {
                line[length++] = character;
            }
            else
            {
                length = 0;
            }
        }
    }
}

void service_serial_overrides(unsigned long now)
{
    if (serialMotorOverride &&
        now - lastSerialMotorCommandTime > SERIAL_COMMAND_TIMEOUT_MS)
    {
        serialMotorOverride = false;
        if (motorCanAlive)
        {
            apply_motor_from_status(static_cast<RobotCommand>(lastMotorStatus));
        }
        else
        {
            motor_stop();
        }
    }

    if (serialArmOverride &&
        now - lastSerialArmCommandTime > SERIAL_COMMAND_TIMEOUT_MS)
    {
        serialArmOverride = false;
        send_can_command(CAN_ID_ARM, STOP);
    }

    if (serialArmOverride &&
        now - lastCanForwardTime >= CAN_FORWARD_INTERVAL_MS)
    {
        lastCanForwardTime = now;
        send_can_command(CAN_ID_ARM, static_cast<uint8_t>(serialArmStatus));
    }
}

void check_can_timeouts(unsigned long now)
{
    if (motorCanAlive && now - lastMotorMessageTime > CAN_TIMEOUT_MS)
    {
        motorCanAlive = false;
        lastMotorStatus = -1;
        if (!serialMotorOverride) motor_stop();
    }

    if (armCanAlive && now - lastArmMessageTime > CAN_TIMEOUT_MS)
    {
        armCanAlive = false;
        lastArmStatus = -1;
    }
}

// -----------------------------------------------------------------------------
// Telemetry: Mega -> FastAPI
// MC1,motor_code,motor_alive,arm_code,arm_alive,battery_mV,battery_adc,
//     ir_front,ir_right,ir_rear,ir_left,
//     enc_fl,enc_fr,enc_bl,enc_br,
//     speed_fl,speed_fr,speed_bl,speed_br,seq*CK
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

int active_motor_status()
{
    return serialMotorOverride ? serialMotorStatus :
        (motorCanAlive ? lastMotorStatus : -1);
}

int active_arm_status()
{
    return serialArmOverride ? serialArmStatus :
        (armCanAlive ? lastArmStatus : -1);
}

void send_can_telemetry()
{
    const int16_t speed_fl = static_cast<int16_t>(encoder_get_speed_fl());
    const int16_t speed_fr = static_cast<int16_t>(encoder_get_speed_fr());
    const int16_t speed_bl = static_cast<int16_t>(encoder_get_speed_bl());
    const int16_t speed_br = static_cast<int16_t>(encoder_get_speed_br());

    uint8_t payload[8] = {
        static_cast<uint8_t>(active_motor_status() < 0 ? STOP : active_motor_status()),
        (serialMotorOverride || motorCanAlive) ? 1 : 0,
        static_cast<uint8_t>((speed_fl >> 8) & 0xFF),
        static_cast<uint8_t>(speed_fl & 0xFF),
        static_cast<uint8_t>((speed_fr >> 8) & 0xFF),
        static_cast<uint8_t>(speed_fr & 0xFF),
        static_cast<uint8_t>((speed_bl >> 8) & 0xFF),
        static_cast<uint8_t>(speed_bl & 0xFF),
    };
    CAN0.sendMsgBuf(CAN_ID_TELEMETRY, 0, 8, payload);
}

void send_serial_telemetry()
{
    char payload[256];
    const int motorStatus = active_motor_status();
    const int armStatus = active_arm_status();

    snprintf(
        payload,
        sizeof(payload),
        "MC1,%d,%d,%d,%d,%lu,%u,%u,%u,%u,%u,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%u",
        motorStatus,
        (serialMotorOverride || motorCanAlive) ? 1 : 0,
        armStatus,
        (serialArmOverride || armCanAlive) ? 1 : 0,
        static_cast<unsigned long>(batterySample.millivolts),
        batterySample.adc,
        irSample.front,
        irSample.right,
        irSample.rear,
        irSample.left,
        encoder_get_ticks_fl(),
        encoder_get_ticks_fr(),
        encoder_get_ticks_bl(),
        encoder_get_ticks_br(),
        static_cast<long>(encoder_get_speed_fl()),
        static_cast<long>(encoder_get_speed_fr()),
        static_cast<long>(encoder_get_speed_bl()),
        static_cast<long>(encoder_get_speed_br()),
        telemetrySequence);

    const uint8_t checksum = calculate_xor_checksum(payload);
    Serial.print(payload);
    Serial.print('*');
    if (checksum < 0x10) Serial.print('0');
    Serial.println(checksum, HEX);
    telemetrySequence++;
}

void send_telemetry()
{
    batterySample = battery_read();
    irSample = ir_sensors_read();
    send_can_telemetry();
    send_serial_telemetry();
}

void setup()
{
    Serial.begin(115200);

    motor_init();
    encoder_init();
    battery_init();
    ir_sensors_init();

    // D53 is Mega hardware SS and must stay OUTPUT/HIGH while using SPI.
    pinMode(SPI_SS_PIN, OUTPUT);
    digitalWrite(SPI_SS_PIN, HIGH);
    pinMode(CAN_CS_PIN, OUTPUT);
    digitalWrite(CAN_CS_PIN, HIGH);
    SPI.begin();

    while (CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK_SET) != CAN_OK)
    {
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    motor_stop();
    const unsigned long now = millis();
    lastControlLoopMs = now;
    lastTelemetryMs = now;
    lastCanForwardTime = now;
}

void loop()
{
    process_serial_commands();
    read_can_bus();

    const unsigned long now = millis();
    check_can_timeouts(now);
    service_serial_overrides(now);

    if (now - lastControlLoopMs >= CONTROL_LOOP_INTERVAL_MS)
    {
        const float dt = static_cast<float>(now - lastControlLoopMs) / 1000.0f;
        lastControlLoopMs = now;
        encoder_update_speeds(dt);
        motor_update_pid(dt);
    }

    if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS)
    {
        lastTelemetryMs = now;
        send_telemetry();
    }
}
