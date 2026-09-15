#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <EEPROM.h>
#include <SPI.h>
#include <mcp_can.h>
#include "robot_config.h"
#include "encoder.h"
#include "motor.h"
#include "settings.h"
#include "tm1638.h"

// ---------------- CAN configuration ----------------
MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- สถานะการควบคุมมอเตอร์ ----------------
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
  SPIN_LEFT = 11,
  SPIN_RIGHT = 12
};

// ==================================================
// FreeRTOS task communication
// ==================================================
// Queue มีความยาว 1 และเก็บเฉพาะคำสั่งล่าสุด เพื่อไม่ให้คำสั่งเก่าค้างคิว
struct MotorCommand {
  uint8_t status;
  unsigned long receivedAtMs;
  bool valid;
};

QueueHandle_t motorCommandQueue = nullptr;
TaskHandle_t motorControlTaskHandle = nullptr;
TaskHandle_t canTaskHandle = nullptr;

// เขียนโดย MotorControlTask และอ่านโดย CAN task
// ใช้ชนิด 1 byte เพื่อให้การอ่าน/เขียนบน AVR atomic
volatile int8_t lastMotorStatus = -1;
volatile bool motorCANAlive = false;

// Local PID tuning state. This is intentionally independent of CAN so the
// controller can be configured on the bench with only a TM1638 connected.
TM1638 tm1638(TM1638_STB_PIN, TM1638_CLK_PIN, TM1638_DIO_PIN);
PIDSettings pidSettings;
volatile bool localPidTuneMode = false;
bool canReady = false;

namespace {
  uint8_t selectedGain = 0; // 0=Kp, 1=Ki, 2=Kd
  bool pidSettingsDirty = false;
  uint8_t previousTmButtons = 0;

  float *selected_gain_value() {
    if (selectedGain == 0) return &pidSettings.kp;
    if (selectedGain == 1) return &pidSettings.ki;
    return &pidSettings.kd;
  }

  char selected_gain_label() {
    if (selectedGain == 0) return 'P';
    if (selectedGain == 1) return 'I';
    return 'd';
  }

  void render_pid_screen() {
    char screen[9] = "        ";
    const float value = *selected_gain_value();
    const int scaled = (int)(value * 1000.0f + 0.5f);

    // Normal tuning values fit as P0.350 / I0.050 / d0.010.
    if (scaled >= 0 && scaled <= 9999) {
      const uint16_t whole = (uint16_t)(scaled / 1000);
      const uint16_t fraction = (uint16_t)(scaled % 1000);
      screen[0] = selected_gain_label();
      screen[1] = (char)('0' + whole);
      screen[2] = '.';
      screen[3] = (char)('0' + (fraction / 100));
      screen[4] = (char)('0' + ((fraction / 10) % 10));
      screen[5] = (char)('0' + (fraction % 10));
    } else {
      screen[0] = 'E';
      screen[1] = 'r';
      screen[2] = 'r';
    }

    uint8_t ledMask = 0;
    if (localPidTuneMode)
      ledMask |= (uint8_t)(1 << selectedGain);
    if (pidSettingsDirty)
      ledMask |= 0x80; // right-most LED means unsaved changes
    tm1638.displayText(screen, ledMask);
  }

  void apply_pid_settings() {
    motor_set_pid_gains(pidSettings.kp, pidSettings.ki, pidSettings.kd);
  }

  void enter_local_pid_mode() {
    localPidTuneMode = true;
    motorCANAlive = false;
    lastMotorStatus = -1;
    render_pid_screen();
    Serial.println(F("TM1638 PID mode: motors stopped; CAN motion ignored"));
  }

  void exit_local_pid_mode() {
    localPidTuneMode = false;
    motorCANAlive = false;
    lastMotorStatus = -1;
    tm1638.displayText("        ");
    Serial.println(F("TM1638 PID mode: exited; waiting for a fresh CAN command"));
  }

  void adjust_selected_gain(float delta) {
    float *value = selected_gain_value();
    *value += delta;
    if (*value < 0.0f) *value = 0.0f;
    if (*value > 9.999f) *value = 9.999f;
    pidSettingsDirty = true;
    apply_pid_settings();
  }

  void handle_tm_buttons(uint8_t pressed) {
    if (pressed == 0)
      return;

    // S8 toggles local mode. Entering/exiting is deliberately explicit.
    // Standard 8-button TM1638 board: S1 is the left-most button and maps
    // to bit 7; S8 is the right-most button and maps to bit 0.
    constexpr uint8_t BUTTON_S1 = 0x80;
    constexpr uint8_t BUTTON_S2 = 0x40;
    constexpr uint8_t BUTTON_S3 = 0x20;
    constexpr uint8_t BUTTON_S4 = 0x10;
    constexpr uint8_t BUTTON_S5 = 0x08;
    constexpr uint8_t BUTTON_S6 = 0x04;
    constexpr uint8_t BUTTON_S7 = 0x02;
    constexpr uint8_t BUTTON_S8 = 0x01;

    if (pressed & BUTTON_S8) {
      if (localPidTuneMode)
        exit_local_pid_mode();
      else
        enter_local_pid_mode();
      return;
    }

    // Any other key also enters local mode, preventing accidental adjustment
    // while the robot is driving from CAN.
    if (!localPidTuneMode)
      enter_local_pid_mode();

    if (pressed & BUTTON_S1) selectedGain = 0; // S1: Kp
    if (pressed & BUTTON_S2) selectedGain = 1; // S2: Ki
    if (pressed & BUTTON_S3) selectedGain = 2; // S3: Kd

    const float step = selectedGain == 2 ? 0.001f : 0.010f;
    if (pressed & BUTTON_S4) adjust_selected_gain(step);  // S4: +
    if (pressed & BUTTON_S5) adjust_selected_gain(-step); // S5: -

    if (pressed & BUTTON_S6) { // S6: SAVE
      if (settings_save(&pidSettings)) {
        pidSettingsDirty = false;
        Serial.println(F("TM1638 PID settings saved to EEPROM"));
      } else {
        Serial.println(F("TM1638 PID EEPROM save failed"));
      }
    }

    if (pressed & BUTTON_S7) { // S7: LOAD
      const bool loaded = settings_load(&pidSettings);
      apply_pid_settings();
      pidSettingsDirty = false;
      Serial.println(loaded ? F("TM1638 PID settings loaded from EEPROM")
                            : F("EEPROM invalid; compile-time PID defaults loaded"));
    }

    render_pid_screen();
  }

  void poll_tm1638() {
    const uint8_t buttons = (uint8_t)(tm1638.readButtons() & 0xFF);
    const uint8_t pressed = (uint8_t)(buttons & (uint8_t)~previousTmButtons);
    previousTmButtons = buttons;
    handle_tm_buttons(pressed);
  }
}

// AVR port นี้ใช้ WDT เป็น tick ประมาณ 15 ms; 1 tick จึงได้รอบควบคุม
// ประมาณ 66 Hz และ motor task จะวัด dt จริงจาก millis() อีกครั้ง
const TickType_t CONTROL_TASK_PERIOD = 1;
const TickType_t TELEMETRY_PERIOD = pdMS_TO_TICKS(100);  // 10 Hz

// ==================================================
// แปลงคำสั่งจาก CAN Bus (รีโมท) ไปยังการเคลื่อนที่ล้อ Mecanum
// ==================================================
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
    case SPIN_LEFT:
      motor_spin_left();
      break;
    case SPIN_RIGHT:
      motor_spin_right();
      break;
    case STOP:
    default:
      motor_stop();
      break;
  }
}

// ตรวจสอบความถูกต้องของข้อมูล Motor Status
bool is_valid_motor_status(byte value) {
  return (value <= BACKWARD_RIGHT) || (value == SPIN_LEFT) || (value == SPIN_RIGHT);
}

// ==================================================
// Serial CAN monitor
// ==================================================
// เรียกจาก CAN task เท่านั้น เพื่อไม่ให้ Serial ถูกเขียนพร้อมกันหลาย task
void serial_debug_can(const __FlashStringHelper *direction,
                      unsigned long canId,
                      byte dataLength,
                      const byte *data,
                      byte result) {
#if CAN_SERIAL_DEBUG
  const byte printableLength = dataLength > 8 ? 8 : dataLength;

  Serial.print(millis());
  Serial.print(F(" CAN "));
  Serial.print(direction);
  Serial.print(F(" id=0x"));
  Serial.print(canId, HEX);
  Serial.print(F(" dlc="));
  Serial.print(printableLength);
  Serial.print(F(" data="));

  for (byte i = 0; i < printableLength; ++i) {
    if (i > 0)
      Serial.print(' ');
    if (data[i] < 0x10)
      Serial.print('0');
    Serial.print(data[i], HEX);
  }

  Serial.print(F(" result="));
  if (result == CAN_OK) {
    Serial.println(F("CAN_OK"));
  } else {
    Serial.println(result);
  }
#else
  (void)direction;
  (void)canId;
  (void)dataLength;
  (void)data;
  (void)result;
#endif
}

byte send_can_frame(unsigned long canId, byte dataLength, byte *data) {
  byte result = CAN0.sendMsgBuf(canId, 0, dataLength, data);
  serial_debug_can(F("TX"), canId, dataLength, data, result);
  return result;
}

// ==================================================
// ประมวลผลข้อความจาก CAN Bus
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData) {
  if (dataLength < 1)
    return;

  // รับเฉพาะคำสั่งสำหรับมอเตอร์ขับเคลื่อนล้อ
  if (receivedId == CAN_ID_MOTOR) {
    byte receivedStatus = rxData[0];

    MotorCommand command;
    command.status = is_valid_motor_status(receivedStatus)
                       ? receivedStatus
                       : STOP;
    command.receivedAtMs = millis();
    command.valid = is_valid_motor_status(receivedStatus);

    // CAN task เท่านั้นที่แตะ CAN0; MotorControlTask รับคำสั่งผ่าน queue
    // frame ที่ไม่ถูกต้องจะสั่ง STOP แต่ไม่นับเป็น heartbeat
    xQueueOverwrite(motorCommandQueue, &command);
  }
}

// ==================================================
// ส่งข้อมูล Telemetry & Encoder กลับผ่าน CAN Bus และ Serial
// ==================================================
void encode_int32_be(byte *buffer, int32_t value) {
  // แปลงเป็น unsigned ก่อน shift เพื่อให้ค่าติดลบมี two's-complement
  // representation ที่ชัดเจนและไม่พึ่งพา signed right-shift ของ compiler
  const uint32_t raw = (uint32_t)value;
  buffer[0] = (byte)((raw >> 24) & 0xFF);
  buffer[1] = (byte)((raw >> 16) & 0xFF);
  buffer[2] = (byte)((raw >> 8) & 0xFF);
  buffer[3] = (byte)(raw & 0xFF);
}

void send_telemetry() {
  EncoderSnapshot snapshot;
  encoder_get_snapshot(&snapshot);

  const int8_t status = lastMotorStatus;
  const bool canAlive = motorCANAlive;

  // จัดเตรียมข้อมูลส่งกลับทาง CAN Bus (CAN_ID_TELEMETRY: 0x102)
  int16_t spd_fl = (int16_t)snapshot.speed_fl;
  int16_t spd_fr = (int16_t)snapshot.speed_fr;
  int16_t spd_bl = (int16_t)snapshot.speed_bl;
  int16_t spd_br = (int16_t)snapshot.speed_br;

  byte canTx[8];
  canTx[0] = (byte)(status >= 0 ? status : 0);
  canTx[1] = canAlive ? 1 : 0;
  canTx[2] = (byte)((spd_fl >> 8) & 0xFF);
  canTx[3] = (byte)(spd_fl & 0xFF);
  canTx[4] = (byte)((spd_fr >> 8) & 0xFF);
  canTx[5] = (byte)(spd_fr & 0xFF);
  canTx[6] = (byte)((spd_bl >> 8) & 0xFF);
  canTx[7] = (byte)(spd_bl & 0xFF);

  send_can_frame(CAN_ID_TELEMETRY, 8, canTx);

  // ส่ง ticks สะสมครบทั้ง 4 ล้อเป็นอีก 2 CAN frames
  // เพราะ CAN 1 frame มี payload ได้สูงสุด 8 bytes และ int32 2 ค่าใช้เต็มพอดี
  byte encoderFlFr[8];
  byte encoderBlBr[8];
  const int32_t ticksFl = (int32_t)snapshot.ticks_fl;
  const int32_t ticksFr = (int32_t)snapshot.ticks_fr;
  const int32_t ticksBl = (int32_t)snapshot.ticks_bl;
  const int32_t ticksBr = (int32_t)snapshot.ticks_br;

  encode_int32_be(&encoderFlFr[0], ticksFl);
  encode_int32_be(&encoderFlFr[4], ticksFr);
  encode_int32_be(&encoderBlBr[0], ticksBl);
  encode_int32_be(&encoderBlBr[4], ticksBr);

  send_can_frame(CAN_ID_ENCODER_FL_FR, 8, encoderFlFr);
  send_can_frame(CAN_ID_ENCODER_BL_BR, 8, encoderBlBr);

  Serial.print("M: ");
  switch (status) {
    case STOP:
      Serial.print("STOP      ");
      break;
    case FORWARD:
      Serial.print("FORWARD   ");
      break;
    case BACKWARD:
      Serial.print("BACKWARD  ");
      break;
    case LEFT:
      Serial.print("LEFT      ");
      break;
    case RIGHT:
      Serial.print("RIGHT     ");
      break;
    case FORWARD_LEFT:
      Serial.print("FWD_LEFT  ");
      break;
    case FORWARD_RIGHT:
      Serial.print("FWD_RIGHT ");
      break;
    case BACKWARD_LEFT:
      Serial.print("BWD_LEFT  ");
      break;
    case BACKWARD_RIGHT:
      Serial.print("BWD_RIGHT ");
      break;
    case SPIN_LEFT:
      Serial.print("SPIN_L    ");
      break;
    case SPIN_RIGHT:
      Serial.print("SPIN_R    ");
      break;
    default:
      Serial.print("NONE      ");
      break;
  }

  Serial.print(" | Enc Ticks [FL,FR,BL,BR]: ");
  Serial.print(snapshot.ticks_fl);
  Serial.print(", ");
  Serial.print(snapshot.ticks_fr);
  Serial.print(", ");
  Serial.print(snapshot.ticks_bl);
  Serial.print(", ");
  Serial.print(snapshot.ticks_br);
  Serial.print(" | Speeds: ");
  Serial.print(spd_fl);
  Serial.print(", ");
  Serial.print(spd_fr);
  Serial.print(", ");
  Serial.print(spd_bl);
  Serial.print(", ");
  Serial.println(spd_br);
}

// ==================================================
// Motor control task — priority สูงสุดของ application
// ==================================================
// Task นี้เป็นเจ้าของ motor_*(), motor_update_pid() และ safety state
// จึงไม่มี task อื่นสั่ง PWM โดยตรง
void task_motor_control(void *pvParameters) {
  (void)pvParameters;

  TickType_t lastWakeTime = xTaskGetTickCount();
  unsigned long lastControlMs = millis();
  unsigned long lastValidCommandMs = lastControlMs;
  bool hasHeartbeat = false;
  bool wasLocalPidTuneMode = false;
  MotorCommand command;

  motor_stop();

  for (;;) {
    if (localPidTuneMode) {
      // Local tuning owns the robot temporarily. Drain queued CAN commands so
      // an old movement command cannot execute when tuning mode is exited.
      motor_stop();
      while (xQueueReceive(motorCommandQueue, &command, 0) == pdPASS) {
        // discard
      }
      hasHeartbeat = false;
      motorCANAlive = false;
      lastMotorStatus = -1;
      wasLocalPidTuneMode = true;
      vTaskDelayUntil(&lastWakeTime, CONTROL_TASK_PERIOD);
      continue;
    }

    if (wasLocalPidTuneMode) {
      // Require a new valid CAN heartbeat after local tuning; never resume an
      // old movement command just because the panel was closed.
      motor_stop();
      hasHeartbeat = false;
      motorCANAlive = false;
      lastMotorStatus = -1;
      wasLocalPidTuneMode = false;
    }

    while (xQueueReceive(motorCommandQueue, &command, 0) == pdPASS) {
      if (command.valid) {
        lastValidCommandMs = command.receivedAtMs;
        hasHeartbeat = true;

        if (command.status != (uint8_t)lastMotorStatus) {
          lastMotorStatus = (int8_t)command.status;
          apply_motor_from_status((PS2_Status)command.status);
        }
        motorCANAlive = true;
      } else {
        // Frame ไม่ถูกต้อง: หยุด แต่ไม่ feed watchdog
        lastMotorStatus = STOP;
        apply_motor_from_status(STOP);
      }
    }

    const unsigned long nowMs = millis();
    if (hasHeartbeat && (nowMs - lastValidCommandMs > CAN_TIMEOUT)) {
      hasHeartbeat = false;
      motorCANAlive = false;
      lastMotorStatus = -1;
      motor_stop();
    }

    unsigned long elapsedMs = nowMs - lastControlMs;
    lastControlMs = nowMs;
    float dt = (float)elapsedMs / 1000.0f;
    if (dt <= 0.0f)
      dt = 0.001f;

    encoder_update_speeds(dt);
    motor_update_pid(dt);

    vTaskDelayUntil(&lastWakeTime, CONTROL_TASK_PERIOD);
  }
}

// ==================================================
// CAN task — เป็นเจ้าของ MCP2515 เพียง task เดียว
// ==================================================
void task_can(void *pvParameters) {
  (void)pvParameters;

  TickType_t lastTelemetryTime = xTaskGetTickCount();

  for (;;) {
    if (canReady) {
      // Drain CAN RX buffer ทุก frame ที่รออยู่
      while (CAN0.checkReceive() == CAN_MSGAVAIL) {
        unsigned long receivedId = 0;
        byte dataLength = 0;
        byte rxData[8];

        byte result = CAN0.readMsgBuf(&receivedId, &dataLength, rxData);
        serial_debug_can(F("RX"), receivedId, dataLength, rxData, result);
        if (result == CAN_OK) {
          process_can_message(receivedId, dataLength, rxData);
        }
      }
    }

    TickType_t now = xTaskGetTickCount();
    if (canReady && (TickType_t)(now - lastTelemetryTime) >= TELEMETRY_PERIOD) {
      lastTelemetryTime += TELEMETRY_PERIOD;
      send_telemetry();
    }

    // TM1638 is local-only; no CAN frame is needed for PID configuration.
    poll_tm1638();

    // Arduino_FreeRTOS บน AVR มี tick ประมาณ 15 ms
    // ห้ามใช้ pdMS_TO_TICKS(1) เพราะจะปัดลงเป็น 0 และไม่ yield
    vTaskDelay(1);
  }
}

// ==================================================
// Setup
// ==================================================
void setup() {
  Serial.begin(115200);

  // Load persistent PID values before the control task starts.
  tm1638.begin(2);
  const bool pidLoaded = settings_load(&pidSettings);
  apply_pid_settings();
  render_pid_screen();
  Serial.println(pidLoaded ? F("PID settings loaded from EEPROM")
                           : F("EEPROM invalid; using default PID settings"));

  // 1. เริ่มต้นระบบมอเตอร์ขับเคลื่อนล้อ
  motor_init();

  // 2. เริ่มต้นระบบตัวนับพัลส์ Encoder (Hardware Interrupts)
  encoder_init();

  // 3. เริ่มต้นระบบ MCP2515 CAN Bus
  Serial.println("Initializing MCP2515 (Motor & Encoder Controller)...");

  // --- บังคับเคลียร์และกำหนดขา SPI ของ Mega 2560 อย่างเด็ดขาด ---
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);
  // แม้จะใช้ CS=10 แต่ AVR SPI master ต้องให้ hardware SS (53) เป็น OUTPUT
  // ไม่เช่นนั้นถ้าขานี้ลอย/ถูกดึง LOW จะทำให้ SPI หลุดจากโหมด master ได้
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  pinMode(51, OUTPUT);        // MOSI (SI)
  pinMode(52, OUTPUT);        // SCK (SCK)
  pinMode(50, INPUT_PULLUP);  // MISO (SO) - ดึงลอจิกขึ้นกันสัญญาณลอย

  SPI.begin();
  // --------------------------------------------------------

  // Do not block forever when CAN is disconnected: local TM1638 PID setup
  // must remain usable on the bench without any bus/controller attached.
  for (uint8_t attempt = 0; attempt < 5 && !canReady; ++attempt) {
    byte canInitResult = CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK_SET);
    if (canInitResult == CAN_OK) {
      canReady = true;
      break;
    }

    Serial.print(F("MCP2515 initialization failed, code="));
    Serial.println(canInitResult);
    delay(200);
  }
  if (canReady) {
    CAN0.setMode(MCP_NORMAL);
    Serial.println(F("MCP2515 Ready. Mecanum Closed-Loop Motor Controller active."));
  } else {
    Serial.println(F("MCP2515 offline. TM1638 local PID setup remains available."));
  }

  motor_stop();

  motorCommandQueue = xQueueCreate(1, sizeof(MotorCommand));
  if (motorCommandQueue == nullptr) {
    // สร้าง queue ไม่สำเร็จ: หยุดระบบไว้ใน safe state
    while (true) {
      motor_stop();
      delay(100);
    }
  }

  // Mega 2560 มี RAM จำกัด จึงไม่สร้าง DebugTask แยก
  // Serial debug ถูกทำใน CAN task ตอนส่ง telemetry
  BaseType_t motorTaskResult = xTaskCreate(
    task_motor_control,
    "MotorCtrl",
    256,
    nullptr,
    3,
    &motorControlTaskHandle);

  BaseType_t canTaskResult = xTaskCreate(
    task_can,
    "CAN",
    256,
    nullptr,
    2,
    &canTaskHandle);

  if (motorTaskResult != pdPASS || canTaskResult != pdPASS) {
    // Scheduler ยังไม่เริ่มจนกว่า setup() จะจบ จึงหยุดได้อย่างปลอดภัยตรงนี้
    motor_stop();
    while (true) {
      delay(100);
    }
  }
}

// ==================================================
// Main loop
// ==================================================
void loop() {
  // FreeRTOS tasks ทำงานทั้งหมด; loop() ถูกเรียกจาก idle hook เท่านั้น
}
