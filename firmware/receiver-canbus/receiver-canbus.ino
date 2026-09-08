#include <SPI.h>
#include <mcp_can.h>
#include <stdio.h>

// =====================================================
// CAN BUS CONFIGURATION
// =====================================================

const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2;

// CAN ID ต้องตรงกับ Arduino ตัวส่ง
const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM = 0x101;

// หากไม่ได้รับข้อมูลเกิน 300 ms ให้หยุด
const unsigned long CAN_TIMEOUT_MS = 300;

// ส่ง snapshot สถานะผ่าน USB Serial ให้ Raspberry Pi ทุก 100 ms
const unsigned long SERIAL_TELEMETRY_PERIOD_MS = 100;

MCP_CAN CAN0(CAN_CS_PIN);

// =====================================================
// STATUS
// ต้องเหมือนกันทั้ง Arduino ตัวส่งและตัวรับ
// =====================================================

enum PS2_Status : uint8_t {
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

// =====================================================
// ตัวแปรสำหรับตรวจสอบสถานะ CAN
// =====================================================

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;

bool motorCanActive = false;
bool armCanActive = false;

int lastMotorStatus = -1;
int lastArmStatus = -1;

unsigned long lastSerialTelemetryTime = 0;
unsigned long serialTelemetrySequence = 0;

// =====================================================
// ฟังก์ชันมอเตอร์รถ
//
// ตอนนี้แสดงผลผ่าน Serial เพื่อทดสอบ CAN
// ให้นำคำสั่ง digitalWrite/analogWrite ของจริงมาใส่แทน
// =====================================================

void motor_forward() {
  Serial.println("MOTOR -> FORWARD");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_backward() {
  Serial.println("MOTOR -> BACKWARD");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_slide_left() {
  Serial.println("MOTOR -> SLIDE LEFT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_slide_right() {
  Serial.println("MOTOR -> SLIDE RIGHT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_forward_left() {
  Serial.println("MOTOR -> FORWARD LEFT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_forward_right() {
  Serial.println("MOTOR -> FORWARD RIGHT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_backward_left() {
  Serial.println("MOTOR -> BACKWARD LEFT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_backward_right() {
  Serial.println("MOTOR -> BACKWARD RIGHT");

  // ใส่โค้ดควบคุมมอเตอร์จริงตรงนี้
}

void motor_stop() {
  Serial.println("MOTOR -> STOP");

  // ใส่โค้ดหยุดมอเตอร์จริงตรงนี้
}

// =====================================================
// ฟังก์ชันแขนกล
// =====================================================

void arm_forward() {
  Serial.println("ARM -> FORWARD");

  // ใส่โค้ดควบคุมแขนจริงตรงนี้
}

void arm_backward() {
  Serial.println("ARM -> BACKWARD");

  // ใส่โค้ดควบคุมแขนจริงตรงนี้
}

void arm_turn_left() {
  Serial.println("ARM -> TURN LEFT");

  // ใส่โค้ดควบคุมแขนจริงตรงนี้
}

void arm_turn_right() {
  Serial.println("ARM -> TURN RIGHT");

  // ใส่โค้ดควบคุมแขนจริงตรงนี้
}

void arm_stop() {
  Serial.println("ARM -> STOP");

  // ใส่โค้ดหยุดแขนจริงตรงนี้
}

void gripper_release() {
  Serial.println("GRIPPER -> RELEASE");

  // ใส่โค้ดปล่อย Gripper จริงตรงนี้
}

void gripper_clamp() {
  Serial.println("GRIPPER -> CLAMP");

  // ใส่โค้ดหนีบ Gripper จริงตรงนี้
}

// =====================================================
// นำสถานะไปควบคุมมอเตอร์
// =====================================================

void apply_motor_from_status(PS2_Status current) {
  switch (current) {
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

// =====================================================
// นำสถานะไปควบคุมแขน
// =====================================================

void apply_arm_from_status(PS2_Status current) {
  switch (current) {
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

// =====================================================
// ตรวจสอบสถานะมอเตอร์
// =====================================================

bool is_valid_motor_status(byte value) {
  switch (value) {
    case STOP:
    case FORWARD:
    case BACKWARD:
    case LEFT:
    case RIGHT:
    case FORWARD_LEFT:
    case FORWARD_RIGHT:
    case BACKWARD_LEFT:
    case BACKWARD_RIGHT:
      return true;

    default:
      return false;
  }
}

// =====================================================
// ตรวจสอบสถานะแขน
// =====================================================

bool is_valid_arm_status(byte value) {
  switch (value) {
    case STOP:
    case FORWARD:
    case BACKWARD:
    case LEFT:
    case RIGHT:
    case FORWARD_LEFT:
    case FORWARD_RIGHT:
    case BACKWARD_LEFT:
    case BACKWARD_RIGHT:
    case Release:
    case Clamp:
      return true;

    default:
      return false;
  }
}

// =====================================================
// Telemetry ไป Raspberry Pi ผ่าน USB Serial
// รูปแบบ: RB1,motor_code,motor_alive,arm_code,arm_alive,seq*CK\n
// CK คือ XOR ของทุกตัวอักษรก่อนเครื่องหมาย *
// =====================================================

void emit_serial_telemetry() {
  unsigned long now = millis();

  if (now - lastSerialTelemetryTime < SERIAL_TELEMETRY_PERIOD_MS) {
    return;
  }
  lastSerialTelemetryTime = now;

  char payload[96];
  snprintf(
    payload,
    sizeof(payload),
    "RB1,%d,%d,%d,%d,%lu",
    lastMotorStatus,
    motorCanActive ? 1 : 0,
    lastArmStatus,
    armCanActive ? 1 : 0,
    serialTelemetrySequence++
  );

  byte checksum = 0;
  for (size_t i = 0; payload[i] != '\0'; i++) {
    checksum ^= static_cast<byte>(payload[i]);
  }

  Serial.print(payload);
  Serial.print('*');
  if (checksum < 0x10) {
    Serial.print('0');
  }
  Serial.println(checksum, HEX);
}

// =====================================================
// ประมวลผลข้อความ CAN
// =====================================================

void process_can_message(
  unsigned long canId,
  byte dataLength,
  byte *receivedData) {
  // ในระบบนี้ต้องมีข้อมูลอย่างน้อย 1 byte
  if (dataLength < 1) {
    Serial.println("ERROR -> Empty CAN frame");
    return;
  }

  byte receivedStatus = receivedData[0];

  // Serial.print("Received ID: 0x");
  // Serial.print(canId, HEX);

  // Serial.print(" | Status byte: ");
  // Serial.println(receivedStatus);

  // -------------------------------------------------
  // CAN ID สำหรับมอเตอร์
  // -------------------------------------------------

  if (canId == CAN_ID_MOTOR) {
    if (!is_valid_motor_status(receivedStatus)) {
      Serial.println("ERROR -> Invalid motor status");

      motor_stop();

      motorCanActive = false;
      lastMotorStatus = -1;

      return;
    }

    // อัปเดตเวลา แม้ว่าสถานะยังเหมือนเดิม
    lastMotorMessageTime = millis();
    motorCanActive = true;

    // เรียกฟังก์ชันเมื่อสถานะเปลี่ยนเท่านั้น
    if (receivedStatus != lastMotorStatus) {
      lastMotorStatus = receivedStatus;

      PS2_Status currentStatus =
        static_cast<PS2_Status>(receivedStatus);

      apply_motor_from_status(currentStatus);
    }
  }

  // -------------------------------------------------
  // CAN ID สำหรับแขน
  // -------------------------------------------------

  else if (canId == CAN_ID_ARM) {
    if (!is_valid_arm_status(receivedStatus)) {
      Serial.println("ERROR -> Invalid arm status");

      arm_stop();

      armCanActive = false;
      lastArmStatus = -1;

      return;
    }

    // อัปเดตเวลา แม้ว่าสถานะยังเหมือนเดิม
    lastArmMessageTime = millis();
    armCanActive = true;

    // เรียกฟังก์ชันเมื่อสถานะเปลี่ยนเท่านั้น
    if (receivedStatus != lastArmStatus) {
      lastArmStatus = receivedStatus;

      PS2_Status currentStatus =
        static_cast<PS2_Status>(receivedStatus);

      apply_arm_from_status(currentStatus);
    }
  }

  // -------------------------------------------------
  // CAN ID ที่ไม่ได้ใช้งาน
  // -------------------------------------------------

  else {
    Serial.println("Unknown CAN ID");
  }
}

// =====================================================
// ตรวจสอบ Timeout
// =====================================================

void check_can_timeout() {
  unsigned long currentTime = millis();

  // มอเตอร์ไม่ได้รับข้อมูลเกิน 300 ms
  if (
    motorCanActive && currentTime - lastMotorMessageTime > CAN_TIMEOUT_MS) {
    motorCanActive = false;
    lastMotorStatus = -1;

    motor_stop();

    Serial.println("WARNING -> Motor CAN timeout");
  }

  // แขนไม่ได้รับข้อมูลเกิน 300 ms
  if (
    armCanActive && currentTime - lastArmMessageTime > CAN_TIMEOUT_MS) {
    armCanActive = false;
    lastArmStatus = -1;

    arm_stop();

    Serial.println("WARNING -> Arm CAN timeout");
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(CAN_INT_PIN, INPUT);

  // เริ่มต้นในสถานะหยุด
  motor_stop();
  arm_stop();

  Serial.println();
  Serial.println("============================");
  Serial.println("CAN BUS RECEIVER");
  Serial.println("============================");
  Serial.println("Initializing MCP2515...");

  // ถ้า Crystal เป็น 16 MHz ให้เปลี่ยน MCP_8MHZ
  // เป็น MCP_16MHZ
  while (
    CAN0.begin(
      MCP_ANY,
      CAN_500KBPS,
      MCP_8MHZ)
    != CAN_OK) {
    Serial.println("MCP2515 initialization failed");
    Serial.println("Retrying in 1 second...");

    delay(1000);
  }

  // Normal Mode สำหรับรับข้อมูลจริงและตอบ ACK
  CAN0.setMode(MCP_NORMAL);

  Serial.println("MCP2515 initialized successfully");
  Serial.println("CAN receiver ready");
  Serial.println("============================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // อ่านข้อความทั้งหมดที่ค้างอยู่ใน MCP2515
  while (CAN0.checkReceive() == CAN_MSGAVAIL) {
    unsigned long receivedId = 0;
    byte dataLength = 0;
    byte receivedData[8];

    byte readResult = CAN0.readMsgBuf(
      &receivedId,
      &dataLength,
      receivedData);

    if (readResult == CAN_OK) {
      process_can_message(
        receivedId,
        dataLength,
        receivedData);
    } else {
      Serial.println("ERROR -> Cannot read CAN message");
    }
  }

  // ตรวจสอบว่าข้อมูลขาดหายหรือไม่
  check_can_timeout();

  // ส่งสถานะล่าสุดให้ Raspberry Pi แม้ไม่มี CAN frame ใหม่
  emit_serial_telemetry();
}
