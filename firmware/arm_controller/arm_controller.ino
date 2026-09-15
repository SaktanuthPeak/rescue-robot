#include <SPI.h>
#include <mcp_can.h>
#include "PCA9685_Control.h"
#include "robot_config.h"

// --- นำเข้าไลบรารีสำหรับจัดการระดับ Low-Level ของ AVR ---
#include <avr/wdt.h>
#include <avr/interrupt.h>

// ---------------- CAN configuration ----------------
const byte CAN_CS_PIN = 10;
// ขา INT บน UNO คือ Pin 2 (PD2 / INT0) เราจะจัดการผ่าน Register ด้านล่าง

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM   = 0x101;

// --- ตั้งค่าเวลาของระบบ (Dual-Timeout) ---
const unsigned long CAN_SIGNAL_TIMEOUT = 1000;  // 1 วินาที: สัญญาณขาดหาย สั่งหยุดแขนทันที
const unsigned long SLEEP_TIMEOUT      = 30000; // 30 วินาที: ปล่อยจอยทิ้งไว้ เข้าสู่ Sleep Mode

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
volatile bool isDataReady = false; 
int lastArmStatus = -1;

unsigned long lastArmMessageTime = 0; 
unsigned long lastActiveTime = 0;     
bool sleepMode = false;

bool is_valid_arm_status(byte value) {
    return (value == STOP) || (value >= FORWARD && value <= BACKWARD_RIGHT) ||
           (value == Pump_On || value == Pump_Off) || (value == Head_Up || value == Head_Down);
}

// ==================================================
// Low-Level Interrupt Service Routine (ISR) สำหรับ INT0 (Pin 2 บน UNO)
// ==================================================
ISR(INT0_vect) 
{
    isDataReady = true; 
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    // 1. ปิด Watchdog ก่อนเป็นอันดับแรก (ป้องกันการติดลูป Reset หากบอร์ดเพิ่งบูต)
    cli(); // ปิด Interrupt ชั่วคราว
    MCUSR &= ~(1 << WDRF); // เคลียร์ Flag การรีเซ็ตจาก Watchdog
    WDTCSR |= (1 << WDCE) | (1 << WDE); 
    WDTCSR = 0x00; // ปิด WDT
    sei();

    Serial.begin(115200);

    pinMode(RELAY_PUMP_PIN, OUTPUT);
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE);

    // ทันทีที่บอร์ดเริ่มทำงาน (หรือโดน WDT Reset) จะดึงแขนกลมาที่จุดปลอดภัยทันที
    pca.begin();
    pca.setPWMFreq(50.0);
    pca.setPWM(0, 0, servo0_pwm); 
    pca.setPWM(1, 0, servo1_pwm); 
    pca.setPWM(2, 0, servo2_pwm); 

    Serial.println("Initializing MCP2515...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);
    
    // ล้าง Buffer ให้ว่างเพื่อเตรียมรับ Interrupt
    while (CAN0.checkReceive() == CAN_MSGAVAIL) {
        unsigned long dummyId; byte dummyLen; byte dummyBuf[8];
        CAN0.readMsgBuf(&dummyId, &dummyLen, dummyBuf);
    }

    // ==========================================
    // 2. Low-Level Configuration: ขา INT0 (Pin 2 บน UNO)
    // ==========================================
    cli(); 
    // ตั้งค่า Pin 2 (PD2) เป็น Input (DDRD2 = 0) และเปิด Pull-up (PORTD2 = 1)
    DDRD &= ~(1 << PD2); 
    PORTD |= (1 << PD2);
    
    // ตั้งค่าให้ INT0 ทริกเกอร์เมื่อเกิดขอบขาลง (FALLING EDGE: ISC01=1, ISC00=0)
    EICRA = (EICRA & ~((1 << ISC00) | (1 << ISC01))) | (1 << ISC01);
    
    // เปิดใช้งานการขัดจังหวะที่ INT0
    EIMSK |= (1 << INT0);
    
    // ==========================================
    // 3. Low-Level Configuration: Watchdog Timer
    // ==========================================
    // เข้าสู่โหมดแก้ไขค่า WDT
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    // เปิดการตั้งค่าแบบ System Reset Mode (WDE=1) 
    // และกำหนดระยะเวลา Timeout เป็น 0.5 วินาที (500ms) ด้วย Prescaler (WDP2=1, WDP0=1)
    WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP0);
    sei(); // เปิด Interrupt ทั้งระบบกลับมา
    
    lastArmMessageTime = millis();
    lastActiveTime = millis();

    Serial.println("System Ready (UNO). Watchdog Active (500ms). Waiting for CAN...");
}

// ==================================================
// Main loop 
// ==================================================
void loop()
{
    // ==========================================
    // 4. "ให้อาหารหมา" (Feed the Dog)
    // ==========================================
    wdt_reset(); 
    
    unsigned long currentTime = millis();

    // ตรวจรับข้อความจาก Interrupt
    if (isDataReady) 
    {
        isDataReady = false; 
        
        while (CAN0.checkReceive() == CAN_MSGAVAIL) 
        {
            unsigned long receivedId;
            byte dataLength;
            byte rxData[8];
            
            CAN0.readMsgBuf(&receivedId, &dataLength, rxData);

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
    }

    // ขยับ Servo
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

    // Fail-Safe: สัญญาณขาดหายเกิน 1 วินาที
    if (lastArmStatus != -1 && (currentTime - lastArmMessageTime > CAN_SIGNAL_TIMEOUT))
    {
        lastArmStatus = -1;
        Serial.println("WARNING: CAN Signal Lost! Arm safely stopped.");
    }

    // Sleep Mode: ปล่อยจอยสติ๊กทิ้งไว้เกิน 30 วินาที
    if (!sleepMode && (currentTime - lastActiveTime > SLEEP_TIMEOUT))
    {
        sleepMode = true;
        lastArmStatus = -1; 
        
        digitalWrite(RELAY_PUMP_PIN, RELAY_OFF_STATE); 
        pumpState = false;
        
        Serial.println("SLEEP MODE: Inactive for 30 seconds. System halted.");
    }
}