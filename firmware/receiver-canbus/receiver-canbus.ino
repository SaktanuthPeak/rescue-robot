#include <SPI.h>
#include <mcp_can.h>
#include <U8g2lib.h>
#include <stdio.h>

// =====================================================
// CAN BUS
// =====================================================

const byte CAN_CS_PIN = 10;
const byte CAN_INT_PIN = 2;

const unsigned long CAN_ID_MOTOR = 0x100;
const unsigned long CAN_ID_ARM = 0x101;

const unsigned long CAN_TIMEOUT_MS = 300;
const unsigned long SERIAL_COMMAND_TIMEOUT_MS = 400;

MCP_CAN CAN0(CAN_CS_PIN);

// =====================================================
// OLED SOFTWARE I2C
//
// OLED SDA -> Arduino D6
// OLED SCL -> Arduino D7
// =====================================================

const byte OLED_SDA_PIN = 6;
const byte OLED_SCL_PIN = 7;

const unsigned long OLED_INTERVAL_MS = 250;
unsigned long lastOledTime = 0;

// ใช้ Page Buffer เพื่อลดการใช้ RAM ของ Arduino Uno
U8G2_SSD1306_128X64_NONAME_1_SW_I2C oled(
  U8G2_R0,
  OLED_SCL_PIN,
  OLED_SDA_PIN,
  U8X8_PIN_NONE);

// ถ้าจอเป็น SH1106 ให้ใช้บรรทัดนี้แทน:
//
// U8G2_SH1106_128X64_NONAME_1_SW_I2C oled(
//     U8G2_R0,
//     OLED_SCL_PIN,
//     OLED_SDA_PIN,
//     U8X8_PIN_NONE
// );

// =====================================================
// VOLTAGE SENSOR — A0
// =====================================================

const byte VOLTAGE_SENSOR_PIN = A0;

const float VOLTAGE_R1 = 30000.0;
const float VOLTAGE_R2 = 7500.0;
const float ADC_REFERENCE_VOLTAGE = 5.0;

int voltageAdcValue = 0;
float adcVoltage = 0.0;
float inputVoltage = 0.0;

// =====================================================
// IR PROXIMITY SENSOR — A1 ถึง A4
// =====================================================

const byte IR_SENSOR_COUNT = 4;

const byte IR_SENSOR_PINS[IR_SENSOR_COUNT] = {
  A1,
  A2,
  A3,
  A4
};

int irAdcValue[IR_SENSOR_COUNT] = {
  0,
  0,
  0,
  0
};

// =====================================================
// TIMING
// =====================================================

const unsigned long SENSOR_INTERVAL_MS = 100;

unsigned long lastSensorTime = 0;
unsigned long telemetrySequence = 0;

// =====================================================
// STATUS
// ต้องเหมือนกับ Arduino ตัวส่ง
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

int lastMotorStatus = -1;
int lastArmStatus = -1;

bool motorCanActive = false;
bool armCanActive = false;

unsigned long lastMotorMessageTime = 0;
unsigned long lastArmMessageTime = 0;

// USB serial command state from Raspberry Pi
bool serialMotorActive = false;
bool serialArmActive = false;
unsigned long lastSerialMotorCommandTime = 0;
unsigned long lastSerialArmCommandTime = 0;

// =====================================================
// MOTOR FUNCTIONS
// ใส่คำสั่งควบคุมมอเตอร์จริงแทน Serial.println()
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
// ARM FUNCTIONS
// ใส่คำสั่งควบคุมแขนจริงแทน Serial.println()
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
// APPLY STATUS
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
// USB COMMANDS FROM RASPBERRY PI
// CMD:MOTOR:<code>, CMD:ARM:<code>, CMD:ALL:0
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

    serialMotorActive = false;
    serialArmActive = false;
    lastMotorStatus = STOP;
    lastArmStatus = STOP;
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
    // Gripper actions are one-shot; only arm movement uses the heartbeat timeout.
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

// =====================================================
// SENSOR FUNCTIONS
// =====================================================

void read_voltage_sensor() {
  voltageAdcValue = analogRead(VOLTAGE_SENSOR_PIN);

  adcVoltage =
    (voltageAdcValue * ADC_REFERENCE_VOLTAGE) / 1024.0;

  inputVoltage =
    adcVoltage * (VOLTAGE_R1 + VOLTAGE_R2) / VOLTAGE_R2;
}

void read_ir_sensors() {
  for (byte i = 0; i < IR_SENSOR_COUNT; i++) {
    irAdcValue[i] =
      analogRead(IR_SENSOR_PINS[i]);
  }
}

// =====================================================
// CAN MESSAGE
// =====================================================

void process_can_message(
  unsigned long canId,
  byte dataLength,
  byte *receivedData) {
  if (dataLength < 1) {
    return;
  }

  byte receivedStatus = receivedData[0];

  // Motor command
  if (canId == CAN_ID_MOTOR) {
    if (receivedStatus > BACKWARD_RIGHT) {
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

  // Arm command
  else if (canId == CAN_ID_ARM) {
    if (receivedStatus > Clamp) {
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

void read_can_bus() {
  while (CAN0.checkReceive() == CAN_MSGAVAIL) {
    unsigned long receivedId = 0;
    byte dataLength = 0;
    byte receivedData[8];

    byte result = CAN0.readMsgBuf(
      &receivedId,
      &dataLength,
      receivedData);

    if (result == CAN_OK) {
      process_can_message(
        receivedId,
        dataLength,
        receivedData);
    }
  }
}

// =====================================================
// CAN TIMEOUT
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
// USB COMMAND TIMEOUT
// =====================================================

void check_serial_command_timeout() {
  unsigned long currentTime = millis();

  // CAN heartbeat has priority over web/USB commands.
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
// STATUS TEXT FOR OLED
// =====================================================

const char *get_status_text(int status) {
  switch (status) {
    case STOP:
      return "STOP";

    case FORWARD:
      return "FWD";

    case BACKWARD:
      return "BACK";

    case LEFT:
      return "LEFT";

    case RIGHT:
      return "RIGHT";

    case FORWARD_LEFT:
      return "FWD-L";

    case FORWARD_RIGHT:
      return "FWD-R";

    case BACKWARD_LEFT:
      return "BACK-L";

    case BACKWARD_RIGHT:
      return "BACK-R";

    case Release:
      return "RELEASE";

    case Clamp:
      return "CLAMP";

    default:
      return "NO DATA";
  }
}

// =====================================================
// OLED DISPLAY
// =====================================================

void update_oled() {
  oled.firstPage();

  do {
    oled.setFont(u8g2_font_6x10_tf);

    // บรรทัด 1: แรงดัน
    oled.setCursor(0, 10);
    oled.print("V: ");
    oled.print(inputVoltage, 2);
    oled.print(" V");

    // บรรทัด 2: IR A1 และ A2
    oled.setCursor(0, 23);
    oled.print("F1:");
    oled.print(irAdcValue[0]);

    oled.print(" F2:");
    oled.print(irAdcValue[1]);

    // บรรทัด 3: IR A3 และ A4
    oled.setCursor(0, 36);
    oled.print("F3:");
    oled.print(irAdcValue[2]);

    oled.print(" F4:");
    oled.print(irAdcValue[3]);

    // บรรทัด 4: Motor
    oled.setCursor(0, 49);
    oled.print("M:");
    oled.print(get_status_text(lastMotorStatus));

    oled.print(
      motorCanActive ? " OK" : " TIMEOUT");

    // บรรทัด 5: Arm
    oled.setCursor(0, 62);
    oled.print("A:");
    oled.print(get_status_text(lastArmStatus));

    oled.print(
      armCanActive ? " OK" : " TIMEOUT");
  } while (oled.nextPage());
}

// =====================================================
// CHECKSUM AND USB TELEMETRY
// =====================================================

byte calculate_xor_checksum(const char *text) {
  byte checksum = 0;

  while (*text != '\0') {
    checksum ^= static_cast<byte>(*text);
    text++;
  }

  return checksum;
}

void send_usb_telemetry() {
  char payload[128];

  // Keep the RB3 field positions wire-compatible while the backend migrates its
  // legacy field names to the new IR proximity terminology.

  unsigned long voltageMillivolts =
    static_cast<unsigned long>(
      (inputVoltage * 1000.0) + 0.5);

  snprintf(
    payload,
    sizeof(payload),
    "RB3,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%lu",
    lastMotorStatus,
    motorCanActive ? 1 : 0,
    lastArmStatus,
    armCanActive ? 1 : 0,
    voltageMillivolts,
    voltageAdcValue,
    irAdcValue[0],
    irAdcValue[1],
    irAdcValue[2],
    irAdcValue[3],
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
  pinMode(VOLTAGE_SENSOR_PIN, INPUT);

  for (byte i = 0; i < IR_SENSOR_COUNT; i++) {
    pinMode(IR_SENSOR_PINS[i], INPUT);
  }

  // เริ่ม OLED
  oled.begin();

  oled.firstPage();

  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(18, 28, "DURIAN BOT");
    oled.drawStr(16, 44, "Starting...");
  } while (oled.nextPage());

  motor_stop();
  arm_stop();

  Serial.println("Initializing MCP2515...");

  // เปลี่ยนเป็น MCP_16MHZ ถ้า Crystal เขียน 16.000
  while (
    CAN0.begin(
      MCP_ANY,
      CAN_500KBPS,
      MCP_8MHZ)
    != CAN_OK) {
    Serial.println("MCP2515 initialization failed");
    delay(1000);
  }

  CAN0.setMode(MCP_NORMAL);

  Serial.println("CAN receiver ready");

  read_voltage_sensor();
  read_ir_sensors();

  lastSensorTime = millis();
  lastOledTime = millis();

  update_oled();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // รับคำสั่งจากหน้าเว็บผ่าน Raspberry Pi -> USB Serial
  process_serial_commands();

  // รับ CAN ตลอดเวลา
  read_can_bus();

  // ตรวจ CAN Timeout
  check_can_timeout();
  check_serial_command_timeout();

  unsigned long currentTime = millis();

  // อ่าน Sensor และส่ง Raspberry Pi ทุก 100 ms
  if (
    currentTime - lastSensorTime >= SENSOR_INTERVAL_MS) {
    lastSensorTime = currentTime;

    read_voltage_sensor();
    read_ir_sensors();
    send_usb_telemetry();
  }

  // อัปเดต OLED ทุก 250 ms
  if (
    currentTime - lastOledTime >= OLED_INTERVAL_MS) {
    lastOledTime = currentTime;

    update_oled();
  }
}
