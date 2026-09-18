#include <SPI.h>
#include <mcp_can.h>
#include "robot_config.h"

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
// ไม่ใช้ขา INT แล้วเพราะเปลี่ยนมาใช้ระบบวนเช็ค (Polling)

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;

// --- ตั้งค่าเวลาของระบบ (Dual-Timeout) ---
const unsigned long CAN_SIGNAL_TIMEOUT = 1000;
const unsigned long SLEEP_TIMEOUT      = 30000;

MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- Relay configuration ----------------
#ifndef RELAY_PUMP_PIN
#define RELAY_PUMP_PIN 4
#endif
const byte RELAY_ON_STATE  = LOW;  
const byte RELAY_OFF_STATE = HIGH; 
bool pumpState = false;

// ---------------- PCA9685 configuration ----------------
PCA9685Control pca;
int servo0_pwm = 335; 
int servo1_pwm = 305; 
int servo2_pwm = 305; 
unsigned long last_servo_update_ms = 0;
const unsigned long SERVO_UPDATE_INTERVAL = 15;

// ---------------- Status definition ----------------
enum PS2_Status : uint8_t {
    STOP = 0, FORWARD = 1, BACKWARD = 2, LEFT = 3, RIGHT = 4,
    FORWARD_LEFT = 5, FORWARD_RIGHT = 6, BACKWARD_LEFT = 7, BACKWARD_RIGHT = 8,
    SPIN_LEFT = 9, SPIN_RIGHT = 10, Pump_On = 11, Pump_Off = 12, Head_Up = 13, Head_Down = 14
};

// ---------------- System Variables ----------------
int lastArmStatus = -1;
unsigned long lastArmMessageTime = 0; 
unsigned long lastActiveTime = 0;     
bool sleepMode = false;

bool is_valid_arm_status(byte value) {
    return (value == STOP) || (value >= FORWARD && value <= BACKWARD_RIGHT) ||
           (value == Pump_On || value == Pump_Off) || (value == Head_Up || value == Head_Down);
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    Serial.begin(115200);

    pinMode(RELAY_PUMP_PIN, OUTPUT);
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE);

    pca.begin();
    pca.setPWMFreq(50.0);
    pca.setPWM(0, 0, servo0_pwm); 
    pca.setPWM(1, 0, servo1_pwm); 
    pca.setPWM(2, 0, servo2_pwm); 

    Serial.println("Initializing MCP2515 (Polling Mode)...");
    
    // ** จุดที่ต้องระวัง: เช็คความถี่คริสตัลบนโมดูล CAN ของคุณว่าสลักเลข 8.000 หรือ 16.000 **
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    lastArmMessageTime = millis();
    lastActiveTime = millis();

    Serial.println("System Ready (UNO). Waiting for CAN data...");
}

// ==================================================
// Main loop 
// ==================================================
void loop()
{
    unsigned long currentTime = millis();

    // ==========================================
    // 1. ตรวจรับข้อความ CAN แบบ Polling (วนเช็คตลอดเวลา)
    // ==========================================
    if (CAN0.checkReceive() == CAN_MSGAVAIL) 
    {
        unsigned long receivedId;
        byte dataLength;
        byte rxData[8];

        CAN0.readMsgBuf(&receivedId, &dataLength, rxData);

        // --- พิมพ์ค่าที่รับได้ออกหน้าจอ เพื่อดูว่ามีสัญญาณมาถึงจริงไหม ---
        Serial.print("CAN RX -> ID: 0x");
        Serial.print(receivedId, HEX);
        Serial.print(" | Data: ");
        if (dataLength > 0) {
            Serial.println(rxData[0]);
        } else {
            Serial.println("Empty");
        }
        // --------------------------------------------------------

        if (receivedId == CAN_ID_ARM && dataLength > 0) 
        {
            byte status = rxData[0];
            if (is_valid_arm_status(status)) 
            {
                lastArmStatus = status;
                lastArmMessageTime = currentTime; 

                if (status != STOP || sleepMode) 
                {
                    lastActiveTime = currentTime; 

                    if (sleepMode) {
                        Serial.println("WAKE UP: System Activated by User");
                        sleepMode = false;
                    }
                }

                if (status == Pump_On) {
                    digitalWrite(RELAY_PUMP_PIN, RELAY_ON_STATE);
                    pumpState = true;
                } else if (status == Pump_Off) {
                    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE);
                    pumpState = false;
                }
            }
        }
    }

    // ==========================================
    // 2. ขยับ Servo
    // ==========================================
    if (!sleepMode && lastArmStatus != -1)
    {
        if (currentTime - last_servo_update_ms >= SERVO_UPDATE_INTERVAL) 
        {
            last_servo_update_ms = currentTime;
            bool servo_changed = false;

            if (lastArmStatus == LEFT || lastArmStatus == FORWARD_LEFT || lastArmStatus == BACKWARD_LEFT) {
                servo0_pwm -= 2; servo_changed = true;
            } else if (lastArmStatus == RIGHT || lastArmStatus == FORWARD_RIGHT || lastArmStatus == BACKWARD_RIGHT) {
                servo0_pwm += 2; servo_changed = true;
            }

            if (lastArmStatus == FORWARD) {
                servo1_pwm -= 2; servo_changed = true;
            } else if (lastArmStatus == BACKWARD) {
                servo1_pwm += 2; servo_changed = true;
            }

            if (lastArmStatus == Head_Up) {
                servo2_pwm -= 2; servo_changed = true; 
            } else if (lastArmStatus == Head_Down) {
                servo2_pwm += 2; servo_changed = true;
            }

            if (servo_changed) 
            {
                servo0_pwm = constrain(servo0_pwm, 150, 450);
                servo1_pwm = constrain(servo1_pwm, 150, 450);
                servo2_pwm = constrain(servo2_pwm, 150, 450);

                pca.setPWM(0, 0, servo0_pwm); 
                pca.setPWM(1, 0, servo1_pwm); 
                pca.setPWM(2, 0, servo2_pwm); 
            }
        }
    }

    // ==========================================
    // 3. ระบบจับเวลา (Timeouts)
    // ==========================================
    if (lastArmStatus != -1 && (currentTime - lastArmMessageTime > CAN_SIGNAL_TIMEOUT))
    {
        lastArmStatus = -1;
        Serial.println("WARNING: CAN Signal Lost! Arm safely stopped.");
    }

    if (!sleepMode && (currentTime - lastActiveTime > SLEEP_TIMEOUT))
    {
        sleepMode = true;
        lastArmStatus = -1; 

        digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE); 
        pumpState = false;

        Serial.println("SLEEP MODE: Inactive for 30 seconds. System halted.");
    }
}