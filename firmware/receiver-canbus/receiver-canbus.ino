#include <SPI.h>
#include <mcp_can.h>
#include <stdio.h>

// =====================================================
// CAN CONFIGURATION
// =====================================================

const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2;

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM = 0x101;

const unsigned long CAN_TIMEOUT_MS = 300;
const unsigned long TELEMETRY_INTERVAL_MS = 100;
const unsigned long SERIAL_COMMAND_TIMEOUT_MS = 400;

MCP_CAN CAN0(CAN_CS_PIN);

// =====================================================
// VOLTAGE SENSOR CONFIGURATION
// อ้างอิงค่าจาก Last Minute Engineers
// =====================================================

#define ANALOG_IN_PIN A0

const float R1 = 30000.0;
const float R2 = 7500.0;
const float REF_VOLTAGE = 5.0;

int voltageAdcValue = 0;
float adcVoltage = 0.0;
float inputVoltage = 0.0;

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
// CAN STATE
// =====================================================

int lastMotorStatus = -1;
int lastArmStatus = -1;

bool motorCanActive = false;
bool armCanActive = false;

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;

bool serialMotorActive = false;
bool serialArmActive = false;
unsigned long lastSerialMotorCommandTime = 0;
unsigned long lastSerialArmCommandTime = 0;

// =====================================================
// TELEMETRY STATE
// =====================================================

unsigned long telemetrySequence = 0;
unsigned long lastTelemetryTime = 0;

// =====================================================
// ฟังก์ชันควบคุมมอเตอร์
//
// นำโค้ดควบคุมมอเตอร์จริงมาใส่ในฟังก์ชันเหล่านี้
// =====================================================

void motor_forward() {
  Serial.println("MOTOR: FORWARD");
}

void motor_backward() {
  Serial.println("MOTOR: BACKWARD");
}

void motor_slide_left() {
  Serial.println("MOTOR: SLIDE LEFT");
}

void motor_slide_right() {
  Serial.println("MOTOR: SLIDE RIGHT");
}

void motor_forward_left() {
  Serial.println("MOTOR: FORWARD LEFT");
}

void motor_forward_right() {
  Serial.println("MOTOR: FORWARD RIGHT");
}

void motor_backward_left() {
  Serial.println("MOTOR: BACKWARD LEFT");
}

void motor_backward_right() {
  Serial.println("MOTOR: BACKWARD RIGHT");
}

void motor_stop() {
  Serial.println("MOTOR: STOP");
}

// =====================================================
// ฟังก์ชันควบคุมแขน
//
// นำโค้ดควบคุมแขนจริงมาใส่ในฟังก์ชันเหล่านี้
// =====================================================

void arm_forward() {
  Serial.println("ARM: FORWARD");
}

void arm_backward() {
  Serial.println("ARM: BACKWARD");
}

void arm_turn_left() {
  Serial.println("ARM: TURN LEFT");
}

void arm_turn_right() {
  Serial.println("ARM: TURN RIGHT");
}

void arm_stop() {
  Serial.println("ARM: STOP");
}

void gripper_release() {
  Serial.println("GRIPPER: RELEASE");
}

void gripper_clamp() {
  Serial.println("GRIPPER: CLAMP");
}

// =====================================================
// APPLY MOTOR STATUS
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
// APPLY ARM STATUS
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
// VALIDATE STATUS
// =====================================================

bool is_valid_motor_status(byte value) {
  return value <= BACKWARD_RIGHT;
}

bool is_valid_arm_status(byte value) {
  return value <= Clamp;
}

// =====================================================
// READ VOLTAGE SENSOR
// สูตรจากเว็บที่ให้มา
// =====================================================

void read_voltage_sensor() {
  // อ่าน ADC จากขา A0
  voltageAdcValue = analogRead(ANALOG_IN_PIN);

  // แรงดันที่ขา ADC
  adcVoltage =
    (voltageAdcValue * REF_VOLTAGE) / 1024.0;

  // คำนวณแรงดันก่อนผ่าน Voltage Divider
  inputVoltage =
    adcVoltage * (R1 + R2) / R2;
}

// =====================================================
// PROCESS CAN MESSAGE
// =====================================================

// =====================================================
// PROCESS COMMAND FROM RASPBERRY PI
// รูปแบบคำสั่ง: CMD:MOTOR:<code>, CMD:ARM:<code>, CMD:ALL:0
// =====================================================

void process_serial_command(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  if (line.startsWith("CMD:ALL:")) {
    int code = line.substring(8).toInt();
    if (code != STOP) {
      Serial.println("CMD ERROR: ALL only supports STOP");
      return;
    }

    lastMotorStatus = STOP;
    lastArmStatus = STOP;
    serialMotorActive = false;
    serialArmActive = false;
    apply_motor_from_status(STOP);
    apply_arm_from_status(STOP);
    Serial.println("CMD ACK: ALL STOP");
    return;
  }

  if (line.startsWith("CMD:MOTOR:")) {
    int code = line.substring(10).toInt();
    if (code < STOP || code > BACKWARD_RIGHT) {
      Serial.println("CMD ERROR: Invalid motor status");
      return;
    }

    lastMotorStatus = code;
    serialMotorActive = code != STOP;
    lastSerialMotorCommandTime = millis();
    apply_motor_from_status(static_cast<PS2_Status>(code));
    Serial.println("CMD ACK: MOTOR");
    return;
  }

  if (line.startsWith("CMD:ARM:")) {
    int code = line.substring(8).toInt();
    if (code < STOP || code > Clamp) {
      Serial.println("CMD ERROR: Invalid arm status");
      return;
    }

    lastArmStatus = code;
    serialArmActive = code >= FORWARD && code <= BACKWARD_RIGHT;
    lastSerialArmCommandTime = millis();
    apply_arm_from_status(static_cast<PS2_Status>(code));
    Serial.println("CMD ACK: ARM");
    return;
  }

  Serial.println("CMD ERROR: Unknown command");
}

void process_serial_commands() {
  static String line;

  while (Serial.available() > 0) {
    char character = static_cast<char>(Serial.read());

    if (character == '\n') {
      process_serial_command(line);
      line = "";
    } else if (character != '\r') {
      line += character;
      if (line.length() > 32) {
        line = "";
      }
    }
  }
}

void process_can_message(
  unsigned long canId,
  byte dataLength,
  byte *receivedData) {
  if (dataLength < 1) {
    Serial.println("CAN ERROR: Empty frame");
    return;
  }

  byte receivedStatus = receivedData[0];

  // -------------------------------------------------
  // Motor
  // -------------------------------------------------

  if (canId == CAN_ID_MOTOR) {
    if (!is_valid_motor_status(receivedStatus)) {
      Serial.println("CAN ERROR: Invalid motor status");

      motor_stop();
      motorCanActive = false;
      lastMotorStatus = -1;

      return;
    }

    lastMotorMessageTime = millis();
    motorCanActive = true;

    if (receivedStatus != lastMotorStatus) {
      lastMotorStatus = receivedStatus;

      apply_motor_from_status(
        static_cast<PS2_Status>(receivedStatus));
    }
  }

  // -------------------------------------------------
  // Arm
  // -------------------------------------------------

  else if (canId == CAN_ID_ARM) {
    if (!is_valid_arm_status(receivedStatus)) {
      Serial.println("CAN ERROR: Invalid arm status");

      arm_stop();
      armCanActive = false;
      lastArmStatus = -1;

      return;
    }

    lastArmMessageTime = millis();
    armCanActive = true;

    if (receivedStatus != lastArmStatus) {
      lastArmStatus = receivedStatus;

      apply_arm_from_status(
        static_cast<PS2_Status>(receivedStatus));
    }
  }
}

// =====================================================
// CHECK CAN TIMEOUT
// =====================================================

void check_can_timeout() {
  unsigned long currentTime = millis();

  if (
    motorCanActive && currentTime - lastMotorMessageTime > CAN_TIMEOUT_MS) {
    motorCanActive = false;
    lastMotorStatus = -1;

    motor_stop();

    Serial.println("CAN WARNING: Motor timeout");
  }

  if (
    armCanActive && currentTime - lastArmMessageTime > CAN_TIMEOUT_MS) {
    armCanActive = false;
    lastArmStatus = -1;

    arm_stop();

    Serial.println("CAN WARNING: Arm timeout");
  }
}

// =====================================================
// SERIAL COMMAND TIMEOUT
// =====================================================

void check_serial_command_timeout() {
  unsigned long currentTime = millis();

  // ถ้า CAN ยังมี heartbeat ให้ CAN เป็นแหล่งควบคุมหลัก
  if (
    serialMotorActive && !motorCanActive &&
    currentTime - lastSerialMotorCommandTime > SERIAL_COMMAND_TIMEOUT_MS) {
    serialMotorActive = false;
    lastMotorStatus = STOP;
    motor_stop();
    Serial.println("CMD WARNING: Motor serial timeout");
  }

  if (
    serialArmActive && !armCanActive &&
    currentTime - lastSerialArmCommandTime > SERIAL_COMMAND_TIMEOUT_MS) {
    serialArmActive = false;
    lastArmStatus = STOP;
    arm_stop();
    Serial.println("CMD WARNING: Arm serial timeout");
  }
}

// =====================================================
// XOR CHECKSUM
// =====================================================

byte calculate_xor_checksum(const char *text) {
  byte checksum = 0;

  while (*text != '\0') {
    checksum ^= static_cast<byte>(*text);
    text++;
  }

  return checksum;
}

// =====================================================
// SEND USB TELEMETRY
//
// รูปแบบใหม่:
// RB2,motor_code,motor_alive,arm_code,arm_alive,
// voltage_mV,adc_value,sequence*CK
//
// ตัวอย่าง:
// RB2,1,1,0,1,12450,510,25*AB
// =====================================================

void send_usb_telemetry() {
  char payload[90];

  // ส่งเป็น millivolt เพื่อหลีกเลี่ยงปัญหา %f บน Arduino Uno
  unsigned long voltageMillivolts =
    static_cast<unsigned long>(
      (inputVoltage * 1000.0) + 0.5);

  snprintf(
    payload,
    sizeof(payload),
    "RB2,%d,%d,%d,%d,%lu,%d,%lu",
    lastMotorStatus,
    motorCanActive ? 1 : 0,
    lastArmStatus,
    armCanActive ? 1 : 0,
    voltageMillivolts,
    voltageAdcValue,
    telemetrySequence);

  byte checksum = calculate_xor_checksum(payload);

  Serial.print(payload);
  Serial.print('*');

  if (checksum < 0x10) {
    Serial.print('0');
  }

  Serial.println(checksum, HEX);

  telemetrySequence++;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(CAN_INT_PIN, INPUT);
  pinMode(ANALOG_IN_PIN, INPUT);

  Serial.println();
  Serial.println("CAN + Voltage receiver starting...");

  motor_stop();
  arm_stop();

  // ถ้า Crystal เขียนว่า 16.000
  // เปลี่ยน MCP_8MHZ เป็น MCP_16MHZ
  while (
    CAN0.begin(
      MCP_ANY,
      CAN_500KBPS,
      MCP_8MHZ)
    != CAN_OK) {
    Serial.println("MCP2515 initialization failed");
    Serial.println("Check wiring and crystal");

    delay(1000);
  }

  CAN0.setMode(MCP_NORMAL);

  Serial.println("MCP2515 initialized");
  Serial.println("CAN receiver ready");

  read_voltage_sensor();

  lastTelemetryTime = millis();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // รับคำสั่งจากหน้าเว็บผ่าน Raspberry Pi → USB Serial
  process_serial_commands();

  // -------------------------------------------------
  // อ่านข้อความ CAN ที่ค้างอยู่ทั้งหมด
  // -------------------------------------------------

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
      Serial.println("CAN ERROR: Cannot read message");
    }
  }

  check_can_timeout();
  check_serial_command_timeout();

  // -------------------------------------------------
  // อ่านแรงดันและส่งไป Raspberry Pi ทุก 100 ms
  // -------------------------------------------------

  unsigned long currentTime = millis();

  if (
    currentTime - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = currentTime;

    read_voltage_sensor();
    send_usb_telemetry();
  }
}
