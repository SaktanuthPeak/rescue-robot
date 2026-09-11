#include <SPI.h>
#include <mcp_can.h>
#include "robot_config.h"
#include "encoder.h"
#include "motor.h"

// ---------------- CAN configuration ----------------
MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- สถานะการควบคุมมอเตอร์ ----------------
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
    SPIN_LEFT = 11,
    SPIN_RIGHT = 12
};

unsigned long lastMotorMessageTime = 0;
bool motorCANAlive = false;
int lastMotorStatus = -1;

// รอบเวลาสำหรับการคำนวณ PID และส่ง Telemetry
unsigned long lastControlLoopMs = 0;
const unsigned long CONTROL_LOOP_INTERVAL_MS = 20; // 50 Hz

unsigned long lastTelemetryMs = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 100;   // 10 Hz

// ==================================================
// แปลงคำสั่งจาก CAN Bus (รีโมท) ไปยังการเคลื่อนที่ล้อ Mecanum
// ==================================================
void apply_motor_from_status(PS2_Status current)
{
    switch (current)
    {
    case FORWARD:        motor_forward(); break;
    case BACKWARD:       motor_backward(); break;
    case LEFT:           motor_slide_left(); break;
    case RIGHT:          motor_slide_right(); break;
    case FORWARD_LEFT:   motor_forward_left(); break;
    case FORWARD_RIGHT:  motor_forward_right(); break;
    case BACKWARD_LEFT:  motor_backward_left(); break;
    case BACKWARD_RIGHT: motor_backward_right(); break;
    case SPIN_LEFT:      motor_spin_left(); break;
    case SPIN_RIGHT:     motor_spin_right(); break;
    case STOP:
    default:             motor_stop(); break;
    }
}

// ตรวจสอบความถูกต้องของข้อมูล Motor Status
bool is_valid_motor_status(byte value)
{
    return (value <= BACKWARD_RIGHT) || (value == SPIN_LEFT) || (value == SPIN_RIGHT);
}

// ==================================================
// ประมวลผลข้อความจาก CAN Bus
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData)
{
    if (dataLength < 1) return;

    // รับเฉพาะคำสั่งสำหรับมอเตอร์ขับเคลื่อนล้อ
    if (receivedId == CAN_ID_MOTOR)
    {
        byte receivedStatus = rxData[0];

        if (!is_valid_motor_status(receivedStatus))
        {
            motor_stop();
            return;
        }

        lastMotorMessageTime = millis();
        motorCANAlive = true;

        if (receivedStatus != lastMotorStatus)
        {
            lastMotorStatus = receivedStatus;
            apply_motor_from_status((PS2_Status)receivedStatus);
        }
    }
}

// ==================================================
// ส่งข้อมูล Telemetry & Encoder กลับผ่าน CAN Bus และ Serial
// ==================================================
void send_telemetry()
{
    // จัดเตรียมข้อมูลส่งกลับทาง CAN Bus (CAN_ID_TELEMETRY: 0x102)
    int16_t spd_fl = (int16_t)encoder_get_speed_fl();
    int16_t spd_fr = (int16_t)encoder_get_speed_fr();
    int16_t spd_bl = (int16_t)encoder_get_speed_bl();
    int16_t spd_br = (int16_t)encoder_get_speed_br();

    byte canTx[8];
    canTx[0] = (byte)(lastMotorStatus >= 0 ? lastMotorStatus : 0);
    canTx[1] = motorCANAlive ? 1 : 0;
    canTx[2] = (byte)((spd_fl >> 8) & 0xFF);
    canTx[3] = (byte)(spd_fl & 0xFF);
    canTx[4] = (byte)((spd_fr >> 8) & 0xFF);
    canTx[5] = (byte)(spd_fr & 0xFF);
    canTx[6] = (byte)((spd_bl >> 8) & 0xFF);
    canTx[7] = (byte)(spd_bl & 0xFF);

    CAN0.sendMsgBuf(CAN_ID_TELEMETRY, 0, 8, canTx);

    // พิมพ์ค่าตรวจสอบทาง Serial Monitor
    Serial.print("M: ");
    Serial.print(lastMotorStatus);
    Serial.print(" | Enc Ticks [FL,FR,BL,BR]: ");
    Serial.print(encoder_get_ticks_fl()); Serial.print(", ");
    Serial.print(encoder_get_ticks_fr()); Serial.print(", ");
    Serial.print(encoder_get_ticks_bl()); Serial.print(", ");
    Serial.print(encoder_get_ticks_br());
    Serial.print(" | Speeds: ");
    Serial.print(spd_fl); Serial.print(", ");
    Serial.print(spd_fr); Serial.print(", ");
    Serial.print(spd_bl); Serial.print(", ");
    Serial.println(spd_br);
}

// ==================================================
// Setup
// ==================================================
void setup()
{
    Serial.begin(115200);

    // 1. เริ่มต้นระบบมอเตอร์ขับเคลื่อนล้อ
    motor_init();

    // 2. เริ่มต้นระบบตัวนับพัลส์ Encoder (Hardware Interrupts)
    encoder_init();

    // 3. เริ่มต้นระบบ MCP2515 CAN Bus
    Serial.println("Initializing MCP2515 (Motor & Encoder Controller)...");
    while (CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK_SET) != CAN_OK)
    {
        Serial.println("MCP2515 initialization failed. Retrying...");
        delay(1000);
    }
    CAN0.setMode(MCP_NORMAL);

    Serial.println("MCP2515 Ready. Mecanum Closed-Loop Motor Controller active.");

    motor_stop();
}

// ==================================================
// Main loop
// ==================================================
void loop()
{
    // 1. รับข้อความจาก CAN Bus (ตรวจจับผ่าน SPI flag ไม่ชนกับขา Interrupt ของ Encoder)
    while (CAN0.checkReceive() == CAN_MSGAVAIL)
    {
        unsigned long receivedId = 0;
        byte dataLength = 0;
        byte rxData[8];

        byte result = CAN0.readMsgBuf(&receivedId, &dataLength, rxData);
        if (result == CAN_OK)
        {
            process_can_message(receivedId, dataLength, rxData);
        }
    }

    unsigned long currentTime = millis();

    // 2. ลูปควบคุมความเร็ววงรอบปิด (Closed-Loop PID) ความถี่ 50 Hz
    if (currentTime - lastControlLoopMs >= CONTROL_LOOP_INTERVAL_MS)
    {
        float dt = (float)(currentTime - lastControlLoopMs) / 1000.0f;
        lastControlLoopMs = currentTime;

        // คำนวณความเร็วจริงของล้อจาก Encoder
        encoder_update_speeds(dt);

        // คำนวณ PID และส่ง PWM ไปยังมอเตอร์ L298N ทั้ง 4 ล้อ
        motor_update_pid(dt);
    }

    // 3. Fail-safe Watchdog: หากขาดการเชื่อมต่อ CAN เกินเวลา CAN_TIMEOUT (300 ms) ให้หยุดล้อทันที
    if (motorCANAlive && (currentTime - lastMotorMessageTime > CAN_TIMEOUT))
    {
        motorCANAlive = false;
        lastMotorStatus = -1;
        motor_stop();
        Serial.println("WARNING: Motor CAN timeout - Fail-safe Stop");
    }

    // 4. ส่งข้อมูล Telemetry & Encoder ทุก 100 ms
    if (currentTime - lastTelemetryMs >= TELEMETRY_INTERVAL_MS)
    {
        lastTelemetryMs = currentTime;
        send_telemetry();
    }
}
