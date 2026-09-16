/*
 * Alternative FreeRTOS version of motor_controller.ino
 *
 * Target board: Arduino Mega 2560
 * Required library: Arduino_FreeRTOS_Library
 *
 * This sketch is kept in its own directory so it can be compiled separately
 * from the legacy motor_controller sketch. The Arduino builder compiles every
 * .ino file in a sketch directory.
 */

// Arduino_FreeRTOS.h must be included before the other FreeRTOS/Arduino headers.
#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

#include <SPI.h>
#include <mcp_can.h>

#include "robot_config.h"
#include "encoder.h"
#include "motor.h"
#include "pid_config.h"
#include "tm1638.h"

// ---------------- CAN configuration ----------------
MCP_CAN CAN0(CAN_CS_PIN);

// ---------------- สถานะคำสั่งมอเตอร์ ----------------
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

// ---------------- FreeRTOS shared resources ----------------
// motorStateMutex protects target speeds, PID state, command status and the
// encoder speed snapshot. canBusMutex serializes MCP2515 SPI access.
static SemaphoreHandle_t motorStateMutex = nullptr;
static SemaphoreHandle_t canBusMutex = nullptr;
static QueueHandle_t pidConfigQueue = nullptr;

static unsigned long lastMotorMessageTime = 0;
static bool motorCANAlive = false;
static int lastMotorStatus = -1;

// ---------------- Periodic task timing ----------------
constexpr uint16_t CONTROL_LOOP_INTERVAL_MS = 20;  // 50 Hz
constexpr uint16_t TELEMETRY_INTERVAL_MS = 100;    // 10 Hz

static const TickType_t CONTROL_LOOP_PERIOD = pdMS_TO_TICKS(CONTROL_LOOP_INTERVAL_MS);
static const TickType_t TELEMETRY_PERIOD = pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS);

// Arduino Mega has limited SRAM. Stack depth is measured in words by the
// Arduino_FreeRTOS AVR port, so keep task-local buffers small.
constexpr uint16_t CAN_TASK_STACK = 256;
constexpr uint16_t CONTROL_TASK_STACK = 192;
constexpr uint16_t TELEMETRY_TASK_STACK = 320;
constexpr uint16_t PID_UI_TASK_STACK = 256;

constexpr uint16_t PID_UI_INTERVAL_MS = 50;
constexpr uint16_t PID_BUTTON_REPEAT_MS = 250;
static const TickType_t PID_UI_PERIOD = pdMS_TO_TICKS(PID_UI_INTERVAL_MS);

constexpr uint8_t BUTTON_SELECT_KP = 0x01;
constexpr uint8_t BUTTON_SELECT_KI = 0x02;
constexpr uint8_t BUTTON_SELECT_KD = 0x04;
constexpr uint8_t BUTTON_DECREASE = 0x08;
constexpr uint8_t BUTTON_INCREASE = 0x10;
constexpr uint8_t BUTTON_STEP = 0x20;
constexpr uint8_t BUTTON_DEFAULTS = 0x40;
constexpr uint8_t BUTTON_SAVE = 0x80;

const float PID_STEPS[] = {0.001f, 0.01f, 0.1f};

static PidConfig activePidConfig;

// ---------------- Forward declarations ----------------
void task_can_receive(void *parameter);
void task_motor_control(void *parameter);
void task_telemetry(void *parameter);
void task_pid_ui(void *parameter);

static char selected_parameter_name(uint8_t parameterIndex) {
  switch (parameterIndex) {
    case 0: return 'P';
    case 1: return 'I';
    default: return 'D';
  }
}

static float *selected_parameter_value(PidConfig &config, uint8_t parameterIndex) {
  switch (parameterIndex) {
    case 0: return &config.kp;
    case 1: return &config.ki;
    default: return &config.kd;
  }
}

static void publish_pid_config(const PidConfig &config) {
  if (pidConfigQueue != nullptr) {
    // Queue length is one, so the latest UI value always replaces the old one.
    xQueueOverwrite(pidConfigQueue, &config);
  }
}

// ==================================================
// แปลงคำสั่งจาก CAN Bus ไปยังการเคลื่อนที่ล้อ Mecanum
// เรียกใช้ภายใต้ motorStateMutex เท่านั้น
// ==================================================
void apply_motor_from_status(PS2_Status current) {
  switch (current) {
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

bool is_valid_motor_status(byte value) {
  return (value <= BACKWARD_RIGHT) || (value == SPIN_LEFT) || (value == SPIN_RIGHT);
}

// ==================================================
// ประมวลผลข้อความจาก CAN Bus
// ==================================================
void process_can_message(unsigned long receivedId, byte dataLength, byte *rxData) {
  if (dataLength < 1 || receivedId != CAN_ID_MOTOR) return;

  const byte receivedStatus = rxData[0];

  if (!is_valid_motor_status(receivedStatus)) {
    if (xSemaphoreTake(motorStateMutex, portMAX_DELAY) == pdTRUE) {
      motor_stop();
      xSemaphoreGive(motorStateMutex);
    }
    return;
  }

  if (xSemaphoreTake(motorStateMutex, portMAX_DELAY) == pdTRUE) {
    // Feed the watchdog for every valid command, including repeated commands.
    lastMotorMessageTime = millis();
    motorCANAlive = true;

    if (receivedStatus != lastMotorStatus) {
      lastMotorStatus = receivedStatus;
      apply_motor_from_status((PS2_Status)receivedStatus);
    }

    xSemaphoreGive(motorStateMutex);
  }
}

// ==================================================
// ถ่าย snapshot และส่งข้อมูล Telemetry
// ==================================================
void send_telemetry() {
  int16_t spd_fl;
  int16_t spd_fr;
  int16_t spd_bl;
  int16_t spd_br;
  long ticks_fl;
  long ticks_fr;
  long ticks_bl;
  long ticks_br;
  int status;
  bool alive;

  // Capture all shared values together so a telemetry line is internally
  // consistent even while the control task is updating PID/encoder speeds.
  if (xSemaphoreTake(motorStateMutex, portMAX_DELAY) != pdTRUE) return;

  status = lastMotorStatus;
  alive = motorCANAlive;
  spd_fl = (int16_t)encoder_get_speed_fl();
  spd_fr = (int16_t)encoder_get_speed_fr();
  spd_bl = (int16_t)encoder_get_speed_bl();
  spd_br = (int16_t)encoder_get_speed_br();
  ticks_fl = encoder_get_ticks_fl();
  ticks_fr = encoder_get_ticks_fr();
  ticks_bl = encoder_get_ticks_bl();
  ticks_br = encoder_get_ticks_br();

  xSemaphoreGive(motorStateMutex);

  byte canTx[8];
  canTx[0] = (byte)(status >= 0 ? status : 0);
  canTx[1] = alive ? 1 : 0;
  canTx[2] = (byte)((spd_fl >> 8) & 0xFF);
  canTx[3] = (byte)(spd_fl & 0xFF);
  canTx[4] = (byte)((spd_fr >> 8) & 0xFF);
  canTx[5] = (byte)(spd_fr & 0xFF);
  canTx[6] = (byte)((spd_bl >> 8) & 0xFF);
  canTx[7] = (byte)(spd_bl & 0xFF);

  if (xSemaphoreTake(canBusMutex, portMAX_DELAY) == pdTRUE) {
    CAN0.sendMsgBuf(CAN_ID_TELEMETRY, 0, 8, canTx);
    xSemaphoreGive(canBusMutex);
  }

  Serial.print("M: ");
  switch (status) {
    case STOP: Serial.print("STOP      "); break;
    case FORWARD: Serial.print("FORWARD   "); break;
    case BACKWARD: Serial.print("BACKWARD  "); break;
    case LEFT: Serial.print("LEFT      "); break;
    case RIGHT: Serial.print("RIGHT     "); break;
    case FORWARD_LEFT: Serial.print("FWD_LEFT  "); break;
    case FORWARD_RIGHT: Serial.print("FWD_RIGHT "); break;
    case BACKWARD_LEFT: Serial.print("BWD_LEFT  "); break;
    case BACKWARD_RIGHT: Serial.print("BWD_RIGHT "); break;
    case SPIN_LEFT: Serial.print("SPIN_L    "); break;
    case SPIN_RIGHT: Serial.print("SPIN_R    "); break;
    default: Serial.print("NONE      "); break;
  }

  Serial.print(" | Enc Ticks [FL,FR,BL,BR]: ");
  Serial.print(ticks_fl);
  Serial.print(", ");
  Serial.print(ticks_fr);
  Serial.print(", ");
  Serial.print(ticks_bl);
  Serial.print(", ");
  Serial.print(ticks_br);
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
// Task: รับข้อความ CAN
// ==================================================
void task_can_receive(void *parameter) {
  (void)parameter;

  for (;;) {
    // Drain all pending frames, then yield so the control task gets its
    // deadline even if CAN traffic is busy.
    for (;;) {
      unsigned long receivedId = 0;
      byte dataLength = 0;
      byte rxData[8];
      byte result = 0;
      bool frameRead = false;

      if (xSemaphoreTake(canBusMutex, portMAX_DELAY) == pdTRUE) {
        if (CAN0.checkReceive() == CAN_MSGAVAIL) {
          result = CAN0.readMsgBuf(&receivedId, &dataLength, rxData);
          frameRead = true;
        }
        xSemaphoreGive(canBusMutex);
      }

      if (!frameRead) break;
      if (result == CAN_OK) {
        process_can_message(receivedId, dataLength, rxData);
      }
    }

    // One tick is the minimum reliable yield on the AVR port. With the
    // default watchdog tick this is approximately 15-16 ms.
    vTaskDelay(1);
  }
}

// ==================================================
// Task: Encoder speed + PID + fail-safe watchdog
// ==================================================
void task_motor_control(void *parameter) {
  (void)parameter;

  TickType_t lastWakeTime = xTaskGetTickCount();
  unsigned long previousMs = millis();
  PidConfig requestedPidConfig;

  for (;;) {
    vTaskDelayUntil(&lastWakeTime, CONTROL_LOOP_PERIOD);

    const unsigned long currentMs = millis();
    unsigned long elapsedMs = currentMs - previousMs;
    previousMs = currentMs;

    // Avoid a zero dt if millis() and the RTOS tick have the same timestamp.
    if (elapsedMs == 0) elapsedMs = CONTROL_LOOP_INTERVAL_MS;
    const float dt = (float)elapsedMs / 1000.0f;

    if (xSemaphoreTake(motorStateMutex, portMAX_DELAY) == pdTRUE) {
      // The control task is the only task that writes the live PID objects.
      if (xQueueReceive(pidConfigQueue, &requestedPidConfig, 0) == pdPASS) {
        pid_config_clamp(requestedPidConfig);
        motor_set_pid_config(requestedPidConfig);
      }

      encoder_update_speeds(dt);

      if (motorCANAlive && (currentMs - lastMotorMessageTime > CAN_TIMEOUT)) {
        motorCANAlive = false;
        lastMotorStatus = -1;
        motor_stop();
        Serial.println("WARNING: Motor CAN timeout - Fail-safe Stop");
      } else {
        motor_update_pid(dt);
      }

      xSemaphoreGive(motorStateMutex);
    }
  }
}

// ==================================================
// Task: ส่ง Telemetry ผ่าน CAN และ Serial
// ==================================================
void task_telemetry(void *parameter) {
  (void)parameter;

  TickType_t lastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTime, TELEMETRY_PERIOD);
    send_telemetry();
  }
}

// ==================================================
// Task: อ่านปุ่ม TM1638 และแก้ค่า PID
// ==================================================
void task_pid_ui(void *parameter) {
  (void)parameter;

  TickType_t lastWakeTime = xTaskGetTickCount();
  uint8_t previousButtons = 0;
  uint8_t selectedParameter = 0; // 0=Kp, 1=Ki, 2=Kd
  uint8_t stepIndex = 1;         // default step = 0.01
  bool dirty = false;
  unsigned long lastButtonRepeatMs = 0;

  for (;;) {
    vTaskDelayUntil(&lastWakeTime, PID_UI_PERIOD);

    const uint8_t buttons = tm1638_read_buttons();
    const uint8_t pressed = (uint8_t)(buttons & (uint8_t)~previousButtons);
    previousButtons = buttons;
    const unsigned long now = millis();
    bool displayChanged = false;
    bool configChanged = false;

    if (pressed & BUTTON_SELECT_KP) {
      selectedParameter = 0;
      displayChanged = true;
    } else if (pressed & BUTTON_SELECT_KI) {
      selectedParameter = 1;
      displayChanged = true;
    } else if (pressed & BUTTON_SELECT_KD) {
      selectedParameter = 2;
      displayChanged = true;
    }

    if (pressed & BUTTON_STEP) {
      stepIndex = (uint8_t)((stepIndex + 1) % 3);
      displayChanged = true;
    }

    if (pressed & BUTTON_DEFAULTS) {
      activePidConfig = pid_config_defaults();
      dirty = true;
      configChanged = true;
      displayChanged = true;
    }

    if (pressed & BUTTON_SAVE) {
      if (pid_config_save(activePidConfig)) {
        dirty = false;
      }
      displayChanged = true;
    }

    const bool increaseHeld = (buttons & BUTTON_INCREASE) != 0;
    const bool decreaseHeld = (buttons & BUTTON_DECREASE) != 0;
    const bool increasePressed = (pressed & BUTTON_INCREASE) != 0;
    const bool decreasePressed = (pressed & BUTTON_DECREASE) != 0;
    const bool repeatReady = (now - lastButtonRepeatMs) >= PID_BUTTON_REPEAT_MS;

    int8_t direction = 0;
    if (increaseHeld && !decreaseHeld && (increasePressed || repeatReady)) {
      direction = 1;
    } else if (decreaseHeld && !increaseHeld && (decreasePressed || repeatReady)) {
      direction = -1;
    }

    if (direction != 0) {
      float *value = selected_parameter_value(activePidConfig, selectedParameter);
      *value += direction * PID_STEPS[stepIndex];
      pid_config_clamp(activePidConfig);
      dirty = true;
      configChanged = true;
      displayChanged = true;
      lastButtonRepeatMs = now;
    }

    if (configChanged) {
      publish_pid_config(activePidConfig);
    }

    if (displayChanged) {
      tm1638_display_pid(
          selected_parameter_name(selectedParameter),
          *selected_parameter_value(activePidConfig, selectedParameter),
          stepIndex,
          dirty);
    }
  }
}

// ==================================================
// Setup
// ==================================================
void setup() {
  Serial.begin(115200);

  motor_init();
  encoder_init();

  Serial.println("Initializing MCP2515 (Motor & Encoder Controller, FreeRTOS)...");

  // MCP2515 SPI pins on Arduino Mega 2560.
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);
  pinMode(51, OUTPUT);        // MOSI (SI)
  pinMode(52, OUTPUT);        // SCK (SCK)
  pinMode(50, INPUT_PULLUP);  // MISO (SO)

  SPI.begin();

  while (CAN0.begin(MCP_ANY, CAN_500KBPS, CAN_CLOCK_SET) != CAN_OK) {
    Serial.println("MCP2515 initialization failed. Retrying...");
    delay(1000);
  }
  CAN0.setMode(MCP_NORMAL);

  Serial.println("MCP2515 Ready. FreeRTOS Mecanum Motor Controller active.");

  tm1638_begin();
  pid_config_load(activePidConfig);
  tm1638_display_pid('P', activePidConfig.kp, 1, false);

  motorStateMutex = xSemaphoreCreateMutex();
  canBusMutex = xSemaphoreCreateMutex();
  pidConfigQueue = xQueueCreate(1, sizeof(PidConfig));

  if (motorStateMutex == nullptr || canBusMutex == nullptr || pidConfigQueue == nullptr) {
    Serial.println("ERROR: FreeRTOS resource creation failed. Motors stopped.");
    motor_stop();
    for (;;) delay(1000);
  }

  motor_stop();
  publish_pid_config(activePidConfig);

  const BaseType_t canTaskResult = xTaskCreate(
    task_can_receive,
    "CAN_RX",
    CAN_TASK_STACK,
    nullptr,
    2,
    nullptr);

  const BaseType_t controlTaskResult = xTaskCreate(
    task_motor_control,
    "MOTOR_CTL",
    CONTROL_TASK_STACK,
    nullptr,
    3,
    nullptr);

  const BaseType_t telemetryTaskResult = xTaskCreate(
      task_telemetry,
      "TELEMETRY",
      TELEMETRY_TASK_STACK,
      nullptr,
      1,
      nullptr);

  const BaseType_t pidUiTaskResult = xTaskCreate(
      task_pid_ui,
      "PID_UI",
      PID_UI_TASK_STACK,
      nullptr,
      1,
      nullptr);

  if (canTaskResult != pdPASS || controlTaskResult != pdPASS ||
      telemetryTaskResult != pdPASS || pidUiTaskResult != pdPASS) {
    Serial.println("ERROR: FreeRTOS task creation failed. Motors stopped.");
    motor_stop();
  }
}

// FreeRTOS tasks own all periodic work. Keep Arduino's loop task idle.
void loop() {}
